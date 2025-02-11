#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6060
#define BUFFER_SIZE 1024

int main()
{
    int sock;
    struct sockaddr_in saddr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    saddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(SERVER_PORT);

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

        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&saddr, sizeof(saddr));
        int bytes_read = recvfrom(sock, buffer, BUFFER_SIZE, 0, NULL, NULL);
        buffer[bytes_read] = '\0';
        printf("Server > %s", buffer);
    }

    close(sock);
    return 0;
}
