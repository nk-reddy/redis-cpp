#include "store.h"

#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>

void Store::set(const std::string &key, const std::string &value) {
    data[key] = Entry{
        .value = value,
        .expiry = std::nullopt
    };
}

void Store::set_with_expiry(const std::string &key, const std::string &value, int ms) {
    data[key] = Entry{
        .value = value,
        .expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms)
    };
}

std::string Store::get(const std::string &key) {
    auto it = data.find(key);
    if (it == data.end()) {
        return "$-1\r\n";
    }
    if (it->second.expiry && *it->second.expiry <= std::chrono::steady_clock::now()) {
        data.erase(it);
        return "$-1\r\n";
    }
    std::string value = it->second.value;
    return "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
}