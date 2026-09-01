#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <mutex>
#include <variant>
#include <vector>

using RedisValue = std::variant<
    std::string, 
    std::vector<std::string>
>;

struct Entry {
    RedisValue value;
    std::optional<std::chrono::steady_clock::time_point> expiry;
};

class Store {
    private:
    std::mutex mtx;
    std::unordered_map<std::string, Entry> data;

    public:
    void set(const std::string &key, const std::string &value);
    void set_with_expiry(const std::string &key, const std::string &value, int ms);
    std::string get(const std::string &key);
    std::string rpush(const std::string &key, const std::vector<std::string> &values);
    std::string lrange(const std::string &key, const std::string &start, const std::string &stop);
    std::string lpush(const std::string &key, const std::vector<std::string> &values);
};