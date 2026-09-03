#include "commands.h"
#include "../store/store.h"
#include "../store/helpers.h"

#include <string>
#include <vector> 
#include <algorithm>
#include <unordered_set>

std::string handle_command(const std::string &command, const std::vector<std::string> &data, Store &store, ServerState &server) {
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
    else {
        response = handle_command_default();
    }
    return response;
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
        response += "connected_clients:" + server.get_connected_clients();
        response += "\r\n";
    }
    if (req_all || req_memory) {
        response += "# Memory\r\n";
        response += "used_memory:" + server.get_used_memory();
        response += "\r\n";
    }
    if (req_all || req_replication) {
        response += "# Replication\r\n";
        response += "role:" + server.get_role();
        response += "\r\n";
    }

    return encode_resp_string(response);
}