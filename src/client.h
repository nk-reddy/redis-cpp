#pragma once

#include "store/store.h"
#include "server/server.h"

#include <vector>

struct QueuedCommand {
    std::vector<std::string> args;
    std::string raw_command;
};

void handle_client(int client_fd, Store &store, ServerState &server, bool is_master_connection = false);