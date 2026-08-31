#include "store.h"

#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>

void Store::set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(mtx);
    data[key] = Entry{
        .value = value,
        .expiry = std::nullopt
    };
}

void Store::set_with_expiry(const std::string &key, const std::string &value, int ms) {
    std::lock_guard<std::mutex> lock(mtx);
    data[key] = Entry{
        .value = value,
        .expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms)
    };
}

std::string Store::get(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return "$-1\r\n";
    }
    if (it->second.expiry && *it->second.expiry <= std::chrono::steady_clock::now()) {
        data.erase(it);
        return "$-1\r\n";
    }
    if (std::holds_alternative<std::string>(it->second.value)) {
        std::string value = std::get<std::string>(it->second.value);
        return "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
    }
    return "$-1\r\n";
}

std::string Store::rpush(const std::string &key, const std::vector<std::string> &values) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        data[key] = Entry{
            .value = std::vector<std::string>{},
            .expiry = std::nullopt
        };
        it = data.find(key);
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    auto &list = std::get<std::vector<std::string>>(it->second.value);
    for (const std::string &value : values) {
        list.push_back(value);
    }
    return ":" + std::to_string(list.size()) + "\r\n";
}