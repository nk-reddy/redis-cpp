#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct QueuedCommand {
    std::vector<std::string> args;
    std::string raw_command;
};

struct ClientState {
    int client_fd = -1;
    bool in_multi = false;
    bool in_subscribed_mode = false;
    std::queue<QueuedCommand> queued_commands {};
    std::unordered_map<std::string, long long> watched_keys {};
    std::unordered_set<std::string> subscribed_channels;
};