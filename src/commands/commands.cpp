#include "commands.h"
#include "../store/store.h"

#include <string>
#include <vector> 
#include <algorithm>

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
    if (type != "streams") {
        return "-ERR invalid arguments\r\n";
    }

    std::vector<std::string> keys {};
    std::vector<std::string> ids {};
    for (size_t i = 2; i < args.size(); ++i) {
        size_t mid = args.size() / 2;
        if (i <= mid) { keys.push_back(args[i]); }
        else { ids.push_back(args[i]); }
    }
    return store.xread(keys, ids);
}