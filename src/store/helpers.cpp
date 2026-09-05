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
    std::string result;
    if (hex_str.length() % 2 != 0 ) { return ""; }

    result.reserve(hex_str.length() / 2);
    for (size_t i = 0; i < hex_str.length(); i += 2) {
        std::string byte_str = hex_str.substr(i, 2);
        unsigned int value = std::stoul(byte_str, nullptr, 16);
        result.push_back(static_cast<char>(value));
    }
    
    return result;
}

ParseResult parse_one_resp_array(const std::string &input) {
    ParseResult result{false, {}, 0};
    size_t pos = 0;

    // parse the number of elements
    if (pos >= input.size() || input[pos] != '*') { return result; }

    size_t line_end = input.find("\r\n", pos);
    if (line_end == std::string::npos) { return result; }

    int count = std::stoi(input.substr(pos + 1, line_end - (pos + 1)));

    pos = line_end + 2;
    for (int i = 0; i < count; ++i) {
        // parse each element
        // parse the bulk string (header)
        if (pos >= input.size() || input[pos] != '*') { return result; }

        line_end = input.find("\r\n", pos);
        if (line_end == std::string::npos) { return result; }

        int length = std::stoi(input.substr(pos + 1, line_end - (pos + 1)));

        // parse the bulk string (value)
        pos = line_end + 2;
        if ((pos + length + 2) > input.size()) { return result; }
        std::string value = input.substr(pos, length);
        result.args.push_back(value);
        pos += length; 
        if (input.substr(pos, 2) != "\r\n") { return result; }
        pos += 2;
    }
    result.raw_command = input.substr(0, pos);
    result.complete = true;
    result.bytes_consumed = pos;
    return result;
}