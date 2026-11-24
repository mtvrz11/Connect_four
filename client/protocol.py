from dataclasses import dataclass
from enum import Enum

MAGIC: bytes = b"MTVRZ"
MAGIC_LEN: int = 5
OPCODE_LEN: int = 4
LENGTH_FIELD_LEN: int = 3
HEADER_LEN: int = MAGIC_LEN + OPCODE_LEN + LENGTH_FIELD_LEN
MAX_BODY_LEN: int = 999
DELIMITER: str = ","
MAX_NICK_LEN: int = 20

class Opcode(str, Enum):
    LGIN = "LGIN"
    LRES = "LRES"
    RLST = "RLST"
    RINF = "RINF"
    RJOI = "RJOI"
    RMAK = "RMAK"
    RRES = "RRES"
    RLEA = "RLEA"
    GSTR = "GSTR"
    GMOV = "GMOV"
    GUPD = "GUPD"
    GOVR = "GOVR"
    PING = "PING"
    PONG = "PONG"
    OSTA = "OSTA"
    SYNC = "SYNC"
    SRES = "SRES"
    ERRR = "ERRR"

class ExtractStatus(Enum):
    NEED_MORE_DATA = 1
    OK = 2
    INVALID_HEADER = 3
    
@dataclass
class ParsedMessage:
    opcode: str
    body: str
    
def build_message(opcode: str, body: str = "") -> bytes:
    if len(opcode) != OPCODE_LEN:
        raise ValueError(f"Opcode musi mit presne delku {OPCODE_LEN}.")
        
    body_bytes = body.encode("ascii")
    if len(body_bytes) > MAX_BODY_LEN:
        raise ValueError(f"Telo zpravy prakracuje maximalni delku {MAX_BODY_LEN} znaku.")
    
    header = f"MTVRZ{opcode}{len(body_bytes):03d}".encode("ascii")
    return header + body_bytes

def extract_message(rx_buffer: bytearray) -> tuple[ExtractStatus, ParsedMessage | None]:
    if len(rx_buffer) < HEADER_LEN:
        return ExtractStatus.NEED_MORE_DATA, None
    
    if rx_buffer[:MAGIC_LEN] != MAGIC:
        return ExtractStatus.INVALID_HEADER, None
    
    len_bytes = rx_buffer[MAGIC_LEN + OPCODE_LEN : HEADER_LEN]
    if not len_bytes.isdigit():
        return ExtractStatus.INVALID_HEADER, None
    
    body_len = int(len_bytes.decode("ascii"))
    msg_len = HEADER_LEN + body_len
    
    if len(rx_buffer) < msg_len:
        return ExtractStatus.NEED_MORE_DATA, None
    
    try:
        opcode = rx_buffer[MAGIC_LEN : MAGIC_LEN + OPCODE_LEN].decode("ascii")
        body = rx_buffer[HEADER_LEN : msg_len].decode("ascii")
    except UnicodeDecodeError:
        return ExtractStatus.INVALID_HEADER, None
    
    del rx_buffer[:msg_len]
    return ExtractStatus.OK, ParsedMessage(opcode, body)