#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

const int PORT = 4032;
const int SIZE = 1024;
const int MAX_CLIENTS = 5;
#define MAX_USERNAME 50

typedef struct sockaddr *SPTR;

typedef struct
{
    int sockfd;
    char username[MAX_USERNAME];
    struct sockaddr_in addr;
} Client;

void timestamp(char *_time)
{
    time_t now = time(NULL);
    strftime(_time, 20, "%I:%M %p", localtime(&now));
}

void welcome()
{
    printf("\033[H\033[J");
    printf("\n\033[1mWelcome to ChatRoom\033[0m\n\n");
}

int main(int argc, char *argv[])
{
    fd_set fds;
    int sfd, maxfd, numclient = 0;
    char buffer[SIZE];
    Client clients[50] = {0};

    struct sockaddr_in server;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)  { printf("Socket Creation failed!\n");  exit(1);}

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&server, sizeof(server));
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    if (bind(sfd, (SPTR)&server, sizeof(server)) == -1) { printf("Bind failed!\n"); exit(1); }
    if (listen(sfd, 5) == -1) { printf("Listen failed!\n"); exit(1); }
    welcome();

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(sfd, &fds);
        maxfd = sfd;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].sockfd > 0)
            {
                FD_SET(clients[i].sockfd, &fds);
                maxfd = (maxfd > clients[i].sockfd) ? maxfd : clients[i].sockfd;
            }
        }

        select(maxfd + 1, &fds, NULL, NULL, NULL);

        if (FD_ISSET(sfd, &fds) && numclient < MAX_CLIENTS)
        {
            socklen_t len = sizeof(struct sockaddr_in);
            int newfd = accept(sfd, (SPTR)&clients[numclient].addr, &len);

            char username[MAX_USERNAME];
            int n = recv(newfd, username, MAX_USERNAME, 0);
            username[n] = '\0';

            clients[numclient].sockfd = newfd;
            strncpy(clients[numclient].username, username, MAX_USERNAME);

            char *IP = inet_ntoa(clients[numclient].addr.sin_addr);
            int port = ntohs(clients[numclient].addr.sin_port);

            char time[20];
            timestamp(time);
            printf("[%s] %s joined the chat\n", time, username);

            char join[SIZE];
            sprintf(join, "\033[1m[%s]\033[0m \033[1;32m%s\033[0m joined the chat!", time, username);
            for (int i = 0; i < numclient; i++) if (clients[i].sockfd > 0) send(clients[i].sockfd, join, strlen(join), 0);
            numclient++;
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].sockfd > 0 && FD_ISSET(clients[i].sockfd, &fds))
            {
                int currfd = clients[i].sockfd;
                int status = recv(currfd, buffer, SIZE, 0);

                if (status > 0)
                {
                    buffer[status] = '\0';
                    char time[20];
                    timestamp(time);

                    char formatted_msg[SIZE];
                    sprintf(formatted_msg, "\033[1m[%s]\033[0m \033[1;36m%s\033[0m: %s", time, clients[i].username, buffer);
                    printf("%s\n", formatted_msg);
                    for (int j = 0; j < MAX_CLIENTS; j++)
                    {
                        if (clients[j].sockfd <= 0) continue;
                        if (clients[j].sockfd == currfd) continue;
                        send(clients[j].sockfd, formatted_msg, strlen(formatted_msg), 0);
                    }
                }
                else if (status == 0)
                {
                    char time[20];
                    timestamp(time);
                    printf("[%s] %s left the chat\n", time, clients[i].username);

                    char left[SIZE];
                    sprintf(left, "\033[1m[%s]\033[0m 👋 \033[1;31m%s\033[0m left the chat!", time, clients[i].username);
                    for (int j = 0; j < MAX_CLIENTS; j++)
                    {
                        if (clients[j].sockfd <= 0) continue;
                        if (clients[j].sockfd == currfd) continue;
                        send(clients[j].sockfd, left, strlen(left), 0);
                    }

                    close(clients[i].sockfd);
                    clients[i].sockfd = 0;
                    memset(clients[i].username, 0, MAX_USERNAME);
                    // numclient--;
                }
            }
        }
    }
}