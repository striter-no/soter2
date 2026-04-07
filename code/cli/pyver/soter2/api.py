import requests
from .peers import PeerConnection
from .system import SystemResponse, SystemStatus
from .core import parse_api_status, parse_sys_api_status

def _post_req(url: str, params: dict | None):
    r = requests.post(url, params = params)
    r.raise_for_status()

    js = r.json()
    return js

def _get_req(url: str, params: dict | None):
    r = requests.get(url, params = params)
    r.raise_for_status()

    js = r.json()
    return js

class PeersAPI:

    @staticmethod
    def connect(api_server: str, uniaddr: str, port: int, pubuid: str) -> PeerConnection:
        data = _post_req(api_server + "/api/peer/connect", {
            "uni": uniaddr,
            "port": port,
            "pubuid": pubuid
        })

        return PeerConnection(
            message = data["message"],
            uid = data["uid"],
            status = parse_api_status(data["status"])
        )

    @staticmethod
    def disconnect(api_server: str, uid: int) -> SystemResponse:
        data = _post_req(api_server + "/api/peer/disconnect", params = {
            "uid": uid
        })

        return SystemResponse(
            message = data["message"],
            status = parse_api_status(data["status"])
        )

class SystemAPI:

    @staticmethod
    def status(api_server: str) -> SystemStatus:
        data = _get_req(api_server + "/api/status", params = None)

        return SystemStatus(
            status = parse_sys_api_status(data["status"]),
            ip = data["ip"],
            port = int(data["port"]),
            fingerprint = data["fingerprint"],
            pubuid = data["pubuid"]
        )
