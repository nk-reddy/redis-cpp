#pragma once

#include <string>
#include <utility>
#include <vector>
#include <cstdint>
#include <optional>

struct ParseResult
{
    bool complete;
    std::vector<std::string> args;
    size_t bytes_consumed;
    std::string raw_command;
};

struct RdbEntry 
{
    std::string key;
    std::string value;
    std::optional<uint64_t> expiry_ms;
};

struct RdbLength 
{
    uint64_t value;
    bool encoded;
};

std::vector<std::string> parse_resp(std::string input);
double parse_resp_string_to_double(const std::string &input);
std::pair<long long, long long> parse_stream_id(const std::string &id, bool is_start = false);

std::string encode_resp_string(const std::string &str);
std::string encode_resp_array(const std::vector<std::string> &values);
std::string encode_resp_integer(const int &val);

std::string parse_hex_to_binary(const std::string &hex_str);

ParseResult parse_one_resp_array(const std::string &input);

// parsing RDB file functions
uint8_t read_byte(const std::string &data, size_t &pos);
uint32_t read_u32_le(const std::string &data, size_t &pos);
uint64_t read_u64_le(const std::string &data, size_t &pos);
uint32_t read_u32_be(const std::string &data, size_t &pos);
uint64_t read_u64_be(const std::string &data, size_t &pos);
RdbLength read_length(const std::string &data, size_t &pos);
std::string read_rdb_string(const std::string &data, size_t &pos);
std::vector<RdbEntry> parse_rdb(const std::string &contents);

// other
std::vector<std::string> split_spaces(const std::string &input);

// geospatial helpers
uint64_t geo_encode(const double &latitude, const double &longitude);
std::pair<double, double> geo_decode(uint64_t geo_code);
double haversine_distance(const std::pair<double, double> &pos1, const std::pair<double, double> &pos2);

// auth helpers 
std::string sha256(const std::string& input);