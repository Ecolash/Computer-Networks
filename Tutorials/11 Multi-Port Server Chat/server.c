/*
    server.c - Non-blocking Group Chat Proxy Server
    
    - Listens on 5 different ports (PORTS[]). At most one client connects per port.
    - When a client sends a message, it is forwarded to all other clients.
    - Clients are identified by the port they are connected to.
    - Uses select() for multiplexing and non-blocking I/O.
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

#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

int PORTS[MAX_CLIENTS] = {9001, 9002, 9003, 9004, 9005};

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int listener[MAX_CLIENTS];
    int clients[MAX_CLIENTS] = {-1, -1, -1, -1, -1};
    struct sockaddr_in addr;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Create and bind listening sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        listener[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (listener[i] < 0) {
            perror("socket"); exit(1);
        }
        set_nonblocking(listener[i]);

        int opt = 1;
        setsockopt(listener[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORTS[i]);

        if (bind(listener[i], (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind"); exit(1);
        }
        if (listen(listener[i], 1) < 0) {
            perror("listen"); exit(1);
        }
        printf("Listening on port %d\n", PORTS[i]);
    }

    while (1) {
        FD_ZERO(&readfds);
        int maxfd = 0;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            FD_SET(listener[i], &readfds);
            maxfd = (listener[i] > maxfd) ? listener[i] : maxfd;
            if (clients[i] != -1) {
                FD_SET(clients[i], &readfds);
                maxfd = (clients[i] > maxfd) ? clients[i] : maxfd;
            }
        }

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select"); exit(1);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            // Accept new client if slot is empty
            if (clients[i] == -1 && FD_ISSET(listener[i], &readfds)) {
                int newfd = accept(listener[i], NULL, NULL);
                if (newfd >= 0) {
                    set_nonblocking(newfd);
                    clients[i] = newfd;
                    printf("Client connected on port %d\n", PORTS[i]);
                }
            }
            // Handle client message
            else if (clients[i] != -1 && FD_ISSET(clients[i], &readfds)) {
                int n = recv(clients[i], buffer, BUFFER_SIZE, 0);
                if (n <= 0) {
                    printf("Client on port %d disconnected\n", PORTS[i]);
                    close(clients[i]);
                    clients[i] = -1;
                } else {
                    buffer[n] = '\0';
                    printf("From port %d: %s", PORTS[i], buffer);
                    // Broadcast to all others
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (j != i && clients[j] != -1) {
                            send(clients[j], buffer, n, 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
