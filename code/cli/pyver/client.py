from soter2 import client
import sys
import time
import random

if __name__ == "__main__":
    print("Soter2, 0.0.1")

    # Initialization
    cli = client.Client(f"http://localhost:{sys.argv[1]}")
    cli.update_status()

    print(f"connection info: {cli.connection_info()}")
    info = input("Input uni:port:pubuid to connect: ")

    # Connecting
    uni, port, pubuid = info.split(':')
    conn = cli.connect(uni, int(port), pubuid)

    print("Connected:", conn.message)

    # Disconnecting
    time.sleep(random.randint(2, 6))
    disc = cli.disconnect(conn.uid)

    print("Disconnected:", disc.message)
