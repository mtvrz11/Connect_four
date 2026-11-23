#pragma once

#include <string>
#include <string_view>

namespace protocol {
    constexpr std::string_view MAGIC = "MTVRZ";
    constexpr size_t MAGIC_LEN = MAGIC.size();
    constexpr size_t OPCODE_LEN = 4;
    constexpr size_t LENGTH_FIELD_LEN = 3;
    constexpr size_t HEADER_LEN = MAGIC_LEN + OPCODE_LEN + LENGTH_FIELD_LEN;
    constexpr size_t MAX_BODY_LEN = 999;
    constexpr char DELIMITER = ',';
    constexpr size_t MAX_INVALID_MSGS = 3;

    enum class Opcode {
        LGIN, LRES,
        RLST, RINF, RJOI, RMAK, RRES, RLEA,
        GSTR, GMOV, GUPD, GOVR,
        PING, PONG, OSTA, SYNC, SRES,
        ERRR,
        UNKNOWN
    };

    enum class ErrorCode {
        RULE,
        BNDS,
        TURN,
        INVL
    };

    Opcode parse_opcode(std::string_view code);
    std::string_view opcode_to_str(Opcode op);
    std::string_view error_to__str(ErrorCode err);

    std::string build_message(Opcode op, std::string_view body = "");
}