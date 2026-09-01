#include "helpers.h"

#include <limits>
#include <vector>

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