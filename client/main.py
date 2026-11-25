import argparse
import tkinter as tk
import logging

from gui import ConnectFourGUI

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Klient pro hru Connect Four")
    parser.add_argument("-i", "--host", default="127.0.0.1", help="IP adresa serveru (vyhozi 127.0.0.1)")
    parser.add_argument("-p", "--port", type=int, default=10000, help="Port serveru (vychozi 10000)")
    parser.add_argument("-l", "--log", default="client.log", help="Soubor pro logovani klienta (vychozi client.log)")
    return parser.parse_args()

def setup_logging(log_file: str) -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levename)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
        handlers=[
            logging.FileHandler(log_file, mode="a", encoding="utf-8"),
            logging.StreamHandler()
        ]
    )

def main() -> None:
    args = parse_args()
    setup_logging(args.log)
    logging.info(f"Spousteni klienta (vychozi server {args.host}:{args.port})")
    
    root = tk.Tk()
    ConnectFourGUI(root, default_host=args.host, default_port=args.port)
    root.mainloop()
    logging.info("Klient korektne ukoncen.")
    
if __name__ == "__main__":
    main()