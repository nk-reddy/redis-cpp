#include "commands.h"
#include "../store/store.h"
#include "../store/helpers.h"

#include <string>
#include <vector> 
#include <algorithm>
#include <unordered_set>
#include <sys/socket.h>
#include <mutex>
#include <condition_variable>
#include <chrono>

std::string handle_command(const std::string &command, const std::vector<std::string> &data, Store &store, ServerState &server, const std::string &raw_command, int client_fd) {
    // handle the command
    std::string response; 
    if (command == "echo") {
        response = handle_command_echo(data);
    }
    else if (command == "set") {
        response = handle_command_set(data, store);
    }
    else if (command == "get") {
        response = handle_command_get(data, store);
    }
    else if (command == "rpush") {
        response = handle_command_rpush(data, store);
    }
    else if (command == "lrange") {
        response = handle_command_lrange(data, store);
    }
    else if (command == "lpush") {
        response = handle_command_lpush(data, store);
    }
    else if (command == "llen") {
        response = handle_command_llen(data, store);
    }
    else if (command == "lpop") {
        response = handle_command_lpop(data, store);
    }
    else if (command == "blpop") {
        response = handle_command_blpop(data, store);
    }
    else if (command == "type") {
        response = handle_command_type(data, store);
    }
    else if (command == "xadd") {
        response = handle_command_xadd(data, store);
    }
    else if (command == "xrange") {
        response = handle_command_xrange(data, store);
    }
    else if (command == "xread") {
        response = handle_command_xread(data, store);
    }
    else if (command == "incr") {
        response = handle_command_incr(data, store);
    }
    else if (command == "info") {
        response = handle_command_info(data, server);
    }
    else if (command == "replconf") {
        response = handle_command_replconf(data, server, client_fd);
    }
    else if (command == "wait") {
        response = handle_command_wait(data, server);
    }
    else {
        response = handle_command_default();
    }

    // propagate the command to any replicas
    if (modifying_command(command) && server.get_role() == "master") {
        propagate_command_to_replicas(raw_command, server);
        server.add_offset(raw_command.length());
        server.set_writes_since_last_wait(true);
    }

    return response;
}

bool modifying_command(const std::string &command) {
    static const std::unordered_set<std::string> write_commands = {
        "set",
        "incr",
        "rpush",
        "lpush",
        "lpop",
        "blpop",
        "xadd"
    };

    return write_commands.contains(command);
}

std::string handle_command_default() {
    return "+PONG\r\n";
}

std::string handle_command_echo(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return "-ERR invalid arguments\r\n";
    }
    return "$" + std::to_string(args[1].length()) + "\r\n" + args[1] + "\r\n";
}

std::string handle_command_set(const std::vector<std::string>& args, Store &store) {
    if (args.size() < 3) {
        return "-ERR invalid arguments\r\n";
    }
    if (args.size() == 3) {
        store.set(args[1], args[2]);
        return "+OK\r\n";
    }
    if (args.size() != 5) {
        return "-ERR invalid arguments\r\n";
    }

    std::string option = args[3];
    std::transform(option.begin(), option.end(), option.begin(), ::tolower);
    if (option != "ex" && option != "px") {
        return "-ERR invalid arguments\r\n";
    }

    int ms = std::stoi(args[4]);
    if (option == "ex") {ms = std::stoi(args[4]) * 1000;}

    store.set_with_expiry(args[1], args[2], ms);
    return "+OK\r\n";
}

std::string handle_command_get(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 2) {
        return "-ERR invalid arguments\r\n";
    }
    return store.get(args[1]);
}

std::string handle_command_rpush(const std::vector<std::string>& args, Store &store) {
    if (args.size() < 3) {
        return "-ERR invalid arguments\r\n";
    }
    std::vector<std::string> values(args.begin() + 2, args.end());
    return store.rpush(args[1], values);
}

std::string handle_command_lrange(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 4) {
        return "-ERR invalid arguments\r\n";
    }
    return store.lrange(args[1], args[2], args[3]);
}

std::string handle_command_lpush(const std::vector<std::string>& args, Store &store) {
    if (args.size() < 3) {
        return "-ERR invalid arguments\r\n";
    }
    std::vector<std::string> values(args.begin() + 2, args.end());
    return store.lpush(args[1], values);
}

std::string handle_command_llen(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 2) {
        return "-ERR invalid arguments\r\n";
    }
    return store.llen(args[1]);  
}

std::string handle_command_lpop(const std::vector<std::string>& args, Store &store) {
    if (args.size() == 2) {
        return store.lpop(args[1], -1); 
    }
    if (args.size() == 3) {
        return store.lpop(args[1], std::stoi(args[2])); 
    }
    return "-ERR invalid arguments\r\n";  
}

std::string handle_command_blpop(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 3) {
        return "-ERR invalid arguments\r\n";
    }
    return store.blpop(args[1], std::stod(args[2]));   
}

std::string handle_command_type(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 2) {
        return "-ERR invalid arguments\r\n";
    }
    return store.type(args[1]);    
}

std::string handle_command_xadd(const std::vector<std::string>& args, Store &store) {
    if (args.size() < 5 || args.size() % 2 == 0) {
        return "-ERR invalid arguments\r\n";
    }
    std::vector<std::string> values(args.begin() + 3, args.end());
    return store.xadd(args[1], args[2], values);
}

std::string handle_command_xrange(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 4) {
        return "-ERR invalid arguments\r\n";
    }
    return store.xrange(args[1], args[2], args[3]); 
}

std::string handle_command_xread(const std::vector<std::string>& args, Store &store) {
    if (args.size() < 4 || args.size() % 2 != 0) {
        return "-ERR invalid arguments\r\n";
    }

    std::string type = args[1];
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    if (type != "streams" && type != "block") {
        return "-ERR invalid arguments\r\n";
    }

    std::vector<std::string> keys {};
    std::vector<std::string> ids {};

    if (type == "streams") {
        for (size_t i = 2; i < args.size(); ++i) {
            size_t mid = args.size() / 2;
            if (i <= mid) { keys.push_back(args[i]); }
            else { ids.push_back(args[i]); }
        }
        return store.xread(keys, ids);
    }

    // blocking scenario
    std::string check = args[3];
    std::transform(check.begin(), check.end(), check.begin(), ::tolower);
    if (check != "streams") {
        return "-ERR invalid arguments\r\n";
    }
    for (size_t i = 4; i < args.size(); ++i) {
        size_t mid = args.size() / 2 + 1;
        if (i <= mid) { keys.push_back(args[i]); }
        else { ids.push_back(args[i]); }
    }

    return store.xread(keys, ids, std::stod(args[2]));
}

std::string handle_command_incr(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 2) {
        return "-ERR invalid arguments\r\n";
    }
    return store.incr(args[1]);  
}

std::string handle_command_info(const std::vector<std::string>& args, ServerState &server) {
    bool req_all = false, req_server = false, req_clients = false, req_memory = false, req_replication = false;
    if (args.size() > 2) { return "-ERR invalid arguments\r\n"; }
    else if (args.size() == 1) { req_all = true; }
    else {
        for (size_t i = 1; i < args.size(); ++i) {
            std::string obj_to_req = args[i];
            std::transform (obj_to_req.begin(), obj_to_req.end(), obj_to_req.begin(), ::tolower);

            if (obj_to_req == "server") { req_server = true; }
            else if (obj_to_req == "clients") { req_clients = true; }
            else if (obj_to_req == "memory") { req_memory = true; }
            else if (obj_to_req == "replication") { req_replication = true; }
        }
    }

    std::string response = "";
    if (req_all || req_server) {
        response += "# Server\r\n";
        response += "redis_version:" + server.get_redis_version();
        response += "\r\n";
    }
    if (req_all || req_clients) {
        response += "# Clients\r\n";
        response += "connected_clients:" + std::to_string(server.get_connected_clients());
        response += "\r\n";
    }
    if (req_all || req_memory) {
        response += "# Memory\r\n";
        response += "used_memory:" + std::to_string(server.get_used_memory());
        response += "\r\n";
    }
    if (req_all || req_replication) {
        response += "# Replication\r\n";
        response += "role:" + server.get_role() + "\r\n";
        response += "master_repl_offset:" + std::to_string(server.get_master_repl_offset()) + "\r\n";
        response += "master_replid:" + server.get_master_replid();
    }

    return encode_resp_string(response);
}

std::string handle_command_replconf(const std::vector<std::string>& args, ServerState &server, int client_fd) {
    if (args.size() >= 3) {
        std::string type = args[1];
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        if (type == "getack")
        {
            std::vector<std::string> ack = {"REPLCONF", "ACK", std::to_string(server.get_master_repl_offset())};
            return encode_resp_array(ack);
        }
        if (type == "ack")
        {
            long long offset = std::stoll(args[2]);
            server.update_replica_offset(client_fd, offset);
            server.get_replica_cv().notify_all();
            return "";
        }
    }
    return "+OK\r\n";
}

void handle_command_psync(int client_fd, const std::vector<std::string>& args, ServerState &server) {
    std::string response = "";
    if (args.size() < 3) { 
        response = "-ERR invalid arguments\r\n"; 
        send(client_fd, response.data(), response.length(), 0);
        return;
    }

    // replica is connecting for the first time
    if (args[1] == "?" && args[2] == "-1") {
        response = "+FULLRESYNC " + server.get_master_replid() + " 0\r\n";
        send(client_fd, response.data(), response.length(), 0);

        // send the empty RDB file
        std::string empty_rdb_hex = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
        std::string empty_rdb_binary = parse_hex_to_binary(empty_rdb_hex);
        std::string rdb_response = "$" + std::to_string(empty_rdb_binary.length()) + "\r\n" + empty_rdb_binary;
        send(client_fd, rdb_response.data(), rdb_response.length(), 0);

        // update our internal state of replicas
        server.add_connected_replica(client_fd);
        server.get_replica_cv().notify_all();
        return;
    }

    send(client_fd, response.data(), response.length(), 0);
}

void propagate_command_to_replicas(const std::string &rawCommand, ServerState &server) {
    for (int replica_fd : server.get_connected_replicas()) {
        send(replica_fd, rawCommand.data(), rawCommand.length(), 0);
    }
}

std::string handle_command_wait(const std::vector<std::string>& args, ServerState &server) {
    if (args.size() != 3) { return "-ERR invalid arguments\r\n"; }
    int min_replicas = std::stoi(args[1]);
    int timeout = std::stoi(args[2]);
    long long required_offset = server.get_master_repl_offset();

    if (server.get_writes_since_last_wait()) {
        server.request_ack_from_replicas();
        server.set_writes_since_last_wait(false);
    }
    
    std::unique_lock<std::mutex> lock(server.get_replica_mutex());
    server.get_replica_cv().wait_for(
        lock,
        std::chrono::milliseconds(timeout),
        [&]() {
            return server.get_num_connected_replicas_with_offset(required_offset) >= min_replicas;
        }
    );

    return ":" + std::to_string(server.get_num_connected_replicas_with_offset(required_offset)) + "\r\n";
}