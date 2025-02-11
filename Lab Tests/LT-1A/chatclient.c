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

#define MAX_CLIENTS  5

typedef struct sockaddr* SPTR;

int main(int argc, char* argv[])
{
    int sfd;
    int numclient = 0;
    int clientsockfd[MAX_CLIENTS] = {0};
    char buffer[SIZE];

    struct sockaddr_in server, client;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {printf("Socket Creation failed! \n"); exit(1);}

    socklen_t len = sizeof(client);
    bzero(&server, sizeof(server));
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    int connected = connect(sfd, (SPTR)&server, sizeof(server));
    if (connected == -1) {printf("Connection failed! \n"); exit(1);}

    char* IP = inet_ntoa(server.sin_addr);
    int port = ntohs(server.sin_port);

    char self_info[1000];
    int nself = recv(sfd, self_info, SIZE, 0);
    self_info[nself] = '\0';
    printf("[+] Client <%s> : Connected to server <%s - %d>\n", self_info, IP , port);


    fd_set fds;
	int maxfd;

    while(1)
    {
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(sfd, &fds);

        maxfd = sfd;
		select(maxfd + 1, &fds, NULL, NULL, NULL);

		if (FD_ISSET(STDIN_FILENO, &fds))
        {
			int n = read(0, buffer, SIZE);
			buffer[n - 1] = '\0';
            send(sfd, buffer, n - 1, 0);
            printf("[>] Client <%s> : Message %s sent to server\n", self_info, buffer);
		}

        if (FD_ISSET(sfd, &fds))
        {
			int n = recv(sfd, buffer, SIZE, 0);
            if (n > 0)
            {
                buffer[n] = '\0';
                printf("[<] Client <%s> : ", self_info);
                printf("%s", buffer);
                fflush(stdout);
            }
		}
    }
}


