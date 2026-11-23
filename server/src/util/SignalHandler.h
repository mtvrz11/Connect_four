#pragma once

#include <atomic>
#include <csignal>

namespace signal_handler {
    inline std::atomic<bool> g_running{true};

    inline void handle_stop_signal(int) {
        g_running = false;
    }

    inline void setup() {
        std::signal(SIGPIPE, SIG_IGN);
        std::signal(SIGINT, handle_stop_signal);
        std::signal(SIGTERM, handle_stop_signal);
    }
}