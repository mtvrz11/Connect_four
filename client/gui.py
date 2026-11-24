import queue
import tkinter as tk
from collections.abc import Callable
from tkinter import messagebox

from network import NetworkClient, NetworkEvent
from protocol import DELIMITER, MAX_NICK_LEN, Opcode
from views import GameView, LoginView, LobbyView, PLAYER1_DISC, PLAYER2_DISC

class ConnectFourGUI:
    root: tk.Tk
    net: NetworkClient
    status_var: tk.StringVar
    
    login_view: LoginView
    lobby_view: LobbyView
    game_view: GameView
    
    nickname: str
    my_disc: str
    opponent_nick: str
    my_turn: bool
    game_paused: bool
    in_waiting_room: bool
    _handlers: dict[str, Callable[[str], None]]
    
    def __init__(self, root: tk.Tk, default_host: str = "127.0.0.1", default_port: int = 10000) -> None:
        self.root = root
        self.root.title("Connect Four")
        self.root.resizable(False, False)
        
        self.net = NetworkClient()
        
        self.nickname: str = ""
        self.my_disc: str = ""
        self.opponent_nick: str = ""
        self.my_turn: bool = False
        self.game_paused: bool = False
        self.in_waiting_room: bool = False
        
        self.status_var = tk.StringVar(value="Odpojeno od serveru")
        tk.Label(self.root, textvariable=self.status_var, bd=1, relief=tk.SUNKEN, anchor=tk.W, padx=8, pady=4).pack(
            side=tk.BOTTOM, fill=tk.X
        )
        
        container = tk.Frame(self.root, padx=16, pady=16)
        container.pack(fill=tk.BOTH, expand=True)
        
        self.login_view = LoginView(
            container,
            default_host,
            default_port,
            on_login=lambda: self._connect_and_auth(sync=False),
            on_sync=lambda: self._connect_and_auth(sync=True)
        )
        self.lobby_view = LobbyView(
            container,
            on_refresh=lambda: self.net.send_message(Opcode.RLST),
            on_create=lambda: self.net.send_message(Opcode.RMAK),
            on_join=self._join_selected_room,
            on_disconnect=self._disconnect_to_login
        )
        self.game_view = GameView(
            container,
            on_column_click=self._send_move,
            on_leave_room=self._leave_waiting_room
        )
        
        self._handlers = {
            Opcode.LRES: self._on_lres,
            Opcode.RINF: self._on_rinf,
            Opcode.RRES: self._on_rres,
            Opcode.GSTR: self._on_gstr,
            Opcode.GUPD: self._on_gupd,
            Opcode.GOVR: self._on_govr,
            Opcode.OSTA: self._on_osta,
            Opcode.SRES: self._on_sres,
            Opcode.ERRR: self._on_errr,
        }
        
        self._show_view(self.login_view)
        self.root.after(50, self._poll_network)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        
    def _show_view(self, target: tk.Frame) -> None:
        for view in (self.login_view, self.lobby_view, self.game_view):
            view.pack_forget()
        target.pack(fill=tk.BOTH, expand=True)
        
    def _protocol_error(self, detail: str) -> None:
        self.net.disconnect()
        self.in_waiting_room = False
        self.game_paused = False
        self.status_var.set(f"Chyba protokolu: {detail}")
        self._show_view(self.login_view)
        messagebox.showerror("Chyba protokolu", f"Server poslal neplatnou zpravu:\n {detail}")
        
    def _connect_and_auth(self, sync: bool) -> None:
        host = self.login_view.host_entry.get().strip()
        port_str = self.login_view.port_entry.get().strip()
        nick = self.login_view.nick_entry.get().strip()
        
        if (
            not host or not nick or not nick.isascii() or not nick.isprintable()
            or " " in nick or DELIMITER in nick or len(nick) > MAX_NICK_LEN
        ):
            messagebox.showwarning("Neplatny vstup", "Zadejte platnou IP adresu a prezdivku (1-20 tisknutelnych ASCII znaku bez mezer a carek).")
            return
        
        try:
            self.status_var.set(f"Pripojovani k {host}:{port_str}...")
            self.root.update_idletasks()
            self.net.connect(host, int(port_str))
        except OSError as exc:
            self.status_var.set("Pripojovani selhalo.")
            messagebox.showerror("Chyba site", f"Nepodarilo se pripojit k serveru:\n{exc}")
            return
        
        self.nickname = nick
        self.net.send_message(Opcode.SYNC if sync else Opcode.LGIN, nick)
    
    def _disconnect_to_login(self) -> None:
        self.net.disconnect()
        self.in_waiting_room = False
        self.game_paused = False
        self.status_var.set("Odpojeno od serveru.")
        self._show_view(self.login_view)
    
    def _join_selected_room(self) -> None:
        room_id = self.lobby_view.get_selected_room_id()
        if room_id is None:
            messagebox.showinfo("Vyber mistnosti", "Nejprve vyberte mistnost ze seznamu.")
            return
        self.net.send_message(Opcode.RJOI, room_id)
        
    def _leave_waiting_room(self) -> None:
        if self.in_waiting_room:
            self.net.send_message(Opcode.RLEA)
            self.in_waiting_room = False
            self._show_view(self.lobby_view)
            self.net.send_message(Opcode.RLST)
            self.status_var.set("Opustili jste cekarnu.")
        
    def _send_move(self, col: int) -> None:
        if not self.in_waiting_room and not self.game_paused and self.my_turn:
            self.net.send_message(Opcode.GMOV, str(col))
        
    def _poll_network(self) -> None:
        while True:
            try:
                ev: NetworkEvent = self.net.incoming_queue.get_nowait()
            except queue.Empty:
                break
            
            if ev.kind in ("DISCONNECTED", "ERROR"):
                self.net.disconnect()
                self.in_waiting_room = False
                self.game_paused = False
                self.status_var.set(ev.info)
                self._show_view(self.login_view)
                messagebox.showwarning(
                    "Odpojeno", f"{ev.info}\nPri rozehrane hre pouzijte 'Obnovit hru'."
                )
            elif ev.kind == "MESSAGE" and ev.message is not None:
                handler = self._handlers.get(ev.message.opcode)
                if handler is None:
                    self._protocol_error(f"Neznamy opcode: {ev.message.opcode}")
                    break
                try:
                    handler(ev.message.body)
                except ValueError as exc:
                    self._protocol_error(str(exc))
                    break
                    
        self.root.after(50, self._poll_network)
    
    def _on_lres(self, body: str) -> None:
        if body == "OK":
            self.lobby_view.set_nickname(self.nickname)
            self._show_view(self.lobby_view)
            self.net.send_message(Opcode.RLST)
            self.status_var.set("Prihlaseni uspesne.")
        elif body == "TAKN":
            self.net.disconnect()
            self.status_var.set("Prezdivka je jiz zabrana.")
            messagebox.showwarning("Obsazeno", "Tuto prezdivku uz pouziva nekdo jiny. Vyberte si jinou.")
        else:
            self._protocol_error(f"Neplatna odpoved LRES: {body}")
            
    def _on_rinf(self, body: str) -> None:
        rooms: list[tuple[str, str]] = []
        if body:
            parts = body.split(DELIMITER)
            if len(parts) % 2 != 0:
                self._protocol_error("Neplatny seznam mistnosti RINF")
                return
            rooms =  [(parts[i], parts[i + 1]) for i in range(0, len(parts) - 1, 2)]
        self.lobby_view.update_rooms(rooms)
        self.status_var.set("Seznam mistnosti aktualizovan.")
        
    def _on_rres(self, body: str) -> None:
        if body == "FULL":
            messagebox.showwarning("Plny server", "Server dosahl maximalniho poctu mistnosti. Pripojte se do jiz existujici nebo cekejte na uvolneni.")
            return
        if not body.isdigit():
            self._protocol_error(f"Neplatne ID mistnosti v RRES: {body}")
            return
        
        self.in_waiting_room = True
        self.game_paused = False
        self.my_turn = False
        self.game_view.set_waiting_mode(body)
        self._show_view(self.game_view)
        self.status_var.set(f"Vytvorena nova mistnost #{body}.")
        
    def _on_gstr(self, body: str) -> None:
        parts = body.split(DELIMITER)
        if len(parts) != 3 or parts[0] not in (PLAYER1_DISC, PLAYER2_DISC) or parts[2] not in ("0", "1"):
            self._protocol_error(f"Nepplatny format GSTR: {body}")
            return
        
        self.my_disc, self.opponent_nick, starts = parts
        self.in_waiting_room = False
        self.game_paused = False
        self.my_turn = (starts == "1")
        self.game_view.reset_board()
        self.game_view.set_playing_mode(self.my_disc, self.my_turn, self.opponent_nick)
        self._show_view(self.game_view)
        self.status_var.set(f"Zahajena hra proti {self.opponent_nick}.")
    
    def _on_gupd(self, body: str) -> None:
        parts = body.split(DELIMITER)
        if len(parts) != 2 or not parts[0].isdigit() or parts[1] not in (PLAYER1_DISC, PLAYER2_DISC):
            self._protocol_error(f"Neplatny format GUPD: {body}")
            return
            
        col = int(parts[0])
        next_turn_disc = parts[1]
        self.game_view.drop_disc(col)
        self.my_turn = (next_turn_disc == self.my_disc)
        self.game_view.set_playing_mode(self.my_disc, self.my_turn, self.opponent_nick)
    
    def _on_govr(self, body: str) -> None:
        if body not in ("WIN", "LOSE", "DRAW"):
            self._protocol_error(f"Neplatny format GOVR: {body}")
            return
        
        self.my_turn = False
        self.my_disc = ""
        messages = {"WIN": "Vyhrali jste!", "LOSE": "Prohrali jste.", "DRAW": "Hra skoncila remizou."}
        text =  messages.get(body, "Konec hry.")
        messagebox.showinfo("Konec hry", text)
        self._show_view(self.lobby_view)
        self.net.send_message(Opcode.RLST)
        self.status_var.set(f"Konec hry: {text}")
        
    def _on_osta(self, body: str) -> None:
        if body == "DISC":
            self.game_paused = True
            self.game_view.set_paused_mode()
            self.status_var.set("Hra pozastavena.")
        elif body == "CONN":
            self.game_paused = False
            self.game_view.set_playing_mode(self.my_disc, self.my_turn, self.opponent_nick)
            self.status_var.set("Souper se znovu pripojil. Hra pokracuje.")
        elif body == "DROP":
            self.game_paused = False
            self.my_disc = ""
            messagebox.showinfo("Konec hry", "Souper se nevratil v casovem limitu a byl odpojen. Vyhral jste kontumacne!")
            self._show_view(self.lobby_view)
            self.net.send_message(Opcode.RLST)
        else:
            self._protocol_error(f"Neplatny format OSTA: {body}")
            
    def _on_sres(self, body: str) -> None:
        if body == "NOTF":
            self.net.disconnect()
            self.status_var.set("Rozehrana hra nenalezena.")
            messagebox.showinfo("Obnova hry", "Pro tuto prezdivku neexistuje zadna rozehrana hra.")
            return
        
        parts = body.split(DELIMITER)
        if (len(parts) != 4 or len(parts[0]) != GameView.ROWS * GameView.COLS
            or parts[1] not in (PLAYER1_DISC, PLAYER2_DISC) or parts[2] not in (PLAYER1_DISC, PLAYER2_DISC) or not parts[3]
        ):
            self._protocol_error(f"Neplatny format SRES: {body}")
            return
        
        board_str, turn_disc, my_disc, opponent_nick = parts
        self.my_disc = my_disc
        self.opponent_nick = opponent_nick
        self.in_waiting_room = False
        self.game_paused = False
        self.my_turn = (turn_disc == self.my_disc)
        self.game_view.load_serialized_board(board_str)
        self.game_view.set_playing_mode(self.my_disc, self.my_turn, self.opponent_nick)
        self._show_view(self.game_view)
        self.status_var.set("Hra uspesne obnovenapo vypadku!")
            
    def _on_errr(self, body: str) -> None:
        self.status_var.set(f"Server odmitl pozadavek (chyba: {body})")
        messagebox.showwarning("Chyba protokolu", f"Server vratil chybu: {body}")
    
    def _on_close(self) -> None:
        self.net.disconnect()
        self.root.destroy()