#include "server.h"
#include "../store/helpers.h"

#include <sys/socket.h>

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
