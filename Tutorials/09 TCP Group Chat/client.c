#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];
    fd_set readfds;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to group chat server.\n");
    int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    fgets(buffer, BUFFER_SIZE, stdin);
    send(sock, buffer, strlen(buffer), 0);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);  
        FD_SET(sock, &readfds);         

        select(sock + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            if (strncmp(buffer, "exit", 4) == 0) {
                printf("Exiting...\n");
                break;
            }
            send(sock, buffer, strlen(buffer), 0);
        }

        if (FD_ISSET(sock, &readfds)) {
            bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_read == 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
        }
    }

    close(sock);
    return 0;
}
