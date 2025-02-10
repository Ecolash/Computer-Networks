/* CSE39006: Sample Program
 * A simple UDP socket 
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
    int serverfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    serverfd = socket(AF_INET, SOCK_DGRAM, 0);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    bind(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("[+] Server Running .........\n");

    len = sizeof(cliaddr);
    int n = recvfrom(serverfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cliaddr, &len); 
    buffer[n] = '\0';
    printf("[+] Received from Client: %s\n", buffer);

    sendto(serverfd, message, MAXLINE, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
    printf("[+] Message sent to client\n");
    close(serverfd);
    exit(0);
}

void client()
{
    char buffer[100];
    char *message = "Hello Server";
    int sockfd, n;
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sendto(sockfd, message, MAXLINE, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

    recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
    printf("[+] Received from Server:%s\n", buffer);
    close(sockfd);
    exit(0);
}

int main()
{
    int pid;
    pid = fork();
    switch (pid)
    {
        case -1: printf("Error"); exit(1);
        case 0: client(); break;
        default: server(); break;
    }
    return 0;
}