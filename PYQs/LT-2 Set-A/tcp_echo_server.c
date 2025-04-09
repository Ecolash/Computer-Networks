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

#define PORT    9090  

typedef struct sockaddr* sptr;

char IP[INET_ADDRSTRLEN];
int cport = 0;

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    int check = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (check < 0) {
        perror("fcntl ()");
        exit(1);
    }
    printf("[+] Socket %d set to non-blocking mode\n\n", fd);
    return;
}

void set_socket_options(int fd) 
{
    int reuse = 1;
    int size = 8 * 1024;

    int opt1 = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int opt2 = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (opt1 < 0 || opt2 < 0) 
    {
        perror("setsockopt()");
        exit(1);
    }

    int retsize;
    int len = sizeof(retsize);
    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &retsize, &len);
    printf("[+] Receive buffer size set to 8KB (retsize = %d)\n", retsize);
}

int create_server_socket(int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    set_socket_options(sockfd);
    struct sockaddr_in saddr = {0};
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    int binded = bind(sockfd, (sptr)&saddr, sizeof(saddr));
    if (binded < 0) 
    {
        perror("bind()");
        exit(1);
    }
    return sockfd;
}

void handle_client(int fd)
{
    set_socket_options(fd);
    set_nonblocking(fd);
    while(1) {
        char echomsg[1000];
        bzero(&echomsg, sizeof(echomsg));
        int n = recv(fd, echomsg, sizeof(echomsg), 0);
        if (n < 0) 
        {
            if (errno == EAGAIN) continue;
            if (errno == EWOULDBLOCK) 
            {
                printf("[+] Client from %s : %d has disconnected abruptly\n", IP, cport);
                exit(1);
            };
            printf("[+] Client from %s : %d has disconnected abruptly\n", IP, cport);
            exit(1);
        }

        if (n == 0) break;
        echomsg[n] = '\0';
        printf("Message received from client: %s\n", echomsg);
        // printf("%s", echomsg);

        char msg[1000];
        strcpy(msg, echomsg);
        int sent = -1;
        while (sent < 0) 
        {
            int sent = send(fd, msg, strlen(msg), 0);
            if (sent > 0) break;
            
            if (errno == EAGAIN) continue;
            if (errno == EWOULDBLOCK) 
            {
                printf("[+] Client from %s : %d has disconnected abruptly\n", IP, cport);
                exit(1);
            };
            printf("[+] Client from %s : %d has disconnected abruptly\n", IP, cport);
            exit(1);
        }
        printf("Message sent to client: %s\n", msg);
    }
    printf("[+] Client from %s : %d has disconnected normally\n", IP, cport);
    exit(0);
}


int main()
{
    int clientfd;
    struct sockaddr_in caddr = {0};
    socklen_t len = sizeof(caddr);

    int serverfd = create_server_socket(PORT);
    set_nonblocking(serverfd);
    listen(serverfd, 5);

    printf("[+] Server ready to accept connections...\n");
    
    while(1)
    {
        clientfd = accept(serverfd, (sptr)&caddr, &len);
        if (clientfd < 0) 
        {
            if (errno == EAGAIN) continue;
            if (errno == EWOULDBLOCK) continue;
            perror("accept() | non-blocking");
            exit(1);
        }
        
        inet_ntop(AF_INET, &(caddr.sin_addr), IP, INET_ADDRSTRLEN);
        cport = ntohs(caddr.sin_port);
        pid_t pid = fork();
        switch(pid)
        {
            case -1:
                close(serverfd);
                close(clientfd);
                perror("fork ()");
                exit(1);

            case 0: 
                close(serverfd);
                handle_client(clientfd);
                exit(0);

            default:
                printf("[+] Client connected %s : %d\n", IP, cport);
                close(clientfd);
        }
    }
}