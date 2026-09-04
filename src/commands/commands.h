#pragma once

#include "../store/store.h"
#include "../server/server.h"

#include <string>
#include <vector> 

std::string handle_command(const std::string &command, const std::vector<std::string> &data, Store &store, ServerState &server, const std::string &rawCommand = "");

// special commands
void handle_command_psync(int client_fd, const std::vector<std::string>& args, ServerState &server);