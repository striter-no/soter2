from dataclasses import dataclass
from .core import APIStatus, APISystemStatus

@dataclass
class SystemResponse:
    message: str
    status: APIStatus

@dataclass
class SystemStatus:
    status: APISystemStatus
    ip: str
    port: int
    fingerprint: str
    pubuid: str
