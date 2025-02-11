#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main()
{
    int sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    int x = connect(sock, (struct sockaddr *)&server_address, sizeof(server_address));
    printf("Connected to server. Type messages (type 'exit' to quit):\n");
    while (1)
    {
        printf("Client > ");
        fgets(buffer, BUFFER_SIZE, stdin);
        if (strncmp(buffer, "exit", 4) == 0)
        {
            printf("Exiting...\n");
            break;
        }

        send(sock, buffer, strlen(buffer), 0);
        int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
        buffer[bytes_read] = '\0';
        printf("Server > %s", buffer);
    }

    close(sock);
    return 0;
}
