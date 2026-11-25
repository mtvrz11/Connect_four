import queue
import socket
import threading
import time
import logging
from dataclasses import dataclass

from protocol import ExtractStatus, ParsedMessage, build_message, extract_message, Opcode

PING_INTERVAL: float = 10.0
PONG_TIMEOUT:float = 5.0

@dataclass
class NetworkEvent:
    kind: str
    message: ParsedMessage | None = None
    info: str = ""
    
class NetworkClient:
    def __init__(self) -> None:
        self._sock: socket.socket | None = None
        self._send_lock = threading.Lock()
        self._running = False
        self._recv_thread: threading.Thread | None = None
        self._last_rx_time: float = 0.0
        self._waiting_for_pong: bool = False
        self._ping_sent_time: float = 0.0
        self.incoming_queue: queue.Queue[NetworkEvent] = queue.Queue()
        
    @property
    def is_connected(self) -> bool:
        return self._running and self._sock is not None
    
    def connect(self, host:str, port: int, timeout: float = 10.0) -> None:
        self.disconnect()
        
        logging.info(f"Pripojovani k serveru {host}:{port}")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout) # jen ochrana proti dlouhemu cekani pri zadani spatne adresy
        sock.connect((host, port))
        
        sock.settimeout(1.0) # pro cteni mensi timeout...
        
        self._sock = sock
        self._last_rx_time = time.monotonic()
        self._waiting_for_pong = False
        self._ping_sent_time = 0.0
        self._running = True
        self._recv_thread = threading.Thread(target=self._recieve_loop, daemon=True)
        self._recv_thread.start()
        logging.info("Spojeni navazano ({host}:{port})")
        
    def disconnect(self):
        was_running = self._running
        self._running = False
        self._waiting_for_pong = False
        with self._send_lock:
            if self._sock is not None:
                try:
                    self._sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                self._sock.close()
                self._sock = None
        if was_running:
            logging.info("Spojeni ukonceno.")
    
    def send_message(self, opcode: Opcode, body: str = "") -> None:
        raw = build_message(opcode, body)
        with self._send_lock:
            if not self._running or self._sock is None:
                return
            try:
                self._sock.sendall(raw)
            except OSError as exc:
                self._running = False
                logging.warning(f"Chyba pri odesilani zpravy: {exc}")
                self.incoming_queue.put(NetworkEvent(kind="DISCONNECTED", info="Ztrata spojeni se serverem"))
    
    def _recieve_loop(self) -> None:
        rx_buffer = bytearray()
        
        while self._running and self._sock is not None:
            try:
                chunk = self._sock.recv(4096)
                if not chunk:
                    if self._running:
                        self._running = False
                        logging.warning("Server ukoncil spojeni (EOF)")
                        self.incoming_queue.put(NetworkEvent(kind="DISCONNECTED", info="Server ukoncil spojeni"))
                    break
                
                self._last_rx_time = time.monotonic()
                rx_buffer.extend(chunk)
                
                while self._running:
                    status, msg = extract_message(rx_buffer)
                    if status == ExtractStatus.OK and msg is not None:
                        if msg.opcode == Opcode.PING:
                            self.send_message(Opcode.PONG)
                        elif msg.opcode == Opcode.PONG:
                            self._waiting_for_pong = False
                        else:
                            self.incoming_queue.put(NetworkEvent("MESSAGE", msg))
                    elif status == ExtractStatus.INVALID_HEADER:
                        self._running = False
                        logging.error("Prijata zprava s nevalidni hlavickou ze serveru.")
                        self.incoming_queue.put(NetworkEvent(kind="ERROR", info="Server poslal nevalidni hlavicku protokolu."))
                        break
                    else:
                        # need more data
                        break
                
            except socket.timeout:
                now = time.monotonic()
                if self._waiting_for_pong:
                    if (now - self._ping_sent_time) >= PONG_TIMEOUT:
                        self._running = False
                        logging.warning("Vyprsel cas cekani na PONG ze serveru.")
                        self.incoming_queue.put(NetworkEvent(kind="DISCONNECTED", info="Server neodpovedel na PING"))
                        break
                elif (now - self._last_rx_time) >= PING_INTERVAL:
                    self.send_message(Opcode.PING)
                    self._waiting_for_pong = True
                    self._ping_sent_time = now
                continue
            except OSError as exc:
                if self._running:
                    self._running = False
                    logging.warning(f"Sitova chyba pri cteni: {exc}")
                    self.incoming_queue.put(NetworkEvent(kind="DISCONNECTED", info="Spojeni se serverem bylo ztraceno"))
                break