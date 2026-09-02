#pragma once

#include <string>

class ServerState 
{
    private:
    std::string role;
    int connected_slaves;
    long long master_replid;
    long long master_repl_offset;

    public:
    ServerState() = default;
    ServerState(std::string role) {
        this->role = role;
    }

    std::string get_role();
};