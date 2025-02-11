#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define TCP_PORT	 8080
#define UDP_PORT	 6060
#define BUFFER_SIZE	 1024

typedef struct sockaddr* SPTR; 

int main()
{
	fd_set S;
	char buffer[BUFFER_SIZE];
	int TCPfd, UDPfd, newfd = -1;
	struct sockaddr_in sTCP, sUDP, cTCP, cUDP;

	socklen_t lTCP = sizeof(cTCP);
	socklen_t lUDP = sizeof(cUDP);

	bzero(&sTCP, sizeof(sTCP));
	sTCP.sin_addr.s_addr = htonl(INADDR_ANY);
	sTCP.sin_family = AF_INET;
	sTCP.sin_port = htons(TCP_PORT);

	bzero(&sUDP, sizeof(sUDP));
	sUDP.sin_addr.s_addr = htonl(INADDR_ANY);
	sUDP.sin_family = AF_INET;
	sUDP.sin_port = htons(UDP_PORT);

	TCPfd = socket(AF_INET, SOCK_STREAM, 0);
	UDPfd = socket(AF_INET, SOCK_DGRAM, 0);

	int TCP1 = bind(TCPfd, (SPTR)&sTCP, sizeof(sTCP));
	int UDP1 = bind(UDPfd, (SPTR)&sUDP, sizeof(sUDP));
	if (TCP1 < 0 || UDP1 < 0) perror("[-] Error in binding");
	
	int check = listen(TCPfd, 5);
	if (check < 0) perror("[-] Error in listening");

	while(1)
	{
		FD_ZERO(&S);
		FD_SET(TCPfd, &S); FD_SET(UDPfd, &S);
		if (newfd >= 0) FD_SET(newfd, &S);

		int maxfd = TCPfd > UDPfd? TCPfd : UDPfd;
		maxfd = maxfd > newfd? maxfd : newfd;
		select(maxfd + 1, &S, NULL, NULL, NULL);
		if (FD_ISSET(TCPfd, &S)) 
		{
			newfd = accept(TCPfd, (SPTR)&cTCP, &lTCP);
			if (newfd < 0) perror("[-] Error in accepting");
		}

		if (FD_ISSET(newfd, &S))
		{
			int n = recv(newfd, buffer, BUFFER_SIZE, 0);
			if (n == 0) { close(newfd); newfd = -1; }
			else
			{
				buffer[n] = '\0';
				printf("TCP Client >> %s", buffer);
				send(newfd, buffer, n, 0);
			}
		}
		
		if (FD_ISSET(UDPfd, &S))
		{
			int n = recvfrom(UDPfd,  buffer, BUFFER_SIZE, 0, (SPTR)&cUDP, &lUDP);
			if (n != 0)
			{
				buffer[n] = '\0';
				printf("UDP Client >> %s", buffer);
				sendto(UDPfd, buffer, n, 0, (SPTR)&cUDP, sizeof(cUDP));
			}
		} 
	}
	close(UDPfd); close(TCPfd);
	return 0;
}