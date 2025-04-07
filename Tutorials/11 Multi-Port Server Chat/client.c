/*
    client.c - Non-blocking Chat Client
 
    - Connects to a server on a given port (9001 - 9005).
    - Uses select() to allow both sending and receiving without blocking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket"); exit(1);
    }

    set_nonblocking(sockfd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    printf("Connected to chat on port %d. Start typing messages.\n", port);

    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        if (select(sockfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select"); exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin)) {
                send(sockfd, buffer, strlen(buffer), 0);
            }
        }

        if (FD_ISSET(sockfd, &readfds)) {
            int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (n <= 0) {
                printf("Disconnected from server.\n");
                break;
            }
            buffer[n] = '\0';
            printf("[Chat] %s", buffer);
        }
    }

    close(sockfd);
    return 0;
}
