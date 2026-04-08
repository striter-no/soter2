import socket

class S2Socket:
    def __init__(self) -> None:
        self.raw = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
