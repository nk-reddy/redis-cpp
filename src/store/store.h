#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <mutex>
#include <variant>
#include <vector>
#include <condition_variable>

struct StreamEntry {
    std::string id;
    std::vector<std::pair<std::string, std::string>> fields;
};

using RedisValue = std::variant<
    std::string, 
    std::vector<std::string>,
    std::vector<StreamEntry>
>;

struct Entry {
    RedisValue value;
    std::string type;
    std::optional<std::chrono::steady_clock::time_point> expiry;
};

class Store {
    private:
    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_map<std::string, Entry> data;

    public:
    void set(const std::string &key, const std::string &value);
    void set_with_expiry(const std::string &key, const std::string &value, int ms);
    std::string get(const std::string &key);
    std::string rpush(const std::string &key, const std::vector<std::string> &values);
    std::string lrange(const std::string &key, const std::string &start, const std::string &stop);
    std::string lpush(const std::string &key, const std::vector<std::string> &values);
    std::string llen(const std::string &key);
    std::string lpop(const std::string &key, int n);
    std::string blpop(const std::string &key, double timeout);
    std::string type(const std::string &key);
    std::string xadd(const std::string &key, std::string id, const std::vector<std::string> &values);
    std::string validate_stream_id(const std::string &key, const std::string &id);
    void generate_stream_id(const std::string &key, std::string &id);
    std::string xrange(const std::string &key, const std::string &start, const std::string &stop);
    std::string xread(const std::vector<std::string> &keys, const std::vector<std::string> &ids, double timeout = -1);
};