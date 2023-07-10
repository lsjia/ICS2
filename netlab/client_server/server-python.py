###############################################################################
# server-python.py
# Name:Luo Sijia
# NetId:
###############################################################################

import sys
import socket

RECV_BUFFER_SIZE = 2048
QUEUE_LENGTH = 10


def server(server_port):
    """TODO: Listen on socket and print received message to sys.stdout"""
    lfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    lfd.bind(('127.0.0.1', server_port))
    lfd.listen(QUEUE_LENGTH)
    # print("Server started. Listening on port {}".format(server_port))
    # Accept and handle client connections
    while True:
        cfd, client_addr = lfd.accept()
        # Receive and print message
        while True:
            recv_buffer = cfd.recv(RECV_BUFFER_SIZE)
            if len(recv_buffer) > 0:
                try:
                    # 如果是文本消息，先解码再输出
                    recv_text = recv_buffer.decode()
                    sys.stdout.write(recv_text)
                    sys.stdout.flush()
                except UnicodeDecodeError:
                    # 是二进制消息，直接输出
                    sys.stdout.buffer.write(recv_buffer)
                    sys.stdout.buffer.flush()
            else:
                # print "The client closed the connection."
                break

        cfd.close()

    lfd.close()


def main():
    """Parse command-line argument and call server function """
    if len(sys.argv) != 2:
        sys.exit("Usage: python server-python.py [Server Port]")
    server_port = int(sys.argv[1])
    server(server_port)


if __name__ == "__main__":
    main()
