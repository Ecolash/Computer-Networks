/* CSE39006: Sample Program
 * A simple TCP socket
 */

#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5000
#define MAXLINE 1000

void server()
{
    char buffer[100];
    char *message = "Hello Client";
    int serverfd, clientfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;

    bzero(&servaddr, sizeof(servaddr));
    serverfd = socket(AF_INET, SOCK_STREAM, 0);

    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    bind(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(serverfd, 5);

    printf("[+] Server Running .........\n");

    len = sizeof(cliaddr);
    clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &len);

    int n = recv(clientfd, buffer, sizeof(buffer), 0);
    buffer[n] = '\0';
    printf("[+] Received from Client: %s\n", buffer);

    send(clientfd, message, MAXLINE, 0);
    printf("[+] Message sent to client\n");

    close(clientfd);
    close(serverfd);
}

void client()
{
    char buffer[100];
    char *message = "Hello Server";
    int sockfd;
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    send(sockfd, message, MAXLINE, 0);
    recv(sockfd, buffer, sizeof(buffer), 0);

    printf("[+] Received from Server: %s\n", buffer);
    close(sockfd);
}

int main()
{
    int pid;
    pid = fork();
    switch (pid)
    {
        case -1: printf("Error in fork\n"); exit(1);
        case 0: sleep(1); client(); break;
        default: server(); break;
    }

    return 0;
}
