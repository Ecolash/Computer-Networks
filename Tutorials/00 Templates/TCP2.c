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

    while(1)
    {
        clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &len);
        if (fork() == 0)
        {
            recv(clientfd, buffer, sizeof(buffer), 0);
            printf("[+] Received from Client: %s\n", buffer);
            send(clientfd, message, MAXLINE, 0);
            close(clientfd);
            exit(0);
        }
    }
}

void client(int i)
{
    char buffer[100];
    char *message = "Hello Server from Client";

    char message2[100];
    sprintf(message2, "%s %d", message, i);

    int sockfd;
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    send(sockfd, message2, MAXLINE, 0);
    recv(sockfd, buffer, sizeof(buffer), 0);

    printf("[+] Received from Server: %s\n", buffer);
    close(sockfd);
}

int main(int argc, char *argv[])
{
    int pid;
    int n = atoi(argv[1]);
    pid = fork();
    switch (pid)
    {
        case -1: printf("Error in fork\n"); exit(1);
        case 0: 
            for (int i = 0; i < n; i++) {
                if (fork() == 0) {
                    sleep(i);
                    client(i);
                    exit(0);
                }
            }
            break;
        default: server(); break;
    }

    return 0;
}
