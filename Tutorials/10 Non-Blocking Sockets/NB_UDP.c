#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#define PORT    5000
#define MAXLINE 1000

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void server()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    set_nonblocking(sockfd);
    
    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    
    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("[+] UDP Server Running (non-blocking) ...\n");
    
    char buffer[100];
    socklen_t len = sizeof(cliaddr);
    int n = -1;
    while (n < 0)
    {
        n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] UDP Server waiting for data...\n");
            usleep(500000);
        }
        else if (n < 0)
        {
            perror("recvfrom");
            exit(1);
        }
    }
    buffer[n] = '\0';
    printf("[+] UDP Server received: %s\n", buffer);
    
    char message[] = "Hello UDP Client";
    int sent = -1;
    sleep(2);
    while (sent < 0)
    {
        sent = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&cliaddr, len);
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] UDP Server send busy, retrying...\n");
            usleep(500000);
        }
        else if (sent < 0)
        {
            perror("sendto");
            exit(1);
        }
    }
    printf("[+] UDP Server sent reply\n");
    close(sockfd);
}

void client()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    set_nonblocking(sockfd);
    
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    
    char message[] = "Hello UDP Server";
    char buffer[100];
    int sent = -1;
    sleep(1);
    while (sent < 0)
    {
        sent = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] UDP Client send busy, retrying...\n");
            usleep(500000);
        }
        else if (sent < 0)
        {
            perror("sendto");
            exit(1);
        }
    }
    printf("[+] UDP Client sent message\n");
    
    socklen_t len = sizeof(servaddr);
    int n = -1;
    while (n < 0)
    {
        n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&servaddr, &len);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] UDP Client waiting for reply...\n");
            usleep(500000);
        }
        else if (n < 0)
        {
            perror("recvfrom");
            exit(1);
        }
    }
    buffer[n] = '\0';
    printf("[+] UDP Client received: %s\n", buffer);
    close(sockfd);
}

int main()
{
    int pid = fork();
    switch (pid)
    {
        case -1: perror("fork"); exit(1);
        case 0:  server(); break;
        default: sleep(1); client(); break;
    }
    return 0;
}