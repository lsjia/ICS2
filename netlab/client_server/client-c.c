/*****************************************************************************
 * client-c.c
 * Name:Luo Sijia
 * NetId:
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#define SEND_BUFFER_SIZE 2048

/* TODO: client()
 * Open socket and send message from stdin.
 * Return 0 on success, non-zero on failure
 */
int client(char *server_ip, char *server_port)
{
    int sockfd, port;
    port = atoi(server_port);

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        exit(0);
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &addr.sin_addr.s_addr);
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect error");
        exit(0);
    }
    // printf("Connected to server successfully at %s:%s\n", server_ip, server_port);
    // Send message from stdin
    while (1)
    {
        ssize_t num_bytes;
        // printf("Please enter the msg:\n");
        char send_buffer[SEND_BUFFER_SIZE + 1]; // 注意要加1
        memset(send_buffer, 0, sizeof(send_buffer));
        num_bytes = read(STDIN_FILENO, send_buffer, SEND_BUFFER_SIZE * sizeof(char));
        if (num_bytes == -1)
        {
            perror("read error");
            break;
        }
        if (num_bytes == 0)
            break; // Reached EOF

        ssize_t bytes_sent = write(sockfd, send_buffer, num_bytes);
        // printf("Send: %s\n", send_buffer);
        // fflush(stdout);
        if (bytes_sent == -1)
        {
            perror("send error");
            break;
        }
    }

    // Close the socket
    close(sockfd);

    return 0;
}

/*
 * main()
 * Parse command-line arguments and call client function
 */
int main(int argc, char **argv)
{
    char *server_ip;
    char *server_port;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: ./client-c [server IP] [server port] < [message]\n");
        exit(EXIT_FAILURE);
    }

    server_ip = argv[1];
    server_port = argv[2];
    return client(server_ip, server_port);
}
