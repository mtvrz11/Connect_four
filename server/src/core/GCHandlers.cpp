#include "GameController.h"
#include "util/Logger.h"

#include <sstream>

void GameController::register_handlers() {
    handlers[protocol::Opcode::PING] = [this](PlayerSession& s, std::string_view b) { handle_ping(s, b); };
    handlers[protocol::Opcode::PONG] = [this](PlayerSession& s, std::string_view b) { handle_pong(s, b); };
    handlers[protocol::Opcode::LGIN] = [this](PlayerSession& s, std::string_view b) { handle_login(s, b); };
    handlers[protocol::Opcode::RLST] = [this](PlayerSession& s, std::string_view b) { handle_room_list(s, b); };
    handlers[protocol::Opcode::RMAK] = [this](PlayerSession& s, std::string_view b) { handle_room_make(s, b); };
    handlers[protocol::Opcode::RJOI] = [this](PlayerSession& s, std::string_view b) { handle_room_join(s, b); };
    handlers[protocol::Opcode::RLEA] = [this](PlayerSession& s, std::string_view b) { handle_room_leave(s, b); };
    handlers[protocol::Opcode::GMOV] = [this](PlayerSession& s, std::string_view b) { handle_game_move(s, b); };
    handlers[protocol::Opcode::SYNC] = [this](PlayerSession& s, std::string_view b) { handle_sync(s, b); };
}

void GameController::handle_ping(PlayerSession& session, std::string_view body) {
    if (!body.empty()) {
            send_error(session, protocol::ErrorCode::INVL);
            return;
        }
        send(session.fd, protocol::Opcode::PONG);
        return;
}

void GameController::handle_pong(PlayerSession& session, std::string_view body) {
    if (!body.empty()) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }
    session.waiting_for_pong = false;
}

void GameController::handle_login(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::UNAUTHORIZED) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }

    auto is_valid_nick = [](std::string_view nick) -> bool {
        if (nick.empty() || nick.size() > MAX_NICK_LENGTH)
            return false;
        for (unsigned char c : nick) {
            if (c < 33 || c > 126 || c == static_cast<unsigned char>(protocol::DELIMITER))
                return false;
        }
        return true;
    };

    if (is_valid_nick(body)) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    if (is_nick_in_use(body)) {
        send(session.fd, protocol::Opcode::LRES, "TAKN");
        return;
    }

    session.nick = std::string(body);
    session.state = ClientState::LOBBY;
    Logger::info("Klient FD = " + std::to_string(session.fd) + "prihlasen jako: " + session.nick);
    send(session.fd, protocol::Opcode::LRES, "OK");
}

void GameController::handle_room_list(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::LOBBY) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }
    
    if (!body.empty()) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    std::ostringstream oss;
    bool first = true;
    for (const auto& [id, room] : rooms) {
        if (!first)
            oss << protocol::DELIMITER;
        oss << id << protocol::DELIMITER << room.occupancy();
        first = false;
    }

    send(session.fd, protocol::Opcode::RINF, oss.str());
}

void GameController::handle_room_make(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::LOBBY) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }

    if (!body.empty()) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    if (rooms.size() >= max_rooms) {
        send(session.fd, protocol::Opcode::RRES, "FULL"); // server je plny - neni to chyba klienta
        return;
    }

    const int room_id = next_room_id;
    next_room_id++;
    rooms.emplace(room_id, Room(room_id, session.fd, session.nick));

    session.room_id = room_id;
    session.state = ClientState::WAITING_IN_ROOM;

    Logger::info("Hrac " + session.nick + " vytvortil mistnost ID = " + std::to_string(room_id));
    send(session.fd,  protocol::Opcode::RRES, std::to_string(room_id));
}

void GameController::handle_room_join(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::LOBBY) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }

    const auto room_id_opt = parse_strict_int(body);
    if (!room_id_opt.has_value()) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    auto r_it = rooms.find(*room_id_opt);
    if (r_it == rooms.end()) {
        send_error(session, protocol::ErrorCode::BNDS);
        return;
    }

    Room& room = r_it->second;
    if (!room.join_second_player(session.fd, session.nick)) {
        send_error(session, protocol::ErrorCode::RULE);
        return;
    }

    session.room_id = room.get_id();
    send(session.fd, protocol::Opcode::RRES, std::to_string(room.get_id()));

    const int p1_fd = *room.get_player_fd(0);
    const int p2_fd = *room.get_player_fd(1);

    sessions[p1_fd].state = ClientState::ACTIVE_GAME;
    sessions[p2_fd].state = ClientState::ACTIVE_GAME;

    const std::string p1_gstr = std::string("R,") + room.get_player_nick(1) + ",1";
    const std::string p2_gstr = std::string("B,") + room.get_player_nick(0) + ",0";

    send(p1_fd, protocol::Opcode::GSTR, p1_gstr);
    send(p2_fd, protocol::Opcode::GSTR, p2_gstr);

    Logger::info("Hra zahajena v mistnosti ID = " + std::to_string(room.get_id()) +
                 " (" + room.get_player_nick(0) + " vs " + room.get_player_nick(1) + ")");
}

void GameController::handle_room_leave(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::WAITING_IN_ROOM || !session.room_id.has_value()) { // kdybych chtel povolit odchod z rozehrane hry, asi bych hru ukoncil jako kontumacni vyhru soupere... Podle diagramu se ale leave da volat jen z lobby, tak to tak asi necham...
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }

    if (!body.empty()) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    const int room_id = *session.room_id;
    rooms.erase(room_id);

    session.room_id = std::nullopt;
    session.state = ClientState::LOBBY;
    Logger::info("Hrac " + session.nick + " opustil mistnost ID = " + std::to_string(room_id));
}

void GameController::handle_game_move(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::ACTIVE_GAME || !session.room_id.has_value()) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }

    const auto col_opt = parse_strict_int(body);
    if (!col_opt.has_value()) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    auto r_it = rooms.find(*session.room_id);
    if (r_it == rooms.end()) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }

    Room& room = r_it->second;
    const MoveResult res = room.make_move(session.fd, *col_opt);

    if (res == MoveResult::NOT_YOUR_TURN) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }
    if (res == MoveResult::OUT_OF_BOUNDS) {
        send_error(session, protocol::ErrorCode::BNDS);
        return;
    }
    if (res == MoveResult::COLUMN_FULL) {
        send_error(session, protocol::ErrorCode::RULE);
        return;
    }

    const std::string gupd_body = std::to_string(*col_opt) + "," + std::string(1, room.current_turn_disc());
    for (int i = 0; i < 2; i++) {
        if (auto fd = room.get_player_fd(i)) {
            send(*fd, protocol::Opcode::GUPD, gupd_body);
        }
    }

    if (res == MoveResult::WIN)
        finish_game(room, room.current_turn_disc());
    else if (res == MoveResult::DRAW)
        finish_game(room, std::nullopt);
}   

void GameController::handle_sync(PlayerSession& session, std::string_view body) {
    if (session.state != ClientState::UNAUTHORIZED) {
        send_error(session, protocol::ErrorCode::TURN);
        return;
    }
    if (body.empty() || body.find(protocol::DELIMITER) != std::string::npos) {
        send_error(session, protocol::ErrorCode::INVL);
        return;
    }

    for (auto& [room_id, room] : rooms) {
        if (room.get_state() != RoomState::PAUSED || !room.contains_player_nick(body))
            continue;
        
        auto res = room.reconnect_player(session.fd, body);
        if(!res.has_value())
            continue;
            
        session.nick = std::string(body);
        session.room_id = room_id;

        const std::string stres_body = room.serialize_board() + protocol::DELIMITER
                                       + room.current_turn_disc() + protocol::DELIMITER
                                       + res->player_disc + protocol::DELIMITER
                                       + res->opponent_nick;
        send(session.fd, protocol::Opcode::SRES, stres_body);

        if (res->opponent_fd.has_value()) {
            session.state = ClientState::ACTIVE_GAME;
            auto o_it = sessions.find(*res->opponent_fd);
            if (o_it != sessions.end()) {
                o_it->second.state = ClientState::ACTIVE_GAME;
                send(*res->opponent_fd, protocol::Opcode::OSTA, "CONN");
            }
        }
        else {
            session.state = ClientState::GAME_PAUSED;
            send(session.fd, protocol::Opcode::OSTA, "DISC");
        }

        Logger::info("Hrac " + session.nick + " se znovu pripojil do mistnosti ID = " + std::to_string(room_id));
        return;
    }

    send(session.fd, protocol::Opcode::SRES, "NOTF");
}