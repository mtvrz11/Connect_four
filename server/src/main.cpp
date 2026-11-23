#include "util/ArgParser.h"
#include "util/Logger.h"
#include "util/SignalHandler.h"
#include "net/Socket.h"
#include "core/GameController.h"
#include "net/TCPServer.h"

#include <iostream>

int main(int argc, char* argv[]) {
    try {
        const auto configOpt = ArgParser::parse(argc, argv);
        if (!configOpt.has_value())
            return 0; // vypsana napoveda
        const ServerConfig& config = configOpt.value();
        
        signal_handler::setup();
        Logger::init("server.log");

        GameController controller(config);
        TCPServer server(config, controller);
        controller.set_server(&server);

        Logger::info("Server spusten na " + config.ip + ":" + std::to_string(config.port) +
                     " (max mistnosti: " + std::to_string(config.max_rooms) + ", max hracu: " + std::to_string(config.max_clients) + ")");

        server.run();

        Logger::info("Server korektne ukoncen");

    } catch (const std::exception& e) {
        std::cerr << "CHYBA: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}