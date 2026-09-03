#include "server.h"

std::string ServerState::get_role() {
    return role;
}

std::string ServerState::get_redis_version() {
    return redis_version;
}

int ServerState::get_connected_clients() {
    return connected_clients;
}

long long ServerState::get_used_memory() {
    return used_memory;
}

void ServerState::set_role(std::string role) {
    this->role = role;
}

void ServerState::add_connected_client() {
    connected_clients++;
}