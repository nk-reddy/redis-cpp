#pragma once

#include <string>
#include <unordered_set>
#include <mutex>
#include <condition_variable>

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

    std::unordered_set<int> replica_fds {};

    std::mutex replica_mutex;
    std::condition_variable replica_cv;

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
    void add_connected_replica(int client_fd);
    void add_offset(int bytes);

    std::unordered_set<int> get_connected_replicas();
    int get_num_connected_replicas();

    std::mutex& get_replica_mutex() { return replica_mutex; };
    std::condition_variable& get_replica_cv() { return replica_cv; }
};