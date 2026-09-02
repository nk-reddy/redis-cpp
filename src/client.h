#pragma once

#include "store/store.h"
#include "server/server.h"

void handle_client(int client_fd, Store &store, ServerState &server);