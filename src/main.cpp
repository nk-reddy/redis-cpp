#include "client.h"
#include "store/store.h"
#include "server/server.h"
#include "replication/replication.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <functional>

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // parse the CLI arguments
  bool custom_port, is_replica; 
  int custom_port_index, master_server_index; 
  for (size_t i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--port") {
      if (i == (argc - 1)) {
        std::cerr << "Invalid CLI arguments\n";
        return 1;
      }
      custom_port = true;
      custom_port_index = i + 1;
    }
    else if (std::string(argv[i]) == "--replicaof") {
      if (i == (argc - 1)) {
        std::cerr << "Invalid CLI arguments\n";
        return 1;
      }
      is_replica = true;
      master_server_index = i + 1;
    }
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // add custom port configurability
  int port = 6379;
  if (custom_port) {
    port = std::stoi(argv[custom_port_index]);
  }
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port " + std::to_string(port) +"\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  std::cout << "Waiting for a client to connect...\n";

  Store store;
  ServerState state(port);
  ReplicaSocket replica;

  // add server replica configurability
  if (is_replica) {
    state.set_role("slave");

    // parse the master server
    std::string master_info = std::string(argv[master_server_index]);
    size_t split = master_info.find(" ");
    std::string host_str = master_info.substr(0, split);
    std::string port_str = master_info.substr(split + 1);

    // define a replica socket
    ReplicaSocket replica(host_str, port_str);

    // connect to the master server
    if (!replica.connect_to_master()) {
      std::cerr << "Failed to connect to master server...\n";
      return -1;
    }

    if (!replica.handshake_with_master(state)) {
      std:: cerr << "Failed handshake with master server...\n";
      return -1;
    }

    // listen for commands from the master
    std::thread(handle_client, replica.get_master_fd(), std::ref(store), std::ref(state), true).detach();
  }

  while (true) {
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::thread(handle_client, client_fd, std::ref(store), std::ref(state), false).detach();
  }

  close(server_fd);
  return 0;
}
