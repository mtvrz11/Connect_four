#include "Logger.h"

#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <chrono>

void Logger::init(const std::string& file_path) {
    log_file.open(file_path, std::ios::out | std::ios::app);
    if (!log_file.is_open())
        std::cerr << "Nepodarilo se otevrit logovaci soubor " << file_path << std::endl;
}

void Logger::info(std::string_view msg) {
    log(Level::INFO, msg);
}

void Logger::warn(std::string_view msg) {
    log(Level::WARN, msg);
}

void Logger::error(std::string_view msg) {
    log(Level::ERROR, msg);
}

void Logger::log(Level level, std::string_view message) {
    std::string_view levelText = "INFO";
    if (level == Level::WARN)
        levelText = "WARN";
    else if (level == Level::ERROR)
        levelText = "ERROR";

    std::ostringstream line;
    line << "[" << currentTimestamp() << "] [" << levelText << "] " << message << std::endl;
    const std::string formatted = line.str();

    std::ostream& console = (level == Level::ERROR) ? std::cerr : std::cout;
    console << formatted;
    console.flush();
    
    if (log_file.is_open()) {
        log_file << formatted;
        log_file.flush();
    }
}

std::string Logger::currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm* localTm = std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(localTm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}