#include "client.h"
#include "commands/commands.h"
#include "store/store.h"
#include "store/helpers.h"
#include "client-state/client-state.h"

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
    ClientState client_state;
    client_state.client_fd = client_fd;

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
                client_state.in_multi = true;
                response = "+OK\r\n";
            }
            // case - command unwatch 
            else if (data[0] == "unwatch")
            {
                client_state.watched_keys = {};
                response = "+OK\r\n";
            }
            // case - command is NOT multi
            else 
            {
                switch (client_state.in_multi) 
                {
                    case true: // case - prior command has multi ON
                        if (data[0] == "exec") 
                        {
                            client_state.in_multi = false;
                            bool watched_change = false;

                            // determine if any watched keys were touched
                            for (const auto &[key, version] : client_state.watched_keys) {
                                if (store.get_version(key) != version) {
                                    watched_change = true;
                                    break;
                                }
                            }

                            client_state.watched_keys = {};
                            if (watched_change)
                            {
                                response = "*-1\r\n";
                                client_state.queued_commands = {};
                            }
                            else 
                            {
                                response = "*" + std::to_string(client_state.queued_commands.size()) + "\r\n";
                                while (!client_state.queued_commands.empty()) {
                                    QueuedCommand current_command = client_state.queued_commands.front();
                                    std::string current_response = handle_command(current_command.args[0], current_command.args, store, server, current_command.raw_command, &client_state);
                                    response += current_response;
                                    client_state.queued_commands.pop();
                                }
                            }
                        }
                        else if (data[0] == "discard")
                        {
                            client_state.in_multi = false;
                            client_state.watched_keys = {};
                            client_state.queued_commands = {};
                            response = "+OK\r\n";
                        }
                        else if (data[0] == "watch") { response = "-ERR WATCH inside MULTI is not allowed\r\n"; }
                        else 
                        {
                            client_state.queued_commands.push({data, parsed.raw_command});
                            response = "+QUEUED\r\n";
                        }
                        break;
                    case false: // case - prior command has multi OFF
                        if (data[0] == "exec") { response = "-ERR EXEC without MULTI\r\n"; }
                        else if (data[0] == "discard") { response = "-ERR DISCARD without MULTI\r\n"; }
                        else if (data[0] == "watch") {
                            for (size_t i = 1; i < data.size(); ++i) {
                                client_state.watched_keys[data[i]] = store.get_version(data[i]);
                            }
                            response = "+OK\r\n";
                        }
                        else { response = handle_command(data[0], data, store, server, parsed.raw_command, &client_state); }
                        break;
                }
            }

            // send response back to the client
            if (is_master_connection) 
            {
                server.add_offset(parsed.raw_command.length());
                if (data[0] != "replconf") { continue; }
            }
            if (!response.empty()) { send(client_fd, response.data(), response.length(), 0); }
        }
    }
    
    close(client_fd);
}