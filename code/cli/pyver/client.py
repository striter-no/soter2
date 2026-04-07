import requests
import sys
import time
import random

def connect(
    http_base: str,
    uni_addr: str,
    port: int,
    pubuid: str
):
    r = requests.post(http_base + "/api/connect", params = {
        "uni": uni_addr,
        "port": port,
        "pubuid": pubuid
    })

    if r.status_code != 200:
        return

    js = r.json()
    return js

def disconnect(http_base: str, uid: int):
    r = requests.post(http_base + "/api/disconnect", params = {"uid": uid})

    if r.status_code != 200:
        return

    js = r.json()
    return js

def status(http_base: str):
    r = requests.get(http_base + "/api/status")

    if r.status_code != 200:
        return

    js = r.json()
    return js

if __name__ == "__main__":
    print("[py] Client Python version, 0.0.1")

    status_data = status(f"http://localhost:{sys.argv[1]}")
    print(f"[py] connection info: \"{status_data['ip']}:{status_data['port']}:{status_data['pubuid']}\"")

    info = input("Input uni:port:pubuid to connect: ")

    uni, port, pubuid = info.split(':')
    conn_data = connect(
        f"http://localhost:{sys.argv[1]}",
        uni, int(port), pubuid
    )
    print(conn_data["message"], conn_data["uid"])

    time.sleep(random.randint(2, 6))
    print(disconnect(f"http://localhost:{sys.argv[1]}", conn_data["uid"])["message"])
