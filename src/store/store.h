#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <mutex>
#include <variant>
#include <vector>
#include <condition_variable>
#include <set>
#include <utility>

struct StreamEntry {
    std::string id;
    std::vector<std::pair<std::string, std::string>> fields;
};

using RedisValue = std::variant<
    std::string, 
    std::vector<std::string>,
    std::vector<StreamEntry>,
    std::set<std::pair<double, std::string>>
>;

struct Entry {
    RedisValue value;
    std::string type;
    std::optional<std::chrono::steady_clock::time_point> expiry;
    unsigned long long version = 0;
};

class Store {
    private:
    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_map<std::string, Entry> data;

    public:
    void set(const std::string &key, const std::string &value);
    void set_with_expiry(const std::string &key, const std::string &value, long long ms);
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
    std::string incr(const std::string &key);
    long long get_version(const std::string &key);

    void read_rdb_file(const std::string &file_path);
    std::vector<std::string> get_keys();

    // sorted set functionality
    std::string zadd(const std::string &key, const std::string &member, double &score);
    std::string zrank(const std::string &key, const std::string &member);
    std::string zrange(const std::string &key, const std::string &start, const std::string &stop);
    std::string zcard(const std::string &key);
    std::string zscore(const std::string &key, const std::string &member);
    std::string zrem(const std::string &key, const std::string &member);
};