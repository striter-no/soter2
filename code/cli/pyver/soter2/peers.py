from dataclasses import dataclass
from .core import APIStatus

@dataclass
class PeerConnection:
    message: str
    uid: int
    status: APIStatus
