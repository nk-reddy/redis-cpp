#include "commands.h"
#include "../store/store.h"

#include <string>
#include <vector> 

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
    if (args.size() != 3) {
        return "-ERR invalid arguments\r\n";
    }
    store.set(args[1], args[2]);
    return "+OK\r\n";
}

std::string handle_command_get(const std::vector<std::string>& args, Store &store) {
    if (args.size() != 2) {
        return "-ERR invalid arguments\r\n";
    }
    return store.get(args[1]);
}