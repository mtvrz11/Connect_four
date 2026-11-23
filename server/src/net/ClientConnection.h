#pragma once

#include "Socket.h"
#include "protocol/Protocol.h"

#include <string>
#include <string_view>
#include <cstdint>

struct ParsedMessage {
    protocol::Opcode opcode = protocol::Opcode::UNKNOWN;
    std::string body;
};

enum class ExtractStatus {
    NEED_MORE_DATA,
    OK,
    INVALID_HEADER
};

class ClientConnection {
    Socket socket;
    std::string rx_buffer;
    std::string tx_buffer;

public:
    explicit ClientConnection(Socket&& skt);

    int fd() const noexcept;

    bool recieve_from_socket();

    ExtractStatus extract_message(ParsedMessage& out_message);

    void queue_message(std::string_view raw_message);

    bool has_pending_output() const noexcept;

    bool flush_to_socket();
};