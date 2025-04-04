#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket failed");
        exit(1);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[2]))
    };
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(1);
    }

    fd_set read_fds;
    char buffer[BUFFER_SIZE];

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getsockname(sock, (struct sockaddr*)&client_addr, &addr_len);

    printf("Client %s:%d connected to server.\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        select(sock + 1, &read_fds, NULL, NULL, NULL);

        if (FD_ISSET(sock, &read_fds)) {
            int bytes = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                printf("Disconnected from server.\n");
                break;
            }
            buffer[bytes] = '\0';
            printf("%s\n", buffer);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0;  // Remove newline
            send(sock, buffer, strlen(buffer), 0);
            printf("Client %s:%d Message \"%s\" sent to server\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);
        }
    }

    close(sock);
    return 0;
}

