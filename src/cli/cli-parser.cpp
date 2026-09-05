#include "cli-parser.h"

std::unordered_map<std::string, std::string> parse_args(int argc, char **argv) {
    std::unordered_map<std::string, std::string> args;
    for (int i = 0; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--replicaof" && i + 2 < argc) {
            args[key] = std::string(argv[i + 1]) + " " + std::string(argv[i + 2]);
            i += 2;
            continue;
        }
        if (key.starts_with("--") && i + 1 < argc) {
            args[key] = argv[i + 1];
            ++i;
        }
    }
    return args;
}