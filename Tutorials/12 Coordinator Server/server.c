#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define TCP_PORT    5555
#define UDP_PORT    5556
#define MAX_CLIENTS 5
#define BROADCAST "255.255.255.255"

struct client
{
    int sockfd;
    struct sockaddr_in caddr;
    socklen_t len;
    int allocated;
    int number;
};

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    int check = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (check == -1) 
    {
        printf("fcntl() - FD = %d", fd);
        exit(EXIT_FAILURE);
    }
}

void sort(int arr[], int n)
{
    int i, key, j;
    for (i = 1; i < n; i++) {
        key = arr[i];
        j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        arr[j + 1] = key;
    }
}

void send_results(char msg[])
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    set_nonblocking(sockfd);

    int reuse = 1;
    int bcast = 1;
    int opt1 = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int opt2 = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));
    if (opt1 < 0 || opt2 < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in broadcast = {0};
    broadcast.sin_addr.s_addr = inet_addr(BROADCAST);
    broadcast.sin_family = AF_INET;
    broadcast.sin_port = htons(UDP_PORT);

    int sent = -1;
    while (sent < 0)
    {
        sent = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&broadcast, sizeof(broadcast));
        if (sent < 0)
        {
            if (errno == EAGAIN) continue;
            if (errno == EWOULDBLOCK) continue;
            perror("send | non-bloking");
            exit(EXIT_FAILURE);
        }
        break;
    }

    printf("Result broadcasted successfully\n");
    close(sockfd);
}

int main()
{
    int serverfd;
    struct sockaddr_in saddr;
    socklen_t slen = sizeof(saddr);

    int connected = 0;
    int allocated = 0;
    struct client Clients[MAX_CLIENTS];

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        Clients[i].sockfd = -1;
        Clients[i].allocated = 0;
        Clients[i].len = sizeof(Clients[i].caddr);
        bzero(&Clients[i].caddr, sizeof(Clients[i].caddr));
    }

    bzero(&saddr, sizeof(saddr));
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(TCP_PORT);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    bind(serverfd, (struct sockaddr*)&saddr, sizeof(saddr));

    listen(serverfd, 5);
    set_nonblocking(serverfd);
    printf("[+] Server listening...\n");

    int flag = 1;
    while(flag)
    {
        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if (Clients[i].sockfd != -1) continue;
            Clients[i].sockfd = accept(serverfd, (struct sockaddr*) &(Clients[i].caddr), &(Clients[i].len));

            int sfd = Clients[i].sockfd; 
            if (sfd < 0)
            {
                if (errno == EAGAIN) continue;
                if (errno == EWOULDBLOCK) continue;
                perror("accept | non-bloking");
                exit(EXIT_FAILURE);            
            }
            set_nonblocking(Clients[i].sockfd);
            printf("[+] Client %d connected!\n", i + 1);
            connected++;
        }

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if (Clients[i].sockfd == -1) continue;
            if (Clients[i].allocated == 1) continue;

            char buff[100];
            int n = recv(Clients[i].sockfd, buff, sizeof(buff) - 1, 0);
            if (n < 0)
            {
                if (errno == EAGAIN) continue;
                if (errno == EWOULDBLOCK) continue;
                perror("recv | non-bloking");
                exit(EXIT_FAILURE);            
            }
            buff[n] = '\0';
            Clients[i].allocated = 1;
            Clients[i].number = atoi(buff);
            printf("[+] Client %d sent number %d!\n", i + 1, Clients[i].number);
        }

        flag = 0;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (Clients[i].sockfd == -1) flag = 1;
            if (Clients[i].allocated == 0) flag = 1;
        }
    }

    printf("\nAll clients sent numbers!\n");

    int arr[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) arr[i] = Clients[i].number;
    
    sort(arr, MAX_CLIENTS);
    char buff[1000] = {0};
    snprintf(buff, sizeof(buff), "[+] Sorted Array Results: %d %d %d %d %d", arr[0], arr[1], arr[2], arr[3], arr[4]);
    send_results(buff);
    return 0;
}