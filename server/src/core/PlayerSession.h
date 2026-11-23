#pragma once

#include <chrono>
#include <optional>
#include <string>

enum class ClientState {
    UNAUTHORIZED,
    LOBBY,
    WAITING_IN_ROOM,
    ACTIVE_GAME,
    GAME_PAUSED
};

struct PlayerSession
{
    int fd = -1;
    ClientState state = ClientState::UNAUTHORIZED;
    std::string nick;

    std::optional<int> room_id = std::nullopt;

    size_t invalid_msgs_count = 0;

    std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();
    bool waiting_for_pong = false;
    std::chrono::steady_clock::time_point ping_sent_at{}; // inicializovano na "nulu"
};