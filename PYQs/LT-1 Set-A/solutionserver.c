#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

int clients[MAX_CLIENTS] = {0};
int client_count = 0;

void broadcast_message(int sender_fd, char *message, struct sockaddr_in sender_addr) {
    char msg_with_info[BUFFER_SIZE];
    snprintf(msg_with_info, BUFFER_SIZE, "Client %s:%d says: %s",
             inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port), message);

    printf("Server: Received message \"%s\" from client %s:%d\n",
           message, inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port));

    for (int i = 0; i < client_count; i++) {
        if (clients[i] > 0 && clients[i] != sender_fd) {
            send(clients[i], msg_with_info, strlen(msg_with_info), 0);
            printf("Server: Sent message \"%s\" from %s:%d to client\n",
                   message, inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port));
        }
    }
}

void handle_new_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (new_client_fd < 0) {
        perror("Accept failed");
        return;
    }

    if (client_count >= MAX_CLIENTS) {
        printf("Server: Maximum clients reached. Connection rejected.\n");
        close(new_client_fd);
        return;
    }

    clients[client_count] = new_client_fd;
    client_count++;

    printf("Server: New client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    if (client_count >= 2) {
        char msg[BUFFER_SIZE] = "Server: You are now connected to the chat.\n";
        send(new_client_fd, msg, strlen(msg), 0);
    }
}

void process_client_message(int client_fd, int index) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len);

    int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        printf("Server: Client %s:%d disconnected.\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(client_fd);
        clients[index] = 0;
        return;
    }

    buffer[bytes_read] = '\0';

    if (client_count < 2) {
        printf("Server: Insufficient clients, \"%s\" from client %s:%d dropped\n",
               buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        return;
    }

    broadcast_message(client_fd, buffer, client_addr);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[1])),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    listen(server_fd, MAX_CLIENTS);
    printf("Server: Waiting for clients...\n");

    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < client_count; i++) {
            if (clients[i] > 0) {
                FD_SET(clients[i], &read_fds);
                if (clients[i] > max_fd) {
                    max_fd = clients[i];
                }
            }
        }

        select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &read_fds)) {
            handle_new_connection(server_fd);
        }

        for (int i = 0; i < client_count; i++) {
            if (clients[i] > 0 && FD_ISSET(clients[i], &read_fds)) {
                process_client_message(clients[i], i);
            }
        }
    }

    close(server_fd);
    return 0;
}

