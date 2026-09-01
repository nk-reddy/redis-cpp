#include "client.h"
#include "commands/commands.h"
#include "store/store.h"

#include <string>
#include <cctype>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

std::vector<std::string> parse_resp(std::string input);

void handle_client(int client_fd, Store &store) {
  char buffer[1024];
  while (1) {
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {break;}

    std::string ret(buffer, bytes_received);
    std::vector<std::string> data = parse_resp(ret);
    if (data.empty()) {
        break;
    }
    std::transform(data[0].begin(), data[0].end(), data[0].begin(), ::tolower);
    std::string response = "";

    // handle the commands by case
    if (data[0] == "echo") {
        response = handle_command_echo(data);
    }
    else if (data[0] == "set") {
        response = handle_command_set(data, store);
    }
    else if (data[0] == "get") {
        response = handle_command_get(data, store);
    }
    else if (data[0] == "rpush") {
        response = handle_command_rpush(data, store);
    }
    else if (data[0] == "lrange") {
        response = handle_command_lrange(data, store);
    }
    else if (data[0] == "lpush") {
        response = handle_command_lpush(data, store);
    }
    else if (data[0] == "llen") {
        response = handle_command_llen(data, store);
    }
    else {
        response = handle_command_default();
    }
    send(client_fd, response.data(), response.length(), 0);
  }
  close(client_fd);
}

std::vector<std::string> parse_resp(std::string input) {
    size_t pos = 1;
    std::vector<std::string> elems;

    size_t end = input.find("\r\n", pos);
    int nelems = std::stoi(input.substr(pos, end - pos));
    if (nelems <= 0) {
        return elems;
    }

    // reach the first data val past the '$'
    pos = end + 3;
    for (size_t i = 0; i < nelems; i++) {
        size_t end = input.find("\r\n", pos);
        int bytes = std::stoi(input.substr(pos, end - pos));
        pos = end + 2;
        elems.push_back(input.substr(pos, bytes));
        pos += (bytes + 3);
    }

    return elems;
}