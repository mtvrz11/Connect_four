#pragma once

#include "ClientConnection.h"

class IServerHandler {
public:
    virtual ~IServerHandler() = default;

    virtual void on_client_connected(int client_fd) = 0;
    virtual void on_message_recieved(int client_fd, const ParsedMessage& msg) = 0;
    virtual void on_client_disconnected(int client_fd) = 0;
    virtual void on_tick() = 0;
};