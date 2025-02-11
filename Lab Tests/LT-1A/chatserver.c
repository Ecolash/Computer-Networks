/*
==========================================================================
COMPUTER NETWORKS : LAB TEST - 1
--------------------------------------------------------------------------
NAME - TUHIN MONDAL
ROLL - 22CS10087
--------------------------------------------------------------------------
QUESTION SET - A
==========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

const int PORT = 4032;
const int SIZE = 100;
const int MAX_CLIENTS = 5;

typedef struct sockaddr* SPTR;

int main(int argc, char* argv[])
{
    int sfd;
    int numclient = 0;
    int clientsockfd[5] = {0};
    char buffer[SIZE];

    struct sockaddr_in server;
    struct sockaddr_in clients[MAX_CLIENTS];
    socklen_t len[MAX_CLIENTS];

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {printf("Socket Creation failed! \n"); exit(1);}

    bzero(&server, sizeof(server));
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    int binded = bind(sfd, (SPTR)&server, sizeof(server));
    if (binded == -1) {printf("Bind failed! \n"); close(sfd); exit(1);}

    int listening = listen(sfd, 5);
    if (listening == -1) {printf("Socket Creation failed! \n"); exit(1);}

    fd_set S;
    int maxfd;
    for(int i = 0; i < MAX_CLIENTS; i++) len[i] = sizeof(clients[i]);

    while(1)
    {
		FD_ZERO(&S);
		FD_SET(sfd, &S);
        maxfd = sfd;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clientsockfd[i] > 0) FD_SET(clientsockfd[i], &S);
            maxfd = (maxfd > clientsockfd[i])? maxfd : clientsockfd[i];
        }
		
		select(maxfd + 1, &S, NULL, NULL, NULL);

		if (FD_ISSET(sfd, &S) && numclient < MAX_CLIENTS) 
        {
			clientsockfd[numclient] = accept(sfd, (SPTR)&clients[numclient], &len[numclient]);
            char* IP = inet_ntoa(clients[numclient].sin_addr);
            int port = ntohs(clients[numclient].sin_port);
            printf("[+] Server : Received a connection from client <%s - %d>\n", IP , port);

            char self_info[1000];
            sprintf(self_info, "%s - %d", IP , port);
            int len2 = strlen(self_info);
            send(clientsockfd[numclient], self_info, len2, 0);
            numclient++;
		}

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if (FD_ISSET(clientsockfd[i], &S)) 
            {
                int currfd = clientsockfd[i];
                // printf("%d - im active\n", currfd);
                char* IP = inet_ntoa(clients[i].sin_addr);
                int port = ntohs(clients[i].sin_port);

                int status = recv(currfd, buffer, SIZE, 0);
                if (status == 0) continue;
                if (status > 0)
                {
                    int n = status;
                    buffer[n] = '\0';
                    printf("[<] Server: Received message %s from client <%s : %d>\n", buffer, IP, port);
                }

                if (numclient < 2) 
                {
                    printf("[-] Server : Insufficient Clients!");
                    printf(" Message from Client <%s - %d> dropped! \n", IP, port);
                    continue;
                }

                for(int j = 0; j < MAX_CLIENTS; j++)
                {
                    int tosend = clientsockfd[j];
                    if (tosend <= 0) continue;
                    if (tosend == currfd) continue;
                    char success_msg[1000];

                    char* IP2 = inet_ntoa(clients[j].sin_addr);
                    int port2 = ntohs(clients[j].sin_port);

                    printf("[>] Server: Send message %s from client <%s : %d> to <%s : %d>\n", buffer, IP, port, IP2, port2);
                    sprintf(success_msg, "Received message %s from client %s : %d\n", buffer, IP, port);
                    int msglen = strlen(success_msg);
                    send(tosend, success_msg, msglen, 0);
                }
            }
        }
	}
}


