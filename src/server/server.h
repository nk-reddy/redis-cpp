#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
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

    std::unordered_map<int, long long> replica_fds;

    std::mutex replica_mutex;
    std::condition_variable replica_cv;

    bool writes_since_last_wait = false;

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

    std::vector<int> ServerState::get_connected_replicas();
    int get_num_connected_replicas();
    int get_num_connected_replicas_with_offset(long long offset);

    std::mutex& get_replica_mutex() { return replica_mutex; };
    std::condition_variable& get_replica_cv() { return replica_cv; }

    void request_ack_from_replicas();
    void update_replica_offset(int fd, long long offset);

    bool get_writes_since_last_wait() { return writes_since_last_wait; }
    void set_writes_since_last_wait(bool update) { writes_since_last_wait = update; }
};