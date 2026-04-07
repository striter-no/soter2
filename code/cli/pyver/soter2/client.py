from .peers import PeerConnection
from .core import APIStatus, APISystemStatus
from .api import PeersAPI, SystemAPI
from .system import SystemResponse

class Client:
    def __init__(self, api_server_url: str) -> None:
        self.ip_address: None | str = None
        self.port: None | int = None
        self.stating_servers: list[str] = []
        self.connected_peers: dict[int, PeerConnection] = {}
        self.api_server: str = api_server_url

        self.fingerprint: str | None = None
        self.pubuid: str | None = None
        self.sys_status: APISystemStatus = APISystemStatus.NON_ACTIVE

    def update_status(self) -> None:
        sys_status = SystemAPI.status(self.api_server)

        self.ip_address = sys_status.ip
        self.fingerprint = sys_status.fingerprint
        self.sys_status = sys_status.status
        self.pubuid = sys_status.pubuid
        self.port = sys_status.port

    def connection_info(self) -> str:
        return f"{self.ip_address}:{self.port}:{self.pubuid}"

    def connect(self, uni_addr: str, port: int, pubuid: str) -> PeerConnection:
        conn = PeersAPI.connect(self.api_server, uni_addr, port, pubuid)
        self.connected_peers[conn.uid] = conn

        return conn

    def disconnect(self, uid: int) -> SystemResponse:
        disc = PeersAPI.disconnect(self.api_server, uid)
        if disc.status == APIStatus.OK:
            del self.connected_peers[uid]

        return disc
