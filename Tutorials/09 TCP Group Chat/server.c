#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    char name[50];
} Client;

int main() {
    int server_fd, new_socket;
    Client clients[MAX_CLIENTS] = {0};
    struct sockaddr_in address;
    fd_set readfds;
    int max_fd, addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Group Chat Server started on port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;
            if (fd > 0) {
                FD_SET(fd, &readfds);
                if (fd > max_fd) max_fd = fd;
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

            if (new_socket >= 0) {
                printf("New client connected: FD %d\n", new_socket);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == 0) {
                        clients[i].fd = new_socket;

                        send(new_socket, "Enter your name: ", 17, 0);
                        int name_len = recv(new_socket, clients[i].name, 50, 0);
                        clients[i].name[name_len - 1] = '\0'; 
                        
                        printf("%s joined the chat\n", clients[i].name);
                        sprintf(buffer, "%s joined the chat\n", clients[i].name);

                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].fd > 0 && clients[j].fd != new_socket) {
                                send(clients[j].fd, buffer, strlen(buffer), 0);
                            }
                        }
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;
            if (fd > 0 && FD_ISSET(fd, &readfds)) {
                int bytes_read = recv(fd, buffer, BUFFER_SIZE, 0);
                if (bytes_read == 0) {
                    printf("Client %s disconnected: FD %d\n", clients[i].name, fd);
                    sprintf(buffer, "%s left the chat\n", clients[i].name);
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd > 0 && clients[j].fd != fd) {
                            send(clients[j].fd, buffer, strlen(buffer), 0);
                        }
                    }
                    close(fd);
                    clients[i].fd = 0;
                } else {
                    buffer[bytes_read] = '\0';
                    printf("%s: %s", clients[i].name, buffer);

                    char message[BUFFER_SIZE];
                    sprintf(message, "%s: %s", clients[i].name, buffer);
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd > 0 && clients[j].fd != fd) {
                            send(clients[j].fd, message, strlen(message), 0);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
