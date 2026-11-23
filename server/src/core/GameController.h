#pragma once

#include "PlayerSession.h"
#include "Room.h"
#include "net/IServerHandler.h"
#include "net/TCPServer.h"
#include "protocol/Protocol.h"
#include "util/ServerConfig.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

class GameController final : public IServerHandler {
    void handle_ping(PlayerSession& session, std::string_view body);
    void handle_pong(PlayerSession& session, std::string_view body);
    void handle_login(PlayerSession& session, std::string_view body);
    void handle_room_list(PlayerSession& session, std::string_view body);
    void handle_room_make(PlayerSession& session, std::string_view body);
    void handle_room_join(PlayerSession& session, std::string_view body);
    void handle_room_leave(PlayerSession& session, std::string_view body);
    void handle_game_move(PlayerSession& session, std::string_view body);
    void handle_sync(PlayerSession& session, std::string_view body);
    //void handle_unknown_opcode(PlayerSession session, std::string_view _);

    void send(int client_fd, protocol::Opcode op, std::string_view body = "");
    void send_error(PlayerSession& session, protocol::ErrorCode code);
    void finish_game(Room& room, std::optional<char> winner_disc);

    bool is_nick_in_use(std::string_view nick) const;
    std::optional<int> parse_strict_int(std::string_view text) const;
    void register_handlers();

    TCPServer* server = nullptr; // kvuli krizove referenci TCPServer a GameController
    size_t max_rooms;
    int next_room_id = 1;

    std::unordered_map<int, PlayerSession> sessions;
    std::unordered_map<int, Room> rooms;
    std::unordered_map<protocol::Opcode, std::function<void(PlayerSession&, std::string)>> handlers;

    static constexpr auto PING_INTERVAL = std::chrono::seconds(10);
    static constexpr auto PONG_TIMEOUT = std::chrono::seconds(5);
    static constexpr auto RECONNECT_TIMEOUT = std::chrono::seconds(30);
    static constexpr int MAX_NICK_LENGTH = 20;

public:
    explicit GameController(const ServerConfig& config);

    void set_server(TCPServer* srv) noexcept { server = srv; }

    void on_client_connected(int client_fd) override;
    void on_message_recieved(int client_fd, const ParsedMessage& msg) override;
    void on_client_disconnected(int client_fd) override;
    void on_tick() override;
};