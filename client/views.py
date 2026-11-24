import tkinter as tk
from collections.abc import Callable

PLAYER1_DISC: str = "R"
PLAYER2_DISC: str = "B"
EMPTY_CELL = "."

PLAYER1_COLOR: str = "#D60D0D"
PLAYER2_COLOR: str = "#156CD7"
PLAYER1_GHOST: str = "#C28B8B"
PLAYER2_GHOST: str = "#799CC8"

BOARD_BG_COLOR: str = "#7A7C7B"
HOVER_BG_COLOR: str = "#565857"
GRID_LINE_COLOR: str = "#333534"

RECONNECT_TIMEOUT: int = 30

class LoginView(tk.Frame):
    host_entry: tk.Entry
    port_entry: tk.Entry
    nick_entry: tk.Entry
    
    def __init__(
        self,
        parent: tk.Widget,
        default_host: str,
        default_port: int,
        on_login: Callable[[], None],
        on_sync: Callable[[], None]
    ) -> None:
        super().__init__(parent)
        
        tk.Label(self, text="Connect FOur - pripojeni", font=("Arial", 14, "bold")).grid(
            row=0, column=0, columnspan=2, pady=(0, 12)
        )

        tk.Label(self, text="IP adresa serveru:").grid(row=1, column=0, sticky=tk.W, pady=4)
        self.host_entry = tk.Entry(self, width=22)
        self.host_entry.insert(0, default_host)
        self.host_entry.grid(row=1, column=1, pady=4)
        
        tk.Label(self, text="Port:").grid(row=2, column=0, sticky=tk.W, pady=4)
        self.port_entry = tk.Entry(self, width=22)
        self.port_entry.insert(0, str(default_port))
        self.port_entry.grid(row=2, column=1, pady=4)
        
        tk.Label(self, text="Prezdivka (max 20 znaku):").grid(row=3, column=0, sticky=tk.W, pady=4)
        self.nick_entry = tk.Entry(self, width=22)
        self.nick_entry.grid(row=3, column=1, pady=4)
        
        btns = tk.Frame(self)
        btns.grid(row=4, column=0, columnspan=2, pady=(14, 0))
        tk.Button(btns, text="Prihlasit do lobby", width=16, command=on_login).pack(side=tk.LEFT, padx=4)
        tk.Button(btns, text="Obnovit hru (SYNC)", width=16, command=on_sync).pack(side=tk.LEFT, padx=4)
        
class LobbyView(tk.Frame):
    header_label: tk.Label
    rooms_listbox: tk.Listbox
    
    def __init__(
        self,
        parent: tk.Widget,
        on_refresh: Callable[[], None],
        on_create: Callable[[], None],
        on_join: Callable[[], None],
        on_disconnect: Callable[[],None]
    ) -> None:
        super().__init__(parent)
        
        self.header_label = tk.Label(self, text="Lobby", font=("Arial", 13, "bold"))
        self.header_label.pack(pady=(0, 8))
        
        self.rooms_listbox = tk.Listbox(self, width=38, height=10, font=("Courier", 10))
        self.rooms_listbox.pack(pady=6)
        
        btns = tk.Frame(self)
        btns.pack(pady=8)
        tk.Button(btns, text="Obnovit seznam", command=on_refresh).pack(side=tk.LEFT, padx=4)
        tk.Button(btns, text="Vytvorit mistnost", command=on_create).pack(side=tk.LEFT, padx=4)
        tk.Button(btns, text="Pripojit se", command=on_join).pack(side=tk.LEFT, padx=4)
        
        tk.Button(self, text="Odpojit od serveru", command=on_disconnect).pack(pady=(6, 0))
        
    def set_nickname(self, nickname: str) -> None:
        self.header_label.config(text=f"Lobby - prihlasen jako {nickname}")
        
    def update_rooms(self, rooms: list[tuple[str, str]]) -> None:
        self.rooms_listbox.delete(0, tk.END)
        for room_id, occupancy in rooms:
            self.rooms_listbox.insert(tk.END, f"Mistnost #{room_id} ({occupancy}/2 hracu)")
        
    def get_selected_room_id(self) -> str | None:
        selection = self.rooms_listbox.curselection()
        if not selection:
            return None
        line: str = self.rooms_listbox.get(selection[0])
        return line.split("#")[1].split(" ")[0] # ziskani ID z popisu

class GameView(tk.Frame):
    ROWS: int = 6
    COLS: int = 7
    CELL_SIZE: int = 70
    
    info_var: tk.StringVar
    canvas: tk.Canvas
    leave_btn: tk.Button
    board: list[list[str]]
    _on_column_click: Callable[[int], None]
    _my_disc: str
    _can_play: bool
    _hover_col: int | None
    
    def __init__(
        self,
        parent: tk.Widget,
        on_column_click: Callable[[int], None],
        on_leave_room: Callable[[], None]
    ) -> None:
        super().__init__(parent)
        
        self._on_column_click = on_column_click
        self.board = [["." for _ in range(self.COLS)] for _ in range(self.ROWS)]
        self._my_disc = ""
        self._can_play = False
        self._hover_col = None
        
        self.info_var = tk.StringVar(value="Cekani na soupere...")
        tk.Label(self, textvariable=self.info_var, font=("Arial", 12, "bold")).pack(pady=(0, 8))
        
        self.canvas = tk.Canvas(
            self,
            width=self.COLS * self.CELL_SIZE,
            height=self.ROWS * self.CELL_SIZE,
            bg=BOARD_BG_COLOR,
            highlightthickness=2,
            highlightbackground=GRID_LINE_COLOR
        )
        self.canvas.pack()
        self.canvas.bind("<Button-1>", self._handle_click) # navazani leveho tlacitka mysi na platno
        self.canvas.bind("<Motion>", self._handle_mouse_move)
        self.canvas.bind("<Leave>", self._handle_mouse_leave)
        
        self.leave_btn = tk.Button(self, text="Opustit cekarnu", command=on_leave_room)
        self.leave_btn.pack(pady=(10, 0))
        
    def reset_board(self) -> None:
        self.board = [["." for _ in range(self.COLS)] for _ in range(self.ROWS)]
        self._redraw()
        
    def load_serialized_board(self, flat: str) -> None:
        for r in range(self.ROWS):
            for c in range(self.COLS):
                ch = flat[r * self.COLS + c]
                if ch not in (PLAYER1_DISC, PLAYER2_DISC, EMPTY_CELL):
                    raise ValueError(f"Neplatny znak v serializovane desce: {ch}")
                self.board[r][c] = ch
        self._redraw()
        
    def drop_disc(self, col: int) -> None:
        target_row = self._get_drop_row(col)
        if target_row is None:
            return
        
        total_discs = sum(cell != "." for row in self.board for cell in row)
        disc = PLAYER1_DISC if (total_discs % 2 == 0) else PLAYER2_DISC
        self.board[target_row][col] = disc
        self._redraw()
        
    def set_waiting_mode(self, room_id: str) -> None:
        self._my_disc = ""
        self._can_play = False
        self._hover_col = None
        self.reset_board()
        self.info_var.set(f"Mistnost #{room_id} - cekani na pripojeni soupere...")
        self.leave_btn.config(state=tk.NORMAL)
        
    def set_playing_mode(self, my_disc: str, my_turn: bool, opponent_nick: str) -> None:
        self._my_disc = my_disc
        self._can_play = my_turn
        self.leave_btn.config(state=tk.DISABLED)
        
        color_name = "CERVENY" if my_disc == PLAYER1_DISC else "MODRY"
        turn_text = "JSTE NA TAHU!" if my_turn else "Taha souper..."
        self.info_var.set(f"Na tahu: {color_name} ({turn_text})")
        self._redraw()
                
    def set_paused_mode(self) -> None:
        self._can_play = False
        self.info_var.set(f"Souper ma vypadek spojeni! Cekame {RECONNECT_TIMEOUT} s na jeho navrat...")
        self._redraw()
        
    def _get_drop_row(self, col: int) -> int | None:
        if not (0 <= col < self.COLS):
            return None
        for r in range(self.ROWS - 1, -1, -1):
            if self.board[r][col] == EMPTY_CELL:
                return r
        return None
    
    def _handle_click(self, event: tk.Event) -> None:
        col = event.x // self.CELL_SIZE # celociselne deleni
        if 0 <= col < self.COLS and self._get_drop_row(col) is not None:
            self._on_column_click(col)
            
    def _handle_mouse_move(self, event: tk.Event) -> None:
        col = event.x // self.CELL_SIZE
        new_hover = col if (0 <= col < self.COLS) else None
        if new_hover != self._hover_col:
            self._hover_col = new_hover
            self._redraw()
            
    def _handle_mouse_leave(self, _event: tk.Event) -> None:
        if self._hover_col is not None:
            self._hover_col = None
            self._redraw()
            
    def _redraw(self) -> None:
        self.canvas.delete("all")
        width = self.COLS * self.CELL_SIZE
        height = self.ROWS * self.CELL_SIZE
        
        ghost_row: int | None = None
        if self._can_play and self._hover_col is not None:
            ghost_row = self._get_drop_row(self._hover_col)
            
        if ghost_row is not None and self._hover_col is not None:
            hx0 = self._hover_col * self.CELL_SIZE
            hx1 = (self._hover_col + 1) * self.CELL_SIZE
            self.canvas.create_rectangle(hx0, 0, hx1, height, fill=HOVER_BG_COLOR, outline="")
            self.canvas.config(cursor="hand2")
        else:
            self.canvas.config(cursor="")
            
        for c in range(1, self.COLS):
            x = c * self.CELL_SIZE
            self.canvas.create_line(x, 0, x, height, fill=GRID_LINE_COLOR, width=1)
        for r in range(1, self.ROWS):
            y = r * self.CELL_SIZE
            self.canvas.create_line(0, y, width, y, fill=GRID_LINE_COLOR, width=1)
                
        pad = 8
        for r in range(self.ROWS):
            for c in range(self.COLS):
                x0 = c * self.CELL_SIZE + pad
                y0 = r * self.CELL_SIZE + pad
                x1 = (c + 1) * self.CELL_SIZE - pad
                y1 = (r + 1) * self.CELL_SIZE - pad
                
                cell = self.board[r][c]
                if cell == PLAYER1_DISC or cell == PLAYER2_DISC:
                    fill = PLAYER1_COLOR if cell == PLAYER1_DISC else PLAYER2_COLOR
                    self.canvas.create_oval(x0, y0, x1, y1, fill=fill, outline="#848484", width=2)
                elif r == ghost_row and c == self._hover_col:
                    ghost_fill = PLAYER1_GHOST if self._my_disc == PLAYER1_DISC else PLAYER2_GHOST
                    self.canvas.create_oval(x0, y0, x1, y1, fill=ghost_fill, outline="#848484", width=2)
        