#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

class Socket {
    explicit Socket(int existing_fd) noexcept;
    int fd = -1;

public:
    Socket();
    ~Socket();

    Socket(const Socket& _) = delete;
    Socket& operator=(const Socket& _) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    void set_reuse_addr();
    void set_non_blocking();
    void bind_and_listen(const std::string& ip, uint16_t port);

    std::optional<Socket> accept_client();
    std::optional<std::string> recieve();
    std::optional<size_t> send_some(std::string_view data);

    int get_fd() const noexcept;
    void close() noexcept;
};