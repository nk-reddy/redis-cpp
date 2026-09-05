#pragma once

#include "../store/store.h"
#include "../server/server.h"

#include <netdb.h>
#include <string>

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

    std::string recv_line(int fd);
};