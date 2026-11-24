import argparse
import tkinter as tk

from gui import ConnectFourGUI

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Klient pro hru Connect Four")
    parser.add_argument("-i", "--host", default="127.0.0.1", help="IP adresa serveru (vyhozi 127.0.0.1)")
    parser.add_argument("-p", "--port", type=int, default=10000, help="Port serveru (vychozi 10000)")
    return parser.parse_args()

def main() -> None:
    args = parse_args()
    root = tk.Tk()
    ConnectFourGUI(root, default_host=args.host, default_port=args.port)
    root.mainloop()
    
if __name__ == "__main__":
    main()