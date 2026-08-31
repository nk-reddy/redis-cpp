#include "store.h"

#include <string>
#include <unordered_map>
#include <algorithm>

void Store::set(const std::string &key, const std::string &value) {
    data[key] = value;
}

std::string Store::get(const std::string &key) {
    auto it = data.find(key);
    if (it == data.end()) {
        return "$-1\r\n";
    }
    std::string value = it->second;
    return "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
}