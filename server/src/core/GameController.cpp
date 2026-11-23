#include "GameController.h"
#include "util/Logger.h"

#include <climits>
#include <cctype>

GameController::GameController(const ServerConfig& config)
    : max_rooms(config.max_rooms) {
    register_handlers();
}

void GameController::send(int client_fd, protocol::Opcode op, std::string_view body) {
    if (server)
        server->send_to_client(client_fd, protocol::build_message(op, body));
}

void GameController::send_error(PlayerSession& session, protocol::ErrorCode code) {
    send(session.fd, protocol::Opcode::ERRR, protocol::error_to__str(code));
    session.invalid_msgs_count++;

    Logger::warn("Poslano ERRR pro FD = " + std::to_string(session.fd) +
                 "(" + std::to_string(session.invalid_msgs_count) + "/" + std::to_string(protocol::MAX_INVALID_MSGS) + ")");

    if (session.invalid_msgs_count >= protocol::MAX_INVALID_MSGS) {
        Logger::warn("Klient FD = " + std::to_string(session.fd) + " prekrocil limit chyb -> bude odpojen");
        if (server) {
            server->disconnect_client(session.fd);
        }
    }
}

void GameController::finish_game(Room& room, std::optional<char> winner_disc) {
    const int room_id = room.get_id();

    for (int i = 0; i < 2; i++) {
        auto fd_opt = room.get_player_fd(i);
        if (!fd_opt.has_value())
            continue;
        
        const int fd = *fd_opt;
        if (!winner_disc.has_value())
            send(fd, protocol::Opcode::GOVR, "DRAW");
        else if (*winner_disc == room.get_player_disc(i))
            send(fd, protocol::Opcode::GOVR, "WIN");
        else
            send(fd, protocol::Opcode::GOVR, "LOSE");

        auto s_it = sessions.find(fd);
        if (s_it != sessions.end()) {
            s_it->second.state = ClientState::LOBBY;
            s_it->second.room_id = std::nullopt;
        }
    }

    Logger::info("Hra v mistnosti ID = " + std::to_string(room_id) + " skoncila");
    rooms.erase(room_id);
}

bool GameController::is_nick_in_use(std::string_view nick) const {
    for (const auto& [fd, session] : sessions) {
        if (session.nick == nick)
            return true;
    }
    for (const auto& [id, room] : rooms) { // i kdyz neni hrac pripojen, jeho prezdivka zustava rezervovana, kdyby se chtel vratit
        if (room.contains_player_nick(nick))
            return true;
    }
    return false;
}

std::optional<int> GameController::parse_strict_int(std::string_view text) const {
    if (text.empty())
        return std::nullopt;

    int value = 0;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c)) || value > (INT_MAX - 9) / 10) // ochrana proti preteceni
            return std::nullopt;
        
        value = value * 10 + (c - '0');
    }
    return value;
}

void GameController::on_client_connected(int client_fd) {
    PlayerSession session;
    session.fd = client_fd;
    session.state = ClientState::UNAUTHORIZED;
    session.last_activity = std::chrono::steady_clock::now();

    sessions.emplace(client_fd, std::move(session));
    Logger::info("Nova relace vytvorena pro FD = " + std::to_string(client_fd));
}

void GameController::on_message_recieved(int client_fd, const ParsedMessage& msg) {
    auto it = sessions.find(client_fd);
    if (it == sessions.end())
        return;

    PlayerSession& session = it->second;
    session.last_activity = std::chrono::steady_clock::now();

    auto h_it = handlers.find(msg.opcode);
    if (h_it == handlers.end()) {
        Logger::warn("Neplatny opcode od FD = " + std::to_string(client_fd));
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    h_it->second(session, msg.body);
}

void GameController::on_client_disconnected(int client_fd) {
    auto it = sessions.find(client_fd);
    if (it == sessions.end())
        return;
    
    const PlayerSession session = it->second;
    sessions.erase(it);

    Logger::info("Odpojen klient FD = " + std::to_string(client_fd) + (session.nick.empty() ? "" : " (" + session.nick + ")"));

    if (!session.room_id.has_value())
        return;
    
    auto r_it = rooms.find(*session.room_id);
    if (r_it == rooms.end())
        return;
    
    Room& room = r_it->second;

    if (room.get_state() == RoomState::WAITING) {
        Logger::info("Po odpojeni hrace ZRUSENA MISTNOST ID = " + std::to_string(room.get_id()));
        rooms.erase(r_it);
    }
    else if (room.get_state() == RoomState::PLAYING || room.get_state() == RoomState::PAUSED) {
        auto opponent_fd = room.handle_player_disconnect(client_fd);
        if (opponent_fd.has_value()) {
            auto o_it = sessions.find(*opponent_fd);
            if (o_it != sessions.end()) {
                o_it->second.state = ClientState::GAME_PAUSED;
                send(*opponent_fd, protocol::Opcode::OSTA, "DISC");
            }
        }
    }
}

void GameController::on_tick() {
    const auto now = std::chrono::steady_clock::now();

    for (auto& [fd, session] : sessions) {
        if (session.waiting_for_pong) {
            if ((now - session.ping_sent_at) >= PONG_TIMEOUT) {
                Logger::warn("Klient FD = " + std::to_string(fd) + " neodpovedel na PING -> byl odpojen");
                if (server)
                    server->disconnect_client(fd);
            }
        }
        else if ((now - session.last_activity) >= PING_INTERVAL) {
            send(fd, protocol::Opcode::PING);
            session.waiting_for_pong = true;
            session.ping_sent_at = now;
        }
    }

    std::vector<int> expired_rooms;
    for (auto& [rooom_id, room] : rooms) {
        if (room.is_reconnect_timed_out(RECONNECT_TIMEOUT)) {
            Logger::info("Vyprsel limit na navrat v mistnosti ID = " + std::to_string(rooom_id) + " -> mistnost bude zrusena.");

            for (int i = 0; i < 2; i++) {
                auto player_fd = room.get_player_fd(i);
                if (player_fd.has_value()) {
                    auto s_it = sessions.find(*player_fd);
                    if (s_it != sessions.end()) {
                        s_it->second.state = ClientState::LOBBY;
                        s_it->second.room_id = std::nullopt;
                        send(*player_fd, protocol::Opcode::OSTA, "DROP");
                    }
                }
            }
            expired_rooms.push_back(rooom_id);
        }
    }

    for(int id : expired_rooms) {
        rooms.erase(id);
    }
}