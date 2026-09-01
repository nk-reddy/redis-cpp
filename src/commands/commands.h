#pragma once

#include "../store/store.h"

#include <string>
#include <vector> 

std::string handle_command_default();
std::string handle_command_echo(const std::vector<std::string>& args);
std::string handle_command_set(const std::vector<std::string>& args, Store &store);
std::string handle_command_get(const std::vector<std::string>& args, Store &store);
std::string handle_command_rpush(const std::vector<std::string>& args, Store &store);
std::string handle_command_lrange(const std::vector<std::string>& args, Store &store);
std::string handle_command_lpush(const std::vector<std::string>& args, Store &store);
std::string handle_command_llen(const std::vector<std::string>& args, Store &store);