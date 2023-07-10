/*****************************************************************************
 * server-c.c
 * Name: Luo Sijia
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

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048

/* TODO: server()
 * Open socket and wait for client to connect
 * Print received message to stdout
 * Return 0 on success, non-zero on failure
 */
int server(char *server_port)
{
    int lfd;
    int port = atoi(server_port);
    struct sockaddr_in server_addr;
    socklen_t addr_size;
    // Create socket
    if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        exit(0);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // Bind socket to address and port
    if (bind(lfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind error");
        exit(0);
    }
    // Listen for incoming connections
    if (listen(lfd, QUEUE_LENGTH) == -1)
    {
        perror("listen error");
        exit(0);
    }
    // printf("Server started. Listening on port %s\n", server_port);
    // Accept and handle client connections
    while (1)
    {
        // Accept connection
        int cfd;
        struct sockaddr_in client_addr;
        addr_size = sizeof client_addr;
        cfd = accept(lfd, (struct sockaddr *)&client_addr, &addr_size);
        if (cfd == -1)
        {
            perror("accept error");
            exit(0);
        }
        // Receive and print message
        ssize_t num_bytes;
        while (1)
        {
            char recv_buffer[RECV_BUFFER_SIZE + 1]; // 注意这里要加1
            memset(recv_buffer, 0, sizeof(recv_buffer));
            num_bytes = read(cfd, recv_buffer, RECV_BUFFER_SIZE * sizeof(char));
            if (num_bytes > 0)
            {
                // printf("%s", recv_buffer);
                fwrite(recv_buffer, sizeof(char), num_bytes, stdout); // 注意为什么要用fwrite而不用printf
                fflush(stdout);
            }
            else if (num_bytes == 0)
            {
                // printf("The client close the connection.\n");
                break;
            }
            else
            {
                perror("recv error");
                break;
            }
        }
        if (num_bytes == -1)
        {
            perror("recv error");
        }
        // Close the connection
        close(cfd);
    }

    close(lfd);

    return 0;
}

/*
 * main():
 * Parse command-line arguments and call server function
 */
int main(int argc, char **argv)
{
    char *server_port;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./server-c [server port]\n");
        exit(EXIT_FAILURE);
    }

    server_port = argv[1];
    return server(server_port);
}
