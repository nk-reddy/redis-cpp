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
    std::string get_role();
    ServerState(std::string role) {
        this->role = role;
    }
};