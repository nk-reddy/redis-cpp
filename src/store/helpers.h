#pragma once

#include <string>
#include <utility>
#include <vector>

struct ParseResult
{
    bool complete;
    std::vector<std::string> args;
    std::string raw_command;
    size_t bytes_consumed;
};

std::vector<std::string> parse_resp(std::string input);
std::pair<long long, long long> parse_stream_id(const std::string &id, bool is_start = false);

std::string encode_resp_string(const std::string &str);
std::string encode_resp_array(const std::vector<std::string> &values);

std::string parse_hex_to_binary(const std::string &hex_str);

ParseResult parse_one_resp_array(const std::string &input);
