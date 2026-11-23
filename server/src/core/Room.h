#pragma once

#include "game/ConnectFourGame.h"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

enum class RoomState {
    WAITING,
    PLAYING,
    PAUSED
};

struct RoomPlayerSlot {
    std::optional<int> fd = std::nullopt;
    std::string nick;
    char disc = ConnectFourGame::EMPTY;
    std::optional<std::chrono::steady_clock::time_point> disconnected_at = std::nullopt;

    bool is_occupied() const noexcept { return !nick.empty(); }
    bool is_connected() const noexcept { return fd.has_value(); }
};

class Room {
    size_t find_slot_index(int fd) const;

    int id;
    RoomState state = RoomState::WAITING;
    std::array<RoomPlayerSlot, 2> slots{};
    ConnectFourGame game;

public:
    Room(int id, int creator_fd, const std::string& creator_nick);

    int get_id() const noexcept;
    RoomState get_state() const noexcept;
    int occupancy() const noexcept;

    bool join_second_player(int fd, const std::string& nick);

    MoveResult make_move(int fd, int column);

    std::optional<int> handle_player_disconnect(int fd);

    std::optional<int> reconnect_player(int new_fd, std::string nick);

    bool is_reconnect_timed_out(std::chrono::seconds limit) const;

    bool contains_player_nick(std::string_view nick) const;
    std::optional<int> get_player_fd(int index) const;
    std::string get_player_nick(int index) const;
    char get_player_disc(int index) const;
    std::optional<int> get_opponent_fd(int fd) const;

    char current_turn_disc() const  noexcept;
    std::string serialize_board() const;
};