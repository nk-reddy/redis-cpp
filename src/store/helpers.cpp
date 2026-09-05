#include "helpers.h"

#include <limits>
#include <vector>
#include <format>

constexpr uint8_t RDB_TYPE_STRING = 0x00;
constexpr uint8_t RDB_OPCODE_AUX = 0xFA;
constexpr uint8_t RDB_OPCODE_RESIZEDB = 0xFB;
constexpr uint8_t RDB_OPCODE_EXPIRE_MS = 0xFC;
constexpr uint8_t RDB_OPCODE_EXPIRE_SEC = 0xFD;
constexpr uint8_t RDB_OPCODE_SELECTDB = 0xFE;
constexpr uint8_t RDB_OPCODE_EOF = 0xFF;

std::vector<std::string> parse_resp(std::string input) {
    size_t pos = 1;
    std::vector<std::string> elems;

    size_t end = input.find("\r\n", pos);
    int nelems = std::stoi(input.substr(pos, end - pos));
    if (nelems <= 0) {
        return elems;
    }

    // reach the first data val past the '$'
    pos = end + 3;
    for (size_t i = 0; i < nelems; i++) {
        size_t end = input.find("\r\n", pos);
        int bytes = std::stoi(input.substr(pos, end - pos));
        pos = end + 2;
        elems.push_back(input.substr(pos, bytes));
        pos += (bytes + 3);
    }

    return elems;
}

std::pair<long long, long long> parse_stream_id(const std::string &id, bool is_start) {
    size_t dash = id.find("-");
    long long ms = 0;
    long long seq = 0;
    if (dash == std::string::npos) {
        ms = std::stoll(id);
        seq = is_start ? 0 : std::numeric_limits<long long>::max();
    }
    else {
        ms = std::stoll(id.substr(0, dash));
        seq = std::stoll(id.substr(dash + 1));
    }
    return {ms, seq};
}

std::string encode_resp_array(const std::vector<std::string> &values) {
    std::string response = "*" + std::to_string(values.size()) + "\r\n";
    for (const std::string &str: values) {
        response += ("$" + std::to_string(str.length()) + "\r\n" + str + "\r\n");
    }
    return response;
}

std::string encode_resp_string(const std::string &str) {
    return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
}

std::string parse_hex_to_binary(const std::string &hex_str) {
    std::string result;
    if (hex_str.length() % 2 != 0 ) { return ""; }

    result.reserve(hex_str.length() / 2);
    for (size_t i = 0; i < hex_str.length(); i += 2) {
        std::string byte_str = hex_str.substr(i, 2);
        unsigned int value = std::stoul(byte_str, nullptr, 16);
        result.push_back(static_cast<char>(value));
    }
    
    return result;
}

ParseResult parse_one_resp_array(const std::string &input) {
    ParseResult result{false, {}, 0, ""};
    size_t pos = 0;

    // parse the number of elements
    if (pos >= input.size() || input[pos] != '*') { return result; }

    size_t line_end = input.find("\r\n", pos);
    if (line_end == std::string::npos) { return result; }

    int count = std::stoi(input.substr(pos + 1, line_end - (pos + 1)));

    pos = line_end + 2;
    for (int i = 0; i < count; ++i) {
        // parse each element
        // parse the bulk string (header)
        if (pos >= input.size() || input[pos] != '$') { return result; }

        line_end = input.find("\r\n", pos);
        if (line_end == std::string::npos) { return result; }

        int length = std::stoi(input.substr(pos + 1, line_end - (pos + 1)));

        // parse the bulk string (value)
        pos = line_end + 2;
        if ((pos + length + 2) > input.size()) { return result; }
        std::string value = input.substr(pos, length);
        result.args.push_back(value);
        pos += length; 
        if (input.substr(pos, 2) != "\r\n") { return result; }
        pos += 2;
    }
    result.raw_command = input.substr(0, pos);
    result.complete = true;
    result.bytes_consumed = pos;
    return result;
}

uint8_t read_byte(const std::string &data, size_t &pos) {
    if (pos >= data.size()) {
        throw std::runtime_error("Unexpected end of RDB file");
    }

    return static_cast<uint8_t>(
        static_cast<unsigned char>(data[pos++])
    );
}

uint32_t read_u32_le(const std::string &data, size_t &pos) {
    uint32_t value = 0;

    for (int i = 0; i < 4; ++i) {
        value |=
            static_cast<uint32_t>(read_byte(data, pos))
            << (8 * i);
    }

    return value;
}

uint64_t read_u64_le(const std::string &data, size_t &pos) {
    uint64_t value = 0;

    for (int i = 0; i < 8; ++i) {
        value |=
            static_cast<uint64_t>(read_byte(data, pos))
            << (8 * i);
    }

    return value;
}

uint32_t read_u32_be(const std::string &data, size_t &pos) {
    uint32_t value = 0;

    for (int i = 0; i < 4; ++i) {
        value =
            (value << 8)
            | read_byte(data, pos);
    }

    return value;
}

uint64_t read_u64_be(const std::string &data, size_t &pos) {
    uint64_t value = 0;

    for (int i = 0; i < 8; ++i) {
        value =
            (value << 8)
            | read_byte(data, pos);
    }

    return value;
}

RdbLength read_length(const std::string &data, size_t &pos) {
    uint8_t first = read_byte(data, pos);

    uint8_t type = first >> 6;

    if (type == 0b00) {
        return {
            static_cast<uint64_t>(first & 0x3F),
            false
        };
    }

    if (type == 0b01) {
        uint8_t second = read_byte(data, pos);

        uint64_t length =
            (static_cast<uint64_t>(first & 0x3F) << 8)
            | second;

        return {
            length,
            false
        };
    }

    if (first == 0x80) {
        return {
            read_u32_be(data, pos),
            false
        };
    }

    if (first == 0x81) {
        return {
            read_u64_be(data, pos),
            false
        };
    }

    if (type == 0b11) {
        return {
            static_cast<uint64_t>(first & 0x3F),
            true
        };
    }

    throw std::runtime_error("Invalid RDB length encoding");
}

std::string read_rdb_string(const std::string &data, size_t &pos) {
    RdbLength length = read_length(data, pos);

    if (!length.encoded) {
        if (pos + length.value > data.size()) {
            throw std::runtime_error(
                "RDB string extends past end of file"
            );
        }

        std::string result =
            data.substr(pos, length.value);

        pos += length.value;

        return result;
    }

    if (length.value == 0) {
        int8_t value =
            static_cast<int8_t>(read_byte(data, pos));

        return std::to_string(value);
    }

    if (length.value == 1) {
        uint16_t raw = 0;

        raw |=
            static_cast<uint16_t>(read_byte(data, pos));

        raw |=
            static_cast<uint16_t>(read_byte(data, pos))
            << 8;

        int16_t value =
            static_cast<int16_t>(raw);

        return std::to_string(value);
    }

    if (length.value == 2) {
        uint32_t raw =
            read_u32_le(data, pos);

        int32_t value =
            static_cast<int32_t>(raw);

        return std::to_string(value);
    }

    if (length.value == 3) {
        throw std::runtime_error(
            "LZF-compressed RDB strings are not supported"
        );
    }

    throw std::runtime_error(
        "Unknown RDB string encoding"
    );
}

std::vector<RdbEntry> parse_rdb(const std::string &contents) {
    std::vector<RdbEntry> entries;

    if (contents.size() < 9) {
        throw std::runtime_error(
            "RDB file is too small"
        );
    }

    if (contents.substr(0, 9) != "REDIS0011") {
        throw std::runtime_error(
            "Invalid RDB header"
        );
    }

    size_t pos = 9;

    while (pos < contents.size()) {
        uint8_t opcode =
            read_byte(contents, pos);

        if (opcode == RDB_OPCODE_EOF) {
            break;
        }

        if (opcode == RDB_OPCODE_AUX) {
            read_rdb_string(contents, pos);
            read_rdb_string(contents, pos);

            continue;
        }

        if (opcode == RDB_OPCODE_SELECTDB) {
            read_length(contents, pos);

            continue;
        }

        if (opcode == RDB_OPCODE_RESIZEDB) {
            read_length(contents, pos);
            read_length(contents, pos);

            continue;
        }

        std::optional<uint64_t> expiry_ms;

        if (opcode == RDB_OPCODE_EXPIRE_MS) {
            expiry_ms =
                read_u64_le(contents, pos);

            opcode =
                read_byte(contents, pos);
        }
        else if (opcode == RDB_OPCODE_EXPIRE_SEC) {
            uint32_t seconds =
                read_u32_le(contents, pos);

            expiry_ms =
                static_cast<uint64_t>(seconds)
                * 1000;

            opcode =
                read_byte(contents, pos);
        }

        if (opcode != RDB_TYPE_STRING) {
            throw std::runtime_error(
                "Unsupported RDB value type: "
                + std::to_string(opcode)
            );
        }

        std::string key =
            read_rdb_string(contents, pos);

        std::string value =
            read_rdb_string(contents, pos);

        entries.push_back({
            key,
            value,
            expiry_ms
        });
    }

    return entries;
}