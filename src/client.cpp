#include "client.h"
#include "commands/commands.h"
#include "store/store.h"
#include "store/helpers.h"

#include <string>
#include <cctype>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <queue>
#include <unordered_set>

void handle_client(int client_fd, Store &store, ServerState &server, bool is_master_connection) {
    server.add_connected_client();
    std::string pending;
    char buffer[1024];

    // state variables SPECIFIC to the client
    bool in_multi = false;
    std::queue<QueuedCommand> queued_commands {};
    std::unordered_map<std::string, long long> watched_keys {};

    while (1) 
    {
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) { break; }

        pending.append(buffer, bytes_received);
        while (1)
        {
            ParseResult parsed = parse_one_resp_array(pending);
            if (!parsed.complete) { break; }

            std::vector<std::string> data = parsed.args;
            pending.erase(0, parsed.bytes_consumed);

            if (data.empty()) {
                break;
            }
            std::transform(data[0].begin(), data[0].end(), data[0].begin(), ::tolower);
            std::string response;

            // case - command psync (multiple sends)
            if (data[0] == "psync")
            {
                handle_command_psync(client_fd, data, server);
                continue;
            }

            // case - command multi
            if (data[0] == "multi") 
            {
                in_multi = true;
                response = "+OK\r\n";
            }
            // case - command unwatch 
            else if (data[0] == "unwatch")
            {
                watched_keys = {};
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
                            for (const auto &[key, version] : watched_keys) {
                                if (store.get_version(key) != version) {
                                    watched_change = true;
                                    break;
                                }
                            }

                            watched_keys = {};
                            if (watched_change)
                            {
                                response = "*-1\r\n";
                                queued_commands = {};
                            }
                            else 
                            {
                                response = "*" + std::to_string(queued_commands.size()) + "\r\n";
                                while (!queued_commands.empty()) {
                                    QueuedCommand current_command = queued_commands.front();
                                    std::string current_response = handle_command(current_command.args[0], current_command.args, store, server, current_command.raw_command);
                                    response += current_response;
                                    queued_commands.pop();
                                }
                            }
                        }
                        else if (data[0] == "discard")
                        {
                            in_multi = false;
                            watched_keys = {};
                            queued_commands = {};
                            response = "+OK\r\n";
                        }
                        else if (data[0] == "watch") { response = "-ERR WATCH inside MULTI is not allowed\r\n"; }
                        else 
                        {
                            queued_commands.push({data, parsed.raw_command});
                            response = "+QUEUED\r\n";
                        }
                        break;
                    case false: // case - prior command has multi OFF
                        if (data[0] == "exec") { response = "-ERR EXEC without MULTI\r\n"; }
                        else if (data[0] == "discard") { response = "-ERR DISCARD without MULTI\r\n"; }
                        else if (data[0] == "watch") {
                            for (size_t i = 1; i < data.size(); ++i) {
                                watched_keys[data[i]] = store.get_version(data[i]);
                            }
                            response = "+OK\r\n";
                        }
                        else { response = handle_command(data[0], data, store, server, parsed.raw_command); }
                        break;
                }
            }

            // send response back to the client
            if (!is_master_connection && data[0] != "replconf") { send(client_fd, response.data(), response.length(), 0); }
        }
    }
    
    close(client_fd);
}