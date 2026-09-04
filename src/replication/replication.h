#pragma once

#include <netdb.h>
#include <string>
#include "../server/server.h"

class ReplicaSocket
{
    private:
    std::string master_host;
    std::string master_port;
    int master_fd = -1;

    public:
    ReplicaSocket() = default;
    ReplicaSocket(
        const std::string &host, 
        const std::string &port
    );
    ~ReplicaSocket();

    bool connect_to_master();
    bool handshake_with_master(ServerState &state);

    int get_master_fd() const;
    void set_master(const std::string &host, const std::string &port);
};