#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVER_PORT 6060
#define BUFFER_SIZE 1024

int main()
{
	int serverfd;
	struct sockaddr_in saddr, caddr;
	int len = sizeof(caddr);

	serverfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&saddr, sizeof(saddr));
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(SERVER_PORT);

	char buffer[BUFFER_SIZE];
	bind(serverfd, (struct sockaddr *)&saddr, sizeof(saddr));
	printf("[+] Server binded successfully!\n");

	while(1)
	{
		int n = recvfrom(serverfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&caddr, &len);
		buffer[n] = '\0';

		printf("Client > %s", buffer);
		int length = strlen(buffer);
		sendto(serverfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&caddr, sizeof(caddr));
	}

}