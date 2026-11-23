#include "ArgParser.h"

#include <stdexcept>
#include <vector>
#include <iostream>

std::optional<ServerConfig> ArgParser::parse(int argc, char* argv[]) {
    ServerConfig config;

    std::vector<std::string> args(argv + 1, argv + argc);

    for (size_t i = 0; i < args.size(); i++) {
        const std::string flag = args[i];

        if (flag == "-h") {
            printHelp();
            return std::nullopt;
        }

        // ostatni prepinace potrabuji jeste jeden argument
        if (i + 1 >= args.size())
            throw std::invalid_argument("Cybi hodnota parametru " + flag);
        
        const std::string value = args[++i];

        if (flag == "-i") {
            if (value.empty())
                throw std::invalid_argument("IP adresa musi byt zadana.");
            config.ip = value;
        }
        else if (flag == "-p") {
            int port = parseInt(value, "port");
            if (port < 1024 || port > 65535)
                throw std::invalid_argument("Port musi byt cislo v rozsahu 1024 az 65535.");
            config.port = static_cast<uint16_t>(port);
        }
        else if (flag == "-r") {
            int rooms = parseInt(value, "pocet mistnosti");
            if (rooms < 1)
                throw std::invalid_argument("Pocet mistnosti musi byt minimalne 1.");
            config.max_rooms = static_cast<size_t>(rooms);
        }
        else if (flag == "-c") {
            int clients = parseInt(value, "pocet klientu");
            if (clients < 2)
                throw std::invalid_argument("Pocet klientu musi byt minimalne 2.");
            config.max_clients = static_cast<size_t>(clients);
        }
        else
            throw std::invalid_argument("Parametr \"" + flag + "\" je neplatny. Pouzij -h pro napovedu.");
    }

    return config;
}

int ArgParser::parseInt(const std::string& str, const std::string& param_name) {
    size_t processed_chars = 0;
    int number = 0;

    try {
        number = std::stoi(str, &processed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument("Parametr \"" + param_name + "\" musi byt cele cislo.");
    }

    if (processed_chars != str.size())
        throw std::invalid_argument("Parametr \"" + param_name + "\" nesmi obsahovat jine znaky nez cislice.");

    return number;
}

void ArgParser::printHelp() {
    std::cout << "Pouziti: ./server [PARAMETRY]\n"
              << "  -i <adresa>   Naslouchaci IPv4 adresa (vychozi: 0.0.0.0)\n"
              << "  -p <port>     Naslouchaci port 1024-65535 (vychozi: 10000)\n"
              << "  -r <pocet>    Maximalni pocet mistnosti (vychozi: 10)\n"
              << "  -c <pocet>    Maximalni celkovy pocet klientu (vychozi: 20)\n"
              << "  -h            Zobrazi tuto napovedu\n";
}