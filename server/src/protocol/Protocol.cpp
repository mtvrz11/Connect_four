#include "Protocol.h"

#include <iomanip>
#include <stdexcept>
#include <sstream>


namespace protocol {
    
    // anonymni namespace - viditelnost jen v ramci tohoto souboru
    namespace {
        struct OpcodeEntry {
            Opcode op;
            std::string_view text;
        };

        constexpr OpcodeEntry OPCODE_TABLE[] = {
            {Opcode::LGIN, "LGIN"},
            {Opcode::LRES, "LRES"},
            {Opcode::RLST, "RLST"},
            {Opcode::RINF, "RINF"},
            {Opcode::RJOI, "RJOI"},
            {Opcode::RMAK, "RMAK"},
            {Opcode::RRES, "RRES"},
            {Opcode::RLEA, "RLEA"},
            {Opcode::GSTR, "GSTR"},
            {Opcode::GMOV, "GMOV"},
            {Opcode::GUPD, "GUPD"},
            {Opcode::GOVR, "GOVR"},
            {Opcode::PING, "PING"},
            {Opcode::PONG, "PONG"},
            {Opcode::OSTA, "OSTA"},
            {Opcode::SYNC, "SYNC"},
            {Opcode::SRES, "SRES"},
            {Opcode::ERRR, "ERRR"}
        };
    }

    Opcode parse_opcode(std::string_view code) {
        for (const auto& entry : OPCODE_TABLE) {
            if (code == entry.text)
                return entry.op;
        }
        return Opcode::UNKNOWN;
    }

    std::string_view opcode_to_str(Opcode op) {
        for (const auto& entry : OPCODE_TABLE) {
            if (op == entry.op)
                return entry.text;
        }
        return "ERRR";
    }

    std::string_view error_to__str(ErrorCode err) {
        switch (err) {
            case ErrorCode::RULE: return "RULE";
            case ErrorCode::BNDS: return "BNDS";
            case ErrorCode::TURN: return "TURN";
            case ErrorCode::INVL: return "INVL";
        }
        return "INVL";
    }

    std::string build_message(Opcode op, std::string_view body) {
        if (body.size() > MAX_BODY_LEN)
            throw std::invalid_argument("Telo zpravy prekracuje maximalni delku.");
        
        std::ostringstream oss;
        oss << MAGIC << opcode_to_str(op) << std::setw(LENGTH_FIELD_LEN) << std::setfill('0') << body.size() << body;
        return oss.str();
    }

}