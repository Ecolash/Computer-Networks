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
int round_number = 1;
int numbers[MAX_CLIENTS] = {-1};

/* Send a message to all clients */
void broadcast_message(const char *message) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i] > 0) {
            send(clients[i], message, strlen(message), 0);
        }
    }
}

/* handle new connection requests */
void handle_new_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (new_client_fd < 0) {
        perror("Accept failed");
        return;
    }

    if (client_count >= MAX_CLIENTS) {
        printf("Server: Maximum client limit reached. Connection rejected.\n");
        close(new_client_fd);
        return;
    }

    clients[client_count] = new_client_fd;
    numbers[client_count] = -1;
    client_count++;

    printf("Server: New client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE, "Server: Send your number for Round %d\n", round_number);
    send(new_client_fd, msg, strlen(msg), 0);
}

/*Process client message*/

void process_client_message(int client_fd, int index) {
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

    if (bytes_read <= 0) {
        printf("Server: Client disconnected.\n");
        close(client_fd);
        clients[index] = 0;
        return;
    }

    buffer[bytes_read] = '\0';
    int num = atoi(buffer);

    if (numbers[index] != -1) {
        char msg[BUFFER_SIZE];
        snprintf(msg, BUFFER_SIZE, "Server: Duplicate messages for Round %d are not allowed.\n", round_number);
        send(client_fd, msg, strlen(msg), 0);
        return;
    }

    numbers[index] = num;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    /*Rather than using getpeername, you can also pass the client address details to this function */
    getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len);

    printf("Server: Received %d from %s:%d\n", num, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    int received_count = 0, max_number = -1, max_index = -1;
    for (int i = 0; i < client_count; i++) {
        if (numbers[i] != -1) {
            received_count++;
            if (numbers[i] > max_number) {
                max_number = numbers[i];
                max_index = i;
            }
        }
    }

    if (received_count == client_count) {
        struct sockaddr_in max_addr;
        socklen_t max_addr_len = sizeof(max_addr);
        getpeername(clients[max_index], (struct sockaddr*)&max_addr, &max_addr_len);

        char result_msg[BUFFER_SIZE];
        snprintf(result_msg, BUFFER_SIZE, "Server: Maximum Number Received in Round %d is: %d. From %s:%d\n",
                 round_number, max_number, inet_ntoa(max_addr.sin_addr), ntohs(max_addr.sin_port));
        broadcast_message(result_msg);

        round_number++;
        memset(numbers, -1, sizeof(numbers));

        char next_round_msg[BUFFER_SIZE];
        snprintf(next_round_msg, BUFFER_SIZE, "Server: Enter the number for Round %d:\n", round_number);
        broadcast_message(next_round_msg);
    }
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

    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(atoi(argv[1])), .sin_addr.s_addr = INADDR_ANY};
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

