#pragma once

#include "ServerConfig.h"
#include <optional>
#include <string>

class ArgParser {
    static void printHelp();
    static int parseInt(const std::string& str, const std::string& param_name);

public:
    static std::optional<ServerConfig> parse(int argc, char* argv[]);
};