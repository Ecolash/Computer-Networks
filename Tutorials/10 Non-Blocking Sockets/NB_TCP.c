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
    int serverfd;
    int clientfd;
    int n;
    
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    memset(&servaddr, 0, sizeof(servaddr));

    char buffer[100];
    char message[] = "Hello Client";
    socklen_t len = sizeof(cliaddr);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    set_nonblocking(serverfd);
    
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    bind(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(serverfd, 5);
    printf("[+] Server Running (non-blocking) ...\n");
    clientfd = -1;
    while (clientfd < 0)
    {
        clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &len);
        if (clientfd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[.] No incoming connection yet, retrying accept...\n");
            usleep(500000);
        }
        else if (clientfd < 0)
        {
            perror("accept");
            exit(1);
        }
    }
    set_nonblocking(clientfd);
    n = -1;
    while (n < 0)
    {
        n = recv(clientfd, buffer, sizeof(buffer) - 1, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] No data received yet, waiting...\n");
            usleep(500000);
        }
        else if (n < 0)
        {
            perror("recv");
            exit(1);
        }
    }
    buffer[n] = '\0';
    printf("[+] Server received: %s\n", buffer);
    int sent = -1;
    sleep(3);
    while (sent < 0)
    {
        sent = send(clientfd, message, strlen(message), 0);
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] Send buffer full, retrying send...\n");
            usleep(500000);
        }
        else if (sent < 0)
        {
            perror("send");
            exit(1);
        }
    }
    printf("[+] Message sent to client\n");
    close(clientfd);
    close(serverfd);
}

void client()
{
    int sockfd, n;
    char buffer[100];
    char message[] = "Hello Server";

    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    set_nonblocking(sockfd);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    int res = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (res < 0 && errno == EINPROGRESS)
    {
        fd_set wfds;
        struct timeval tv;
        FD_ZERO(&wfds);
        FD_SET(sockfd, &wfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        res = select(sockfd + 1, NULL, &wfds, NULL, &tv);
        if (res <= 0)
        {
            perror("connect/select");
            exit(1);
        }
        int so_error;
        socklen_t len = sizeof(so_error);

        // SO_ERROR is used to check if the connection was successful
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0)
        {
            errno = so_error;
            perror("connect/getsockopt");
            exit(1);
        }
    }
    else if (res < 0)
    {
        perror("connect");
        exit(1);
    }
    printf("[+] Client connected (non-blocking)\n");
    int sent = -1;
    sleep(2);
    while (sent < 0)
    {
        sent = send(sockfd, message, strlen(message), 0);
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] Client send busy, retrying...\n");
            usleep(500000);
        }
        else if (sent < 0)
        {
            perror("send");
            exit(1);
        }
    }
    int total = -1;
    while (total < 0)
    {
        total = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (total < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("[*] Client waiting for data...\n");
            usleep(500000);
        }
        else if (total < 0)
        {
            perror("recv");
            exit(1);
        }
    }
    buffer[total] = '\0';
    printf("[+] Client received: %s\n", buffer);
    close(sockfd);
}

int main()
{
    int pid = fork();
    switch(pid)
    {
        case -1: perror("fork"); exit(1);
        case 0:  server(); break;
        default: sleep(1); client(); break;
    }
    return 0;
}