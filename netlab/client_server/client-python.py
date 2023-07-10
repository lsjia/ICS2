###############################################################################
# client-python.py
# Name:Luo Sijia
# NetId:
###############################################################################

import sys
import socket

SEND_BUFFER_SIZE = 2048


def client(server_ip, server_port):
    """TODO: Open socket and send message from sys.stdin"""
    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Connect to the server
    sockfd.connect((server_ip, server_port))
    # print("Connected to server successfully")

    # Send message from stdin
    while True:
        send_data = sys.stdin.buffer.read(SEND_BUFFER_SIZE)

        if not send_data:
            break
        # Send data to the server
        sockfd.send(send_data)

    # Close the socket
    sockfd.close()


def main():
    """Parse command-line arguments and call client function """
    if len(sys.argv) != 3:
        sys.exit(
            "Usage: python client-python.py [Server IP] [Server Port] < [message]"
        )
    server_ip = sys.argv[1]
    server_port = int(sys.argv[2])
    client(server_ip, server_port)


if __name__ == "__main__":
    main()
