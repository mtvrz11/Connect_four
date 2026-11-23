#include "Socket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

Socket::Socket() {
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error("Selhalo vytvoreni socketu: " + std::string(std::strerror(errno)));
}

Socket::Socket(int existing_fd) noexcept : fd(existing_fd) {}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : fd(other.fd) {
    other.fd = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd = other.fd;
        other.fd = -1;
    }
    return *this;
}

void Socket::close() noexcept {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void Socket::set_reuse_addr() {
    int opt = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("Selhalo setsockopt(SO_REUSEADDR): " + std::string(std::strerror(errno)));
}

void Socket::set_non_blocking() {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || :: fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("Selhalo nastaveni neblokujiciho rezimu: " + std::string(std::strerror(errno)));
}

void Socket::bind_and_listen(const std::string& ip, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1)
        throw std::invalid_argument("Neplatny format IPv4 adresa: " + ip);
    if(::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("Selhal bind na " + ip + ":" + std::to_string(port) + " - " + std::strerror(errno));
    if(::listen(fd, SOMAXCONN) < 0)
        throw std::runtime_error("Selhal listen: " + std::string(std::strerror(errno)));
}

std::optional<Socket> Socket::accept_client() {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = ::accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return std::nullopt;
        throw std::runtime_error("Chyba pri accept(): " + std::string(std::strerror(errno)));
    }

    Socket client_socket(client_fd);
    client_socket.set_non_blocking();
    return client_socket;
}

std::optional<std::string> Socket::recieve() {
    char buffer[4096];
    const ssize_t bytes_read = ::recv(fd, buffer, sizeof(buffer), 0);

    if (bytes_read > 0)
        return std::string(buffer, static_cast<std::size_t>(bytes_read));
    if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return std::nullopt;
    return std::string{}; // EOF nebo odpojeni klienta
}

std::optional<std::size_t> Socket::send_some(std::string_view data) {
    const ssize_t bytes_sent = ::send(fd, data.data(), data.size(), 0);

    if (bytes_sent >= 0)
        return static_cast<std::size_t>(bytes_sent);
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;
    return std::nullopt;
}

int Socket::get_fd() const noexcept {
    return static_cast<int>(fd);
}