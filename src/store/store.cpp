#include "store.h"
#include "helpers.h"

#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>
#include <fstream>
#include <iterator>
#include <ranges>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <bit>

void Store::set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(mtx);

    unsigned long long version = 0;
    auto it = data.find(key);
    if (it != data.end()) { version = it->second.version; }

    data[key] = Entry{
        .value = value,
        .type = "string",
        .expiry = std::nullopt,
        .version = version + 1
    };
}

void Store::set_with_expiry(const std::string &key, const std::string &value, long long ms) {
    std::lock_guard<std::mutex> lock(mtx);

    unsigned long long version = 0;
    auto it = data.find(key);
    if (it != data.end()) { version = it->second.version; }

    data[key] = Entry{
        .value = value,
        .type = "string",
        .expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms),
        .version = version + 1
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
    it->second.version++;
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
    it->second.version++;
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
    it->second.version++;
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
    it->second.version++;
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
    it->second.version++;
    cv.notify_all();
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

std::string Store::xrange(const std::string &key, const std::string &start, const std::string &stop) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return "*0\r\n";
    }
    if (!std::holds_alternative<std::vector<StreamEntry>>(it->second.value)) {
        return "-ERR invalid arguments\r\n";
    }

    auto &stream = std::get<std::vector<StreamEntry>>(it->second.value); 

    // need to get a subset of stream from start to stop, inclusive
    std::pair<long long, long long> id_start = start == "-" ? std::make_pair(0LL, 0LL) 
                                                            : parse_stream_id(start, true);
    std::pair<long long, long long> id_stop = stop == "+" ? std::make_pair(std::numeric_limits<long long>::max(), std::numeric_limits<long long>::max()) 
                                                            : parse_stream_id(stop);
    std::vector<StreamEntry> responseEntries{};
    for (StreamEntry &entry: stream) {
        std::pair<long long, long long> id_curr = parse_stream_id(entry.id);
        if (id_curr.first > id_start.first || id_curr.first == id_start.first && id_curr.second >= id_start.second) {
            if (id_curr.first < id_stop.first || id_curr.first == id_stop.first && id_curr.second <= id_stop.second) {
                responseEntries.push_back(entry);
            }
        }
    }

    // return that subset as a RESP array
    std::string response = "*" + std::to_string(responseEntries.size()) + "\r\n";
    for (const StreamEntry &entry : responseEntries) {
        std::vector<std::string> flattenedFields{};
        for (const auto &field : entry.fields) {
            flattenedFields.push_back(field.first);
            flattenedFields.push_back(field.second);
        }

        // [id, [fields...]]
        response += "*2\r\n";
        response += ("$" + std::to_string(entry.id.length()) + "\r\n" + entry.id + "\r\n");
        response += encode_resp_array(flattenedFields);
    }

    return response;
}

std::string Store::xread(const std::vector<std::string> &keys, const std::vector<std::string> &ids, double timeout) {
    std::vector<std::pair<long long, long long>> requested_ids;

    if (timeout >= 0) {
        std::unique_lock<std::mutex> lk(mtx);
        for (size_t i = 0; i < keys.size(); i++) {
            if (ids[i] == "$") {
                auto it = data.find(keys[i]);
                if (it == data.end() || 
                !std::holds_alternative<std::vector<StreamEntry>>(it->second.value)) {
                    requested_ids.push_back({0, 0});
                    continue;
                }

                auto &stream = std::get<std::vector<StreamEntry>>(it->second.value);
                if (stream.empty()) { requested_ids.push_back({0, 0}); }
                else { requested_ids.push_back(parse_stream_id(stream.back().id)); }
            }
            else { requested_ids.push_back(parse_stream_id(ids[i])); }
        }

        auto ready = [&]() {
            for (size_t i = 0; i < keys.size(); i++) {
                auto it = data.find(keys[i]);
                if (it == data.end()) { continue; }
                if (!std::holds_alternative<std::vector<StreamEntry>>(it->second.value)) { continue; }

                auto &stream = std::get<std::vector<StreamEntry>>(it->second.value);
                if (stream.empty()) { continue; }

                auto newest_id = parse_stream_id(stream.back().id);
                if (newest_id > requested_ids[i]) { return true; }
            }
            return false;
        };

        if (timeout == 0) { cv.wait(lk, ready); }
        else { 
            bool got_ready = cv.wait_for(lk, std::chrono::duration<double, std::milli>(timeout), ready);
            if (!got_ready) { return "*-1\r\n"; }
        }
    }
    else {
        for (const std::string &id : ids) {
            requested_ids.push_back(parse_stream_id(id));
        }
    }

    std::string response = "*" + std::to_string(keys.size()) + "\r\n";
    for (size_t i = 0; i < keys.size(); i++) {
        std::pair<long long, long long> id_val = requested_ids[i];
        id_val.second++;
        response += "*2\r\n" + encode_resp_string(keys[i]);
        response += xrange(
            keys[i], 
            std::to_string(id_val.first) + "-" + std::to_string(id_val.second), 
            "+"
        );
    }
    return response;
}

std::string Store::incr(const std::string &key) {
    std::unique_lock<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        lock.unlock();
        set(key, "1");
        return ":1\r\n";
    }
    if (it->second.type != "string") {
        return "-ERR value is not an integer or out of range\r\n";
    }

    std::string &val = std::get<std::string>(it->second.value);

    // make sure that the string is a number
    try {
        size_t pos;
        long long converted = std::stoll(val, &pos);
        if (pos != val.size()) {
            return "-ERR value is not an integer or out of range\r\n";
        }

        it->second.version++;
        converted++;
        val = std::to_string(converted);
        return ":" + val + "\r\n";
    }
    catch (const std::exception &) { return "-ERR value is not an integer or out of range\r\n"; }
}

long long Store::get_version(const std::string &key) {
    std::unique_lock<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) {
        return 0;
    }
    return it->second.version;
}

void Store::read_rdb_file(const std::string &file_path) {
    // read the file contents
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) { return; }
    std::istreambuf_iterator<char> begin(file), end;
    std::string file_contents(begin, end);

    // parse the file contents
    std::vector<RdbEntry> entries = parse_rdb(file_contents);
    
    uint64_t now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    for (const RdbEntry &entry : entries) {
        if (!entry.expiry_ms.has_value()) {
            set(entry.key, entry.value);
            continue;
        }
        if (*entry.expiry_ms <= now_ms) { continue; }
        uint64_t remaining_ms = *entry.expiry_ms - now_ms;
        set_with_expiry(entry.key, entry.value, static_cast<long long>(remaining_ms));
    }
}

std::vector<std::string> Store::get_keys() {
    std::vector<std::string> keys {};
    std::ranges::copy(std::views::keys(data), std::back_inserter(keys));
    return keys;
}

std::string Store::zadd(const std::string &key, const std::string &member, double &score) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key); 
    if (it != data.end()) {
        if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return "-ERR key is not a sorted set\r\n"; }
        auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);
        
        // check if the map already has the member
        for (auto member_it = sorted_set.begin(); member_it != sorted_set.end(); ++member_it) {
            if (member_it->second == member) {
                sorted_set.erase(member_it);
                sorted_set.insert({score, member});
                return encode_resp_integer(0);
            }
        }
        sorted_set.insert({score, member});
        return encode_resp_integer(1);
    }

    data[key] = Entry{
        .value = std::set<std::pair<double, std::string>>{{score, member}},
        .type = "sorted set",
        .expiry = std::nullopt,
    };
    return encode_resp_integer(1);
}

std::string Store::zrank(const std::string &key, const std::string &member) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) { return "$-1\r\n"; }
    if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return "$-1\r\n"; }

    auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);
    
    int rank = 0;
    for (const auto &[_, name] : sorted_set) {
        if (name == member) { return encode_resp_integer(rank); }
        rank++;
    }
    return "$-1\r\n";
}

std::string Store::zrange(const std::string &key, const std::string &start, const std::string &stop) {
    std::lock_guard<std::mutex> lock(mtx);
    int start_i = std::stoi(start);
    int stop_i = std::stoi(stop);

    auto it = data.find(key);
    if (it == data.end()) { return "*0\r\n"; }
    if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return "*0\r\n"; }

    auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);

    // check for valid indices
    int size = sorted_set.size();

    // negative indices
    if (start_i < 0) { start_i = size + start_i; }
    if (stop_i < 0) { stop_i = size + stop_i; }

    // clamp values if needed
    if (start_i < 0) { start_i = 0; }
    if (stop_i >= size) { stop_i = size - 1; }

    if (start_i > stop_i) { return "*0\r\n"; }
    if (start_i >= size) { return "*0\r\n"; }
    
    std::vector<std::string> response_members;
    int i = 0;
    for (const auto &[_, name] : sorted_set) {
        if (i >= start_i && i <= stop_i) { response_members.push_back(name); }
        if (i == stop_i) { break; }
        i++;
    }
    return encode_resp_array(response_members);
}

std::string Store::zcard(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = data.find(key);
    if (it == data.end()) { return encode_resp_integer(0); }
    if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return encode_resp_integer(0); }

    auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);
    return encode_resp_integer(sorted_set.size());
}

std::string Store::zscore(const std::string &key, const std::string &member) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = data.find(key);
    if (it == data.end()) { return "$-1\r\n"; }
    if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return "$-1\r\n"; }

    auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);
    for (const auto &[score, name] : sorted_set) {
        if (name == member) { 
            std::ostringstream out;
            out << std::setprecision(17) << score;
            return encode_resp_string(out.str()); 
        }
    }
    return "$-1\r\n";
}

std::string Store::zrem(const std::string &key, const std::string &member) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = data.find(key);
    if (it == data.end()) { return encode_resp_integer(0); }
    if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return encode_resp_integer(0); }

    auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);
    for (auto member_it = sorted_set.begin(); member_it != sorted_set.end(); ++member_it) {
        if (member_it->second == member) {
            sorted_set.erase(member_it);
            return encode_resp_integer(1);
        }
    }
    return encode_resp_integer(0);
}

std::string Store::geoadd(const std::string &key, const std::string &member, const std::string &longitude, const std::string &latitude) {
    double longitude_val = std::stod(longitude);
    double latitude_val = std::stod(latitude);
    if (std::abs(longitude_val) > 180 || std::abs(latitude_val) > 85.05112878) {
        return "-ERR invalid longitude,latitude pair " + longitude + "," + latitude + "\r\n";
    }

    double score = static_cast<double>(geo_encode(latitude_val, longitude_val));
    return zadd(key, member, score);
}

std::optional<std::pair<double, double>> Store::geopos(const std::string &key, const std::string &member) {
    std::string response = zscore(key, member);
    if (response == "$-1\r\n") { return std::nullopt; }

    double response_val = parse_resp_string_to_double(response);
    return geo_decode(static_cast<uint64_t>(response_val));
}

double Store::geodist(const std::string &key, const std::string &member_one, const std::string &member_two) {
    auto pos1 = geopos(key, member_one);
    auto pos2 = geopos(key, member_two);
    if (!pos1.has_value() | !pos2.has_value()) { return -1; }
    return haversine_distance(*pos1, *pos2);
}

std::string Store::geosearch(const std::string &key, const std::pair<double, double> &center, double &radius) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> places_vec;
    
    auto it = data.find(key);
    if (it == data.end()) { return "*0\r\n"; }
    if (!std::holds_alternative<std::set<std::pair<double, std::string>>>(it->second.value)) { return "$-1\r\n"; }

    auto &sorted_set = std::get<std::set<std::pair<double, std::string>>>(it->second.value);
    for (const auto &[score, name] : sorted_set) {
        std::pair<double, double> loc_info = geo_decode(score);
        if (haversine_distance(center, loc_info) < radius) { places_vec.push_back(name); }
    }
    return encode_resp_array(places_vec);
}

std::string Store::setbit(const std::string &key, int &offset, bool val) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it != data.end()) {
        // case exists - we need to grab and modify the string
        if (!std::holds_alternative<std::string>(it->second.value)) { return "-ERR key exists and not a string.\r\n"; }
        auto &value = std::get<std::string>(it->second.value);

        // get the char to modify based on offset
        int char_pos = offset / 8;
        int bit_pos = offset % 8;
        if (char_pos >= value.length()) { 
            int to_add = char_pos - value.length() + 1;
            value.append(to_add, '\0');
         }

        // modify but return the original 
        int original_val = value[char_pos] & (1 << (7 - bit_pos));
        if (val) { value[char_pos] |= (1 << (7 - bit_pos)); } 
        else { value[char_pos] &= ~(1 << (7 - bit_pos)); }
        return encode_resp_integer(original_val);
    }

    // case new - we need to create the string
    int n_bytes = offset / 8 + 1;
    int bit_pos = offset % 8;
    std::string value(n_bytes, '\0');
    if (val) { value[value.length() - 1] |= (1 << (7 - bit_pos)); }

    data[key] = Entry{
        .value = value,
        .type = "string",
        .expiry = std::nullopt,
        .version = 1
    };
    return encode_resp_integer(0);
}

std::string Store::getbit(const std::string &key, int &offset) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) { return ":0\r\n"; }
    if (!std::holds_alternative<std::string>(it->second.value)) { return ":0\r\n"; }

    auto &value = std::get<std::string>(it->second.value);

    int char_pos = offset / 8;
    int bit_pos = offset % 8;
    if (char_pos >= value.length()) { return ":0\r\n"; }

    int val = value[char_pos] & (1 << (7 - bit_pos));
    return encode_resp_integer(val);
}

std::string Store::strlen(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end()) { return ":0\r\n"; }
    if (!std::holds_alternative<std::string>(it->second.value)) { return ":0\r\n"; }

    auto &value = std::get<std::string>(it->second.value);
    return encode_resp_integer(value.length());
}

std::string Store::bitcount(const std::string &key, int start, int stop) {
    std::lock_guard<std::mutex> lock(mtx);
    if (start > stop) { return ":0\r\n"; }

    auto it = data.find(key);
    if (it == data.end()) { return ":0\r\n"; }
    if (!std::holds_alternative<std::string>(it->second.value)) { return ":0\r\n"; }

    auto &value = std::get<std::string>(it->second.value);
    int size = value.length();
    if (stop == -1) { stop = size - 1; }
    if (start >= size || stop >= size) { return ":0\r\n"; }

    int count = 0;
    for (size_t i = start; i <= stop; ++i) {
        count += std::popcount(static_cast<unsigned char>(value[i]));
    }
    return encode_resp_integer(count);
}

std::string Store::bitop_and(const std::string &dest_key, const std::string &src_key1, const std::string &src_key2) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // obtain the source strings 
    auto it = data.find(src_key1);
    if (it == data.end()) { return ":0\r\n"; }
    if (!std::holds_alternative<std::string>(it->second.value)) { return ":0\r\n"; }

    auto it_two = data.find(src_key2);
    if (it_two == data.end()) { return ":0\r\n"; }
    if (!std::holds_alternative<std::string>(it_two->second.value)) { return ":0\r\n"; }

    auto &value_one = std::get<std::string>(it->second.value);
    auto &value_two = std::get<std::string>(it_two->second.value);

    // obtain the result
    size_t max_len = std::max(value_one.length(), value_two.length());
    std::string result;
    result.resize(max_len);
    for (size_t i = 0; i < max_len; ++i) {
        unsigned char byte_one = i < value_one.size() ? value_one[i] : 0;
        unsigned char byte_two = i < value_two.size() ? value_two[i] : 0;
        result[i] = static_cast<char>(byte_one & byte_two);
    }
    
    // check if the destination key already exists
    auto it_three = data.find(dest_key);
    if (it_three != data.end()) 
    {
        it_three->second.value = result;
        it_three->second.type = "string";
        it_three->second.version++;
    }
    else 
    {
        data[dest_key] = Entry{
            .value = result,
            .type = "string",
            .expiry = std::nullopt,
            .version = 1
        };
    }
    return encode_resp_integer(result.length());
}

std::string Store::bitop_or(const std::string &dest_key, const std::string &src_key1, const std::string &src_key2) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // obtain the source strings 
    auto it = data.find(src_key1);
    if (it == data.end()) { return ":0\r\n"; }
    if (std::holds_alternative<std::string>(it->second.value)) { return ":0\r\n"; }

    auto it_two = data.find(src_key2);
    if (it_two == data.end()) { return ":0\r\n"; }
    if (std::holds_alternative<std::string>(it_two->second.value)) { return ":0\r\n"; }

    auto &value_one = std::get<std::string>(it->second.value);
    auto &value_two = std::get<std::string>(it_two->second.value);

    // obtain the result
    size_t max_len = std::max(value_one.length(), value_two.length());
    std::string result;
    result.resize(max_len);
    for (size_t i = 0; i < max_len; ++i) {
        unsigned char byte_one = i < value_one.size() ? value_one[i] : 0;
        unsigned char byte_two = i < value_two.size() ? value_two[i] : 0;
        result[i] = static_cast<char>(byte_one | byte_two);
    }
    
    // check if the destination key already exists
    auto it_three = data.find(dest_key);
    if (it_three != data.end()) 
    {
        it_three->second.value = result;
        it_three->second.type = "string";
        it_three->second.version++;
    }
    else 
    {
        data[dest_key] = Entry{
            .value = result,
            .type = "string",
            .expiry = std::nullopt,
            .version = 1
        };
    }
    return encode_resp_integer(result.length());  
}
