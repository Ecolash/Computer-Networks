#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 9090
#define BUFFER_SIZE 1024

/*
Total: 8 marks
socket, address set and connect: 4 marks
send: 2 marks
recv: 2 marks
*/

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at 127.0.0.1:%d\n", PORT);
    printf("Type messages (Ctrl+C to exit)...\n");

    while (1) {
        printf("You: ");
        fflush(stdout);
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        send(sockfd, buffer, strlen(buffer), 0);
        ssize_t bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Echo: %s", buffer);
        }
    }

    close(sockfd);
    return 0;
}

