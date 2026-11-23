#pragma once

#include <string>
#include <cstdint>

struct ServerConfig {
    std::string ip = "0.0.0.0";
    uint16_t port = 10000;
    std::size_t max_rooms = 10;
    std::size_t max_clients = 20;
};