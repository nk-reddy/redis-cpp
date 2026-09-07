#pragma once

#include "../store/store.h"
#include "../server/server.h"
#include "../client-state/client-state.h"

#include <string>
#include <vector> 

std::string handle_command(const std::string &command, const std::vector<std::string> &data, Store &store, ServerState &server, const std::string &rawCommand = "", ClientState *client_state = nullptr);
std::string handle_command_default(bool in_subscribed_mode);
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
std::string handle_command_replconf(const std::vector<std::string>& args, ServerState &server, ClientState *client_state);
std::string handle_command_wait(const std::vector<std::string>& args, ServerState &server);
std::string handle_command_config(const std::vector<std::string>& args, ServerState &server);
std::string handle_command_keys(const std::vector<std::string>& args, Store &store);
std::string handle_command_subscribe(const std::vector<std::string>& args, ServerState &server, ClientState *client_state);
std::string handle_command_publish(const std::vector<std::string>& args, ServerState &server);
std::string handle_command_unsubscribe(const std::vector<std::string>& args, ServerState &server, ClientState *client_state);

// sorted set commands
std::string handle_command_zadd(const std::vector<std::string>& args, Store &store);
std::string handle_command_zrank(const std::vector<std::string>& args, Store &store);
std::string handle_command_zrange(const std::vector<std::string>& args, Store &store);
std::string handle_command_zcard(const std::vector<std::string>& args, Store &store);
std::string handle_command_zscore(const std::vector<std::string>& args, Store &store);
std::string handle_command_zrem(const std::vector<std::string>& args, Store &store);

// special commands
void handle_command_psync(int client_fd, const std::vector<std::string>& args, ServerState &server);

// others
bool modifying_command(const std::string &command);
bool subscribed_command(const std::string &command);
void propagate_command_to_replicas(const std::string &rawCommand, ServerState &server);
