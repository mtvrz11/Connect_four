#include "Room.h"

#include <utility>

Room::Room(int id, int creator_fd, const std::string& creator_nick) : id(id) {
    slots[0].fd = creator_fd;
    slots[0].nick = std::move(creator_nick);
    slots[0].disc = ConnectFourGame::PLAYER_1;
}

int Room::get_id() const noexcept {
    return id;
}

RoomState Room::get_state() const noexcept {
    return state;
}

int Room::occupancy() const noexcept {
    int count = 0;
    for (const auto& slot : slots) {
        if (slot.is_occupied())
            count++;
    }
    return count;
}

bool Room::join_second_player(int fd, const std::string& nick) {
    if (state != RoomState::WAITING || slots[1].is_occupied())
        return false;

    slots[1].fd = fd;
    slots[1].nick = std::move(nick);
    slots[1].disc = ConnectFourGame::PLAYER_2;

    game.reset(ConnectFourGame::PLAYER_1);
    state = RoomState::PLAYING;
    return true;
}

MoveResult Room::make_move(int fd, int column) {
    if (state != RoomState::PLAYING)
        return MoveResult::NOT_YOUR_TURN;

    const int idx = find_slot_index(fd);
    if (idx == -1) {
        return MoveResult::NOT_YOUR_TURN;
    }

    return game.drop_disc(column, slots[idx].disc);
}

std::optional<int> Room::handle_player_disconnect(int fd) {
    const int idx = find_slot_index(fd);
    if (idx == -1)
        return std::nullopt;

    slots[idx].fd = std::nullopt;
    slots[idx].disconnected_at = std::chrono::steady_clock::now();

    if (state == RoomState::PLAYING)
        state = RoomState::PAUSED;

    const int opponent_idx = (idx == 0) ? 1 : 0;
    return slots[opponent_idx].fd;
}

std::optional<int> Room::reconnect_player(int new_fd, std::string nick) {
    for (size_t i = 0; i < slots.size(); i++) {
        if (slots[i].nick == nick && !slots[i].is_connected()) {
            slots[i].fd = new_fd;
            slots[i].disconnected_at = std::nullopt;

            if (slots[0].is_connected() && slots[1].is_connected())
                state = RoomState::PLAYING;

            const int opponent_idx = (i == 0) ? 1 : 0;
            return slots[opponent_idx].fd;
        }
    }
    return std::nullopt;
}

bool Room::is_reconnect_timed_out(std::chrono::seconds limit) const {
    if (state != RoomState::PAUSED)
        return false;

    const auto now = std::chrono::steady_clock::now();
    for (const auto& slot : slots) {
        if (!slot.is_connected() && slot.disconnected_at.has_value()) {
            if ((now - *slot.disconnected_at) >= limit)
                return true;
        }
    }
    return false;
}

bool Room::contains_player_nick(std::string_view nick) const {
    return slots[0].nick == nick || slots[1].nick == nick;
}

std::optional<int> Room::get_player_fd(int index) const {
    return slots[index].fd;
}

std::string Room::get_player_nick(int index) const {
    return slots[index].nick;
}

char Room::get_player_disc(int index) const {
    return slots[index].disc;
}

std::optional<int> Room::get_opponent_fd(int fd) const {
    const int idx = find_slot_index(fd);
    if (idx == -1)
        return std::nullopt;
    
    const int opponent_idx = (idx == 0) ? 1 : 0;
    return slots[opponent_idx].fd;
}

char Room::current_turn_disc() const noexcept {
    return game.get_current_turn();
}

std::string Room::serialize_board() const {
    return game.serialize_board();
}

size_t Room::find_slot_index(int fd) const {
    for (size_t i = 0; i < slots.size(); i++) {
        if (slots[i].fd == fd)
            return i;
    }
    return -1;
}