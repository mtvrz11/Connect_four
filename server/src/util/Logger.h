#pragma once

#include <fstream>
#include <string>
#include <string_view>

class Logger {
    static inline std::ofstream log_file;

    enum class Level { INFO, WARN, ERROR };
    static void log(Level level, std::string_view message);
    static std::string currentTimestamp();

public:
    Logger() = delete;

    static void init(const std::string& file_path = "server.log");
    static void info(std::string_view msg);
    static void warn(std::string_view msg);
    static void error(std::string_view msg);
};