#pragma once

#include <string>

class ServerState 
{
    private:
    std::string role = "master";
    std::string redis_version = "7.2.4";
    std::string master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
    int port;
    int connected_clients = 0;
    long long used_memory = 0;
    long long master_repl_offset = 0;

    public:
    ServerState() = default;
    ServerState(int port) {
        this->port = port;
    }
    ServerState(std::string role) {
        this->role = role;
    }

    std::string get_role();
    std::string get_master_replid();
    std::string get_redis_version();
    int get_port();
    int get_connected_clients();
    long long get_used_memory();
    long long get_master_repl_offset();

    void set_role(std::string role);
    void add_connected_client();
};