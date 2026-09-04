#pragma once

#include "../store/store.h"
#include "../server/server.h"

#include <string>
#include <vector> 

std::string handle_command(const std::string &command, const std::vector<std::string> &data, Store &store, ServerState &server);
std::string handle_command_default();
std::string handle_command_echo(const std::vector<std::string>& args);
std::string handle_command_set(const std::vector<std::string>& args, Store &store);
std::string handle_command_get(const std::vector<std::string>& args, Store &store);
std::string handle_command_rpush(const std::vector<std::string>& args, Store &store);
std::string handle_command_lrange(const std::vector<std::string>& args, Store &store);
std::string handle_command_lpush(const std::vector<std::string>& args, Store &store);
std::string handle_command_llen(const std::vector<std::string>& args, Store &store);
std::string handle_command_lpop(const std::vector<std::string>& args, Store &store);
std::string handle_command_blpop(const std::vector<std::string>& args, Store &store);
std::string handle_command_type(const std::vector<std::string>& args, Store &store);
std::string handle_command_xadd(const std::vector<std::string>& args, Store &store);
std::string handle_command_xrange(const std::vector<std::string>& args, Store &store);
std::string handle_command_xread(const std::vector<std::string>& args, Store &store);
std::string handle_command_incr(const std::vector<std::string>& args, Store &store);
std::string handle_command_info(const std::vector<std::string>& args, ServerState &server);
std::string handle_command_replconf();

// special commands
void handle_command_psync(int client_fd, const std::vector<std::string>& args, ServerState &server);