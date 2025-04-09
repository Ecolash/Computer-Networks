#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

typedef struct sockaddr* sptr;

#define PORT        9090
#define SERVER_IP   "127.0.0.1"  

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    int reuse = 1;
    int opt1 = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in saddr = {0};
    saddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);

    connect(sockfd, (sptr)&saddr, sizeof(saddr));
    printf("[+] Connected successfully\n");
    
    while(1)
    {
        char msg[1000];
        printf("Enter message: ");
        fflush(stdout);

        scanf(" %[^\n]s", msg);
        printf("Message sent to server: %s\n", msg);
        send(sockfd, msg, strlen(msg), 0);

        char echomsg[1000];
        int n = recv(sockfd, echomsg, sizeof(echomsg), 0);
        // printf("%d\n", n);
        if (n == 0) break;
        echomsg[n] = '\0';

        printf("Message received from server: %s\n", echomsg);
    }
    printf("CLient disconnected\n");
}