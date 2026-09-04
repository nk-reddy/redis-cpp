#include "helpers.h"

#include <limits>
#include <vector>
#include <format>

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

std::pair<long long, long long> parse_stream_id(const std::string &id, bool is_start) {
    size_t dash = id.find("-");
    long long ms = 0;
    long long seq = 0;
    if (dash == std::string::npos) {
        ms = std::stoll(id);
        seq = is_start ? 0 : std::numeric_limits<long long>::max();
    }
    else {
        ms = std::stoll(id.substr(0, dash));
        seq = std::stoll(id.substr(dash + 1));
    }
    return {ms, seq};
}

std::string encode_resp_array(const std::vector<std::string> &values) {
    std::string response = "*" + std::to_string(values.size()) + "\r\n";
    for (const std::string &str: values) {
        response += ("$" + std::to_string(str.length()) + "\r\n" + str + "\r\n");
    }
    return response;
}

std::string encode_resp_string(const std::string &str) {
    return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
}

std::string parse_hex_to_binary(const std::string &hex_str) {
    unsigned long val = std::stoll(hex_str, nullptr, 16);
    std::string binary_str = std::format("{:b}", val);
    return binary_str;
}