#include "replication.h"
#include "../store/helpers.h"
#include "../client.h"

#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

ReplicaSocket::ReplicaSocket(
    const std::string &host, 
    const std::string &port): 
        master_host(host),
        master_port(port),
        master_fd(-1)
{
}

ReplicaSocket::~ReplicaSocket() {
    if (master_fd > 0) {
        close(master_fd);
    }
}

int ReplicaSocket::get_master_fd() const { return master_fd; }

void ReplicaSocket::set_master(const std::string &host, const std::string &port) {
    master_host = host;
    master_port = port;
}

bool ReplicaSocket::connect_to_master() {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // define the server socket
    addrinfo *result = nullptr;
    if (getaddrinfo(master_host.c_str(), master_port.c_str(), &hints, &result) != 0) {
      return false;
    }

    // create the client socket
    int fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(result);
        return false;
    }
    this->master_fd = fd;

    // connect to the server
    if (connect(this->master_fd, result->ai_addr, result->ai_addrlen) != 0) {
        close(this->master_fd);
        this->master_fd = -1;
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);
    return true;
}

bool ReplicaSocket::handshake_with_master(ServerState &state) {
    // 1 - send PING
    const char *message = "*1\r\n$4\r\nPING\r\n";
    if (send(master_fd, message, strlen(message), 0) < 0) {
        return false;
    }

    char buffer[128];
    int bytes_received = recv(master_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) { return false; }

    std::string response(buffer, bytes_received);
    if (response != "+PONG\r\n") { return false; }

    // 2 - send REPLCONF twice
    std::string listening_port = std::to_string(state.get_port());
    std::vector<std::string> replconf_vec_one{"REPLCONF", "listening-port", listening_port};
    std::vector<std::string> replconf_vec_two{"REPLCONF", "capa", "psync2"};
    std::string replconf_message_one = encode_resp_array(replconf_vec_one);
    std::string replconf_message_two = encode_resp_array(replconf_vec_two);

    if (send(master_fd, replconf_message_one.c_str(), replconf_message_one.length(), 0) < 0) {
        return false;
    }

    bytes_received = recv(master_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) { return false; }
    response = std::string(buffer, bytes_received);
    if (response != "+OK\r\n") { return false; }

    if (send(master_fd, replconf_message_two.c_str(), replconf_message_two.length(), 0) < 0) {
        return false;
    }

    bytes_received = recv(master_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) { return false; }
    response = std::string(buffer, bytes_received);
    if (response != "+OK\r\n") { return false; }

    // 3 - send PSYNC
    std::vector<std::string> psync_vec{"PSYNC", "?", "-1"};
    std::string psync_message = encode_resp_array(psync_vec);

    if (send(master_fd, psync_message.c_str(), psync_message.length(), 0) < 0) {
        return false;
    }

    bytes_received = recv(master_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) { return false; }

    return true;
}

void ReplicaSocket::listen_for_master_commands(Store &store, ServerState &server) {
    handle_client(master_fd, store, server, true);
}