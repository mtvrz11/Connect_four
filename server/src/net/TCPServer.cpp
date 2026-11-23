#include "TCPServer.h"
#include "util/Logger.h"
#include "util/SignalHandler.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <poll.h>

TCPServer::TCPServer(const ServerConfig& config, IServerHandler& handler)
    : handler(handler), max_clients(config.max_clients) {
        listen_socket.set_reuse_addr();
        listen_socket.set_non_blocking();
        listen_socket.bind_and_listen(config.ip, config.port);
}

void TCPServer::send_to_client(int client_fd, std::string_view raw_message) {
    auto it = clients.find(client_fd);
    if (it != clients.end())
        it->second.queue_message(raw_message);
}

void TCPServer::disconnect_client(int client_fd) {
    to_disconnect.insert(client_fd);
}

void TCPServer::run() {
    constexpr int POLL_TIMEOUT_MS = 100;

    while (signal_handler::g_running) {
        std::vector<pollfd> fds;
        fds.reserve(1 + clients.size());

        fds.push_back(pollfd{listen_socket.get_fd(), POLLIN, 0});

        for (const auto& [fd, conn] : clients) {
            short events = POLLIN;
            if (conn.has_pending_output()) {
                events |= POLLOUT;
            }
            fds.push_back(pollfd{fd, events, 0});
        }

        const int ready = ::poll(fds.data(), fds.size(), POLL_TIMEOUT_MS);
        if(ready < 0) {
            if (errno == EINTR) {
                continue; // preruseno signalem - chci ciste ukonceni smycky
            }
            throw std::runtime_error("Chyba ve funkci poll(): " + std::string(std::strerror(errno)));
        }

        if (fds[0].revents & POLLIN)
            accept_new_clients();
        
        for (size_t i = 1; i < fds.size(); i++) {
            const int fd = fds[i].fd;
            const short revents = fds[i].revents;

            if (revents == 0)
                continue; // nic se na tomto socketu nestalo
            
            auto it = clients.find(fd);
            if (it == clients.end())
                continue;
            
            // chyba socketu nebo odpojeni klienta
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                disconnect_client(fd);
                continue;
            }

            if (revents & POLLIN)
                handle_client_read(fd, it->second);

            if ((revents & POLLOUT) && to_disconnect.count(fd) == 0)
                handle_client_write(fd, it->second);
        }

        cleanup_disconnected_clients();
        handler.on_tick();
    }
}

void TCPServer::accept_new_clients() {
    while (true) {
        auto new_socket = listen_socket.accept_client();
        if (!new_socket.has_value())
            break;
        
        const int client_fd = new_socket->get_fd();

        if (clients.size() >= max_clients) {
            Logger::warn("Odmitnut novy klient (FD =" + std::to_string(client_fd) +
                         "): zapsan maximalni pocet hracu (" + std::to_string(max_clients) + ").");
            continue;
        }

        clients.emplace(client_fd, ClientConnection(std::move(*new_socket)));
        handler.on_client_connected(client_fd);
    }
}

void TCPServer::handle_client_read(int client_fd, ClientConnection& conn) {
    if (!conn.recieve_from_socket()) {
        disconnect_client(client_fd); // spojeni ukonceno
        return;
    }

    ParsedMessage msg;
    while (to_disconnect.count(client_fd) == 0) {
        const ExtractStatus status = conn.extract_message(msg);

        if (status == ExtractStatus::OK)
            handler.on_message_recieved(client_fd, msg);
        else if (status == ExtractStatus::INVALID_HEADER) {
            Logger::warn("KLient FD = " + std::to_string(client_fd) + " poslal nzpravu s nevalidni hlavickou -> bude odpojen");
            disconnect_client(client_fd);
            break;
        }
        else
            break; // NEED_MORE_DATA - cekani do dalsiho cyklu
    }
}

void TCPServer::handle_client_write(int client_fd, ClientConnection& conn) {
    if (!conn.flush_to_socket())
        disconnect_client(client_fd); // spojeni s klientem ukonceno - odpojil se
}

void TCPServer::cleanup_disconnected_clients() {
    for (int fd : to_disconnect) {
        if (clients.erase(fd) > 0) { // ochrana, aby byl smazan jen jednou, i kdyby byl vickrat behem jednoho cyklu pridan ke smazani
            handler.on_client_disconnected(fd);
        }
    }
    to_disconnect.clear();
}