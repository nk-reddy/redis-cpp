#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <optional>

struct Entry {
    std::string value;
    std::optional<std::chrono::steady_clock::time_point> expiry;
};

class Store {
    private:
    std::unordered_map<std::string, Entry> data;

    public:
    void set(const std::string &key, const std::string &value);
    void set_with_expiry(const std::string &key, const std::string &value, int ms);
    std::string get(const std::string &key);
};