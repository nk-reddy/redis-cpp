#include "client.h"

#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

void handle_client(int client_fd) {
  const char *response = "+PONG\r\n";
  char buffer[1024];
  while (1) {
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {break;}
    send(client_fd, response, strlen(response), 0);
  }
  close(client_fd);
}