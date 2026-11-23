#pragma once

#include "ClientConnection.h"
#include "IServerHandler.h"
#include "Socket.h"
#include "util/ServerConfig.h"

#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

class TCPServer {
    Socket listen_socket;
    IServerHandler& handler;
    size_t max_clients;
    std::unordered_map<int, ClientConnection> clients;
    std::unordered_set<int> to_disconnect;

    void accept_new_clients();
    void handle_client_read(int client_fd, ClientConnection& conn);
    void handle_client_write(int client_fd, ClientConnection& conn);
    void cleanup_disconnected_clients();

public:
    TCPServer(const ServerConfig& config, IServerHandler& handler);

    void run();
    void send_to_client(int client_fd, std::string_view raw_message);
    void disconnect_client(int client_fd);
};