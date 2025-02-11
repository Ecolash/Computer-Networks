#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int main()
{
    int server_fd, new_socket;
    int client_fds[MAX_CLIENTS] = {0};
    struct sockaddr_in address;
    fd_set readfds;

    int max_fd, addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, MAX_CLIENTS);

    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_fds[i] > 0)
            {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > max_fd)
                {
                    max_fd = client_fds[i];
                }
            }
        }

        select(max_fd + 1, &readfds, NULL, NULL, NULL); 
        if (FD_ISSET(server_fd, &readfds))
        {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            if (new_socket >= 0)
            {
                printf("New client connected: FD %d\n", new_socket);
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_fds[i] == 0)
                    {
                        client_fds[i] = new_socket;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int fd = client_fds[i];
            if (fd > 0 && FD_ISSET(fd, &readfds))
            {
                int bytes_read = recv(fd, buffer, BUFFER_SIZE, 0);
                if (bytes_read == 0)
                {
                    printf("Client disconnected: FD %d\n", fd);
                    close(fd);
                    client_fds[i] = 0;
                }
                else
                {
                    buffer[bytes_read] = '\0';
                    printf("Received: %s", buffer);
                    send(fd, buffer, bytes_read, 0);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
