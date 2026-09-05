#pragma once

#include <unordered_map>
#include <string>

std::unordered_map<std::string, std::string> parse_args(int argc, char **argv);