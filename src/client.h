#pragma once

#include "store/store.h"
#include "server/server.h"

#include <vector>

void handle_client(int client_fd, Store &store, ServerState &server, bool is_master_connection = false);