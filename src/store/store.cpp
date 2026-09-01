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
        .type = "string",
        .expiry = std::nullopt
    };
}

void Store::set_with_expiry(const std::string &key, const std::string &value, int ms) {
    std::lock_guard<std::mutex> lock(mtx);
    data[key] = Entry{
        .value = value,
        .type = "string",
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
            .type = "list",
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
    cv.notify_one();
    return ":" + std::to_string(list.size()) + "\r\n";
}

std::string Store::lrange(const std::string &key, const std::string &start, const std::string &stop) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return "*0\r\n";
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    int start_i = std::stoi(start); 
    int end_i = std::stoi(stop);
    auto &list = std::get<std::vector<std::string>>(it->second.value);
    if (start_i < 0) {
        if (abs(start_i) > list.size()) { start_i = 0; }
        else { start_i += list.size(); }
    }
    if (end_i < 0) {
        if (abs(end_i) > list.size()) { end_i = 0; }
        else { end_i += list.size(); }
    }

    if (start_i >= list.size() || start_i > end_i) {
        return "*0\r\n";
    }

    int response_size = end_i >= list.size() ? list.size() - start_i : end_i - start_i + 1;
    std::string response = "*" + std::to_string(response_size) + "\r\n";
    for (int i = start_i; i < start_i + response_size; ++i) {
        response += "$" + std::to_string(list[i].length()) + "\r\n" + list[i] + "\r\n";
    }
    return response;
}

std::string Store::lpush(const std::string &key, const std::vector<std::string> &values) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        data[key] = Entry{
            .value = std::vector<std::string>{},
            .type = "list",
            .expiry = std::nullopt
        };
        it = data.find(key);
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    auto &list = std::get<std::vector<std::string>>(it->second.value);
    for (const std::string &value : values) {
        list.insert(list.begin(), value);
    }
    cv.notify_one();
    return ":" + std::to_string(list.size()) + "\r\n";
}

std::string Store::llen(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return ":0\r\n";
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    auto &list = std::get<std::vector<std::string>>(it->second.value);
    return ":" + std::to_string(list.size()) + "\r\n";
}

std::string Store::lpop(const std::string &key, int n) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return "*0\r\n";
    }
    if (!std::holds_alternative<std::vector<std::string>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    auto &list = std::get<std::vector<std::string>>(it->second.value);
    if (list.size() == 0) {
        return "*0\r\n";
    }
    if (n < 1) {
        std::string start = list[0];
        list.erase(list.begin());
        return "$" + std::to_string(start.length()) + "\r\n" + start + "\r\n";
    }
    if (n > list.size()) {
        n = list.size();
    }
    std::vector<std::string> to_remove(list.begin(), list.begin() + n);
    list.erase(list.begin(), list.begin() + n);

    std::string response = "*" + std::to_string(to_remove.size()) + "\r\n";
    for (const std::string &str: to_remove) {
        response += ("$" + std::to_string(str.length()) + "\r\n" + str + "\r\n");
    }
    return response;
}

std::string Store::blpop(const std::string &key, double timeout) {
    std::unique_lock<std::mutex> lk(mtx);

    auto it = data.find(key);
    if (it != data.end() &&
        !std::holds_alternative<std::vector<std::string>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    auto ready = [&]() {
        auto it = data.find(key);
        if (it == data.end()) {
            return false;
        }
        auto &list = std::get<std::vector<std::string>>(it->second.value);
        return !list.empty();
    };

    if (timeout == 0) {
        cv.wait(lk, ready);
    }
    else {
        bool got_ready = cv.wait_for(lk, std::chrono::duration<double>(timeout), ready);
        if (!got_ready) {
            return "*-1\r\n";
        }
    }

    it = data.find(key);
    auto &list = std::get<std::vector<std::string>>(it->second.value);
    std::string start = list[0];
    list.erase(list.begin());
    return "*2\r\n$" + std::to_string(key.length()) + "\r\n" + key + "\r\n$" + std::to_string(start.length()) + "\r\n" + start + "\r\n";
}

std::string Store::type(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return "+none\r\n";
    }
    return "+" + it->second.type + "\r\n";
}

std::string Store::xadd(const std::string &key, std::string id, const std::vector<std::string> &values) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        data[key] = Entry{
            .value = std::vector<StreamEntry>{},
            .type = "stream",
            .expiry = std::nullopt
        };
    }

    generate_stream_id(key, id);
    std::string error = validate_stream_id(key, id);
    if (error != "") {
        return error;
    }

    StreamEntry entry {.id = id};
    for (size_t i = 1; i < values.size(); i += 2) {
        entry.fields.push_back({values[i-1], values[i]});
    }

    it = data.find(key);
    auto &stream = std::get<std::vector<StreamEntry>>(it->second.value);
    stream.push_back(entry);
    return "$" + std::to_string(id.length()) + "\r\n" + id + "\r\n";
}

std::string Store::validate_stream_id(const std::string &key, const std::string &id) {
    if (id == "0-0") {
        return "-ERR The ID specified in XADD must be greater than 0-0\r\n";
    }
    auto it = data.find(key);
    auto &stream = std::get<std::vector<StreamEntry>>(it->second.value);
    if (stream.empty()) {
        return "";
    }

    std::string prev_id = stream[stream.size() - 1].id;
    size_t prev_dash = prev_id.find("-");
    size_t new_dash = id.find("-");

    long long prev_ms = std::stoll(prev_id.substr(0, prev_dash));
    long long prev_seq = std::stoll(prev_id.substr(prev_dash + 1));
    long long new_ms = std::stoll(id.substr(0, new_dash));
    long long new_seq = std::stoll(id.substr(new_dash + 1));

    if (new_ms < prev_ms || new_ms == prev_ms && new_seq <= prev_seq) {
        return "-ERR The ID specified in XADD is equal or smaller than the target stream top item\r\n";
    }
    return "";
}

void Store::generate_stream_id(const std::string &key, std::string &id) {
    auto it = data.find(key);
    auto &stream = std::get<std::vector<StreamEntry>>(it->second.value);

    long long prev_ms = -1;
    long long prev_seq = -1;

    if (!stream.empty()) {
        const std::string &prev_id = stream.back().id;
        size_t dash = prev_id.find("-");

        prev_ms = std::stoll(prev_id.substr(0, dash));
        prev_seq = std::stoll(prev_id.substr(dash + 1));
    }

    if (id == "*") {
        long long now =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

        long long seq = (prev_ms == now) ? prev_seq + 1 : 0;

        id = std::to_string(now) + "-" + std::to_string(seq);
        return;
    }

    if (id.size() < 2 || id.substr(id.size() - 2) != "-*") {
        return;
    }

    long long new_ms = std::stoll(id.substr(0, id.find("-")));
    long long new_seq;

    if (prev_ms == new_ms) { new_seq = prev_seq + 1; } 
    else { new_seq = (new_ms == 0) ? 1 : 0; }

    id = std::to_string(new_ms) + "-" + std::to_string(new_seq);
}