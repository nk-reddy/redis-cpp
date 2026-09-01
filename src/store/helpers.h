#pragma once

#include <string>
#include <utility>

std::pair<long long, long long> parse_stream_id(const std::string &id, bool is_start = false);

std::string encode_resp_array(const std::vector<std::string> &values);