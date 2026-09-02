#include "client.h"
#include "commands/commands.h"
#include "store/store.h"
#include "store/helpers.h"

#include <string>
#include <cctype>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <queue>
#include <unordered_set>

void handle_client(int client_fd, Store &store, ServerState &server) {
  char buffer[1024];

  // state variables SPECIFIC to the client
  bool in_multi = false;
  std::queue<std::vector<std::string>> queuedCommands {};
  std::unordered_map<std::string, long long> watchedKeys {};

  while (1) {
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) { break; }

    std::string ret(buffer, bytes_received);
    std::vector<std::string> data = parse_resp(ret);
    if (data.empty()) {
        break;
    }
    std::transform(data[0].begin(), data[0].end(), data[0].begin(), ::tolower);
    std::string response;

    // case - command multi
    if (data[0] == "multi") 
    {
        in_multi = true;
        response = "+OK\r\n";
    }
    // case - command unwatch 
    else if (data[0] == "unwatch")
    {
        watchedKeys = {};
        response = "+OK\r\n";
    }
    // case - command is NOT multi
    else 
    {
        switch (in_multi) 
        {
            case true: // case - prior command has multi ON
                if (data[0] == "exec") 
                {
                    in_multi = false;
                    bool watched_change = false;

                    // determine if any watched keys were touched
                    for (const auto &[key, version] : watchedKeys) {
                        if (store.get_version(key) != version) {
                            watched_change = true;
                            break;
                        }
                    }

                    watchedKeys = {};
                    if (watched_change)
                    {
                        response = "*-1\r\n";
                        queuedCommands = {};
                    }
                    else 
                    {
                        response = "*" + std::to_string(queuedCommands.size()) + "\r\n";
                        while (!queuedCommands.empty()) {
                            std::vector<std::string> currentCommand = queuedCommands.front();
                            std::string currentResponse = handle_command(currentCommand[0], currentCommand, store, server);
                            response += currentResponse;
                            queuedCommands.pop();
                        }
                    }
                }
                else if (data[0] == "discard")
                {
                    in_multi = false;
                    watchedKeys = {};
                    queuedCommands = {};
                    response = "+OK\r\n";
                }
                else if (data[0] == "watch") { response = "-ERR WATCH inside MULTI is not allowed\r\n"; }
                else 
                {
                    queuedCommands.push(data);
                    response = "+QUEUED\r\n";
                }
                break;
            case false: // case - prior command has multi OFF
                if (data[0] == "exec") { response = "-ERR EXEC without MULTI\r\n"; }
                else if (data[0] == "discard") { response = "-ERR DISCARD without MULTI\r\n"; }
                else if (data[0] == "watch") {
                    for (size_t i = 1; i < data.size(); ++i) {
                        watchedKeys[data[i]] = store.get_version(data[i]);
                    }
                    response = "+OK\r\n";
                }
                else { response = handle_command(data[0], data, store, server); }
                break;
        }
    }

    // send response back to the client
    send(client_fd, response.data(), response.length(), 0);
  }
  close(client_fd);
}