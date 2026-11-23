#include "ClientConnection.h"

ClientConnection::ClientConnection(Socket&& skt)
    : socket(std::move(skt)) {}

int ClientConnection::fd() const noexcept {
    return socket.get_fd();
}

bool ClientConnection::has_pending_output() const noexcept {
    return !tx_buffer.empty();
}

void ClientConnection::queue_message(std::string_view raw_message) {
    tx_buffer.append(raw_message);
}

bool ClientConnection::flush_to_socket() {
    if (tx_buffer.empty())
        return true;

    auto sent = socket.send_some(tx_buffer);
    if (!sent.has_value())
        return false; // spojeni spadlo
    
    if(sent.value() > 0)
        tx_buffer.erase(0, sent.value());
    return true;
}

bool ClientConnection::recieve_from_socket() {
    auto chunk = socket.recieve();
    if (!chunk.has_value())
        return true; // nejsou tam dalsi data
    if (chunk.value().empty())
        return false; // klient se odpojil nebo chyba site
    
    rx_buffer += chunk.value();
    return true;
}

ExtractStatus ClientConnection::extract_message(ParsedMessage& out_message) {
    if (rx_buffer.size() < protocol::HEADER_LEN)
        return ExtractStatus::NEED_MORE_DATA;

    std::string_view view(rx_buffer);

    if (view.substr(0, protocol::MAGIC_LEN) != protocol::MAGIC)
        return ExtractStatus::INVALID_HEADER;

    std::string_view len_str = view.substr(protocol::MAGIC_LEN + protocol::OPCODE_LEN, protocol::LENGTH_FIELD_LEN);
    size_t processed_chars = 0;
    int body_len = 0;
    std::size_t body_len = 0;
    for (char ch : len_str) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return ExtractStatus::INVALID_HEADER;
        }
        body_len = body_len * 10 + static_cast<std::size_t>(ch - '0');
    }
    
    const int msg_len = protocol::HEADER_LEN + body_len;
    if (rx_buffer.size() < msg_len)
        return ExtractStatus::NEED_MORE_DATA;

    std::string_view opcode_str = view.substr(protocol::MAGIC_LEN, protocol::OPCODE_LEN);
    out_message.opcode = protocol::parse_opcode(opcode_str);
    out_message.body = rx_buffer.substr(protocol::HEADER_LEN, body_len);

    rx_buffer.erase(0, msg_len);
    return ExtractStatus::OK;
}