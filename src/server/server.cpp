#include "server.h"
#include "../store/helpers.h"

#include <sys/socket.h>
#include <fstream>
#include <cstdio>
#include <unistd.h>

std::string ServerState::get_role() {
    return role;
}

std::string ServerState::get_redis_version() {
    return redis_version;
}

int ServerState::get_port() {
    return port;
}

int ServerState::get_connected_clients() {
    return connected_clients;
}

long long ServerState::get_used_memory() {
    return used_memory;
}

void ServerState::set_role(std::string role) {
    this->role = role;
}

void ServerState::add_connected_client() {
    connected_clients++;
}

std::string ServerState::get_master_replid() {
    return master_replid;
}

long long ServerState::get_master_repl_offset() {
    return master_repl_offset;
}

void ServerState::add_connected_replica(int client_fd) {
    replica_fds.insert({client_fd, 0});
}

std::vector<int> ServerState::get_connected_replicas() {
    std::vector<int> fds;
    for (const auto &[fd, offset] : replica_fds) {
        fds.push_back(fd);
    }
    return fds;
}

void ServerState::add_offset(int bytes) {
    master_repl_offset += bytes;
}

int ServerState::get_num_connected_replicas() {
    return replica_fds.size();
}

int ServerState::get_num_connected_replicas_with_offset(long long offset) {
    int count = 0;
    for (const auto &[_, fd_offset] : replica_fds) {
        if (fd_offset >= offset) { count++; }
    }
    return count;
}

void ServerState::request_ack_from_replicas() {
    std::string get_ack = encode_resp_array({"REPLCONF", "GETACK", "*"});
    for (const auto &[fd, _]: replica_fds) {
        send(fd, get_ack.data(), get_ack.length(), 0);
    }
}

void ServerState::update_replica_offset(int fd, long long offset) {
    replica_fds[fd] = offset;
}

void ServerState::set_config_file_param(std::string param, std::string value) {
    if (param == "--dir") {
        config_file.dir = value;
        return;
    }
    if (param == "--dbfilename") {
        config_file.db_filename = value;
        return;
    }
    if (param == "--appendonly") {
        config_file.appendonly = value;
        return;
    }
    if (param == "--appenddirname") {
        config_file.appenddirname = value;
        return;
    }
    if (param == "--appendfilename") {
        config_file.appendfilename = value;
        return;
    }
    if (param == "--appendfsync") {
        config_file.appendfsync = value;
        return;
    }
}

std::string ServerState::get_config_file_param (std::string param) {
    if (param == "dir") return config_file.dir;
    if (param == "dbfilename") return config_file.db_filename;
    if (param == "appendonly") return config_file.appendonly;
    if (param == "appenddirname") return config_file.appenddirname;
    if (param == "appendfilename") return config_file.appendfilename;
    if (param == "appendfsync") return config_file.appendfsync;
    return "";
}

void ServerState::handle_append_only() {
    // 1 - create the dir if it doesn't exist
    std::filesystem::path dir_path = std::filesystem::path(config_file.dir) / config_file.appenddirname;
    std::filesystem::create_directories(dir_path);

    // 2 - create AOF file inside the dir
    config_file.active_aof_path = dir_path / (config_file.appendfilename + ".1.incr.aof");
    std::ofstream aof_file(config_file.active_aof_path, std::ios::app);

    // 3 - create manifest file inside the dir
    std::filesystem::path manifest_file_path = dir_path / (config_file.appendfilename + ".manifest");
    std::ofstream manifest_file(manifest_file_path);
    manifest_file << "file " + config_file.active_aof_path.filename().string() + " seq 1 type i\n";
}

void ServerState::write_to_append_only_file(const std::string &raw_command) {
    // 1 - open the file in append mode
    FILE *aof_file = fopen(config_file.active_aof_path.c_str(), "a");
    if (aof_file != nullptr) {
        // 2 - write the new command to the file
        fwrite(raw_command.c_str(), 1, raw_command.length(), aof_file);

        // 3 - flush if needed
        if (is_append_fsync_always()) {
            fflush(aof_file);
            fsync(fileno(aof_file));
        }
    }
    fclose(aof_file);
}