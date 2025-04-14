#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 9090
#define BUFFER_SIZE 1024

/* 
Total: 6 marks -- check if all the calls are in order

2 marks for setting the socket parameters
2 marks for sendto
2 marks for recvfrom
*/ 



int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE], recv_buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    printf("UDP Client running. Type messages (Ctrl+C to exit)...\n");

    while (1) {
        printf("You: ");
        fflush(stdout);
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        // Remove trailing newline if any
        buffer[strcspn(buffer, "\n")] = '\0';

        sendto(sockfd, buffer, strlen(buffer), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));

        socklen_t addr_len = sizeof(server_addr);
        ssize_t recv_len = recvfrom(sockfd, recv_buffer, BUFFER_SIZE - 1, 0,
                                    (struct sockaddr*)&server_addr, &addr_len);
        if (recv_len > 0) {
            recv_buffer[recv_len] = '\0';
            printf("Server: %s\n", recv_buffer);
        }
    }

    close(sockfd);
    return 0;
}

