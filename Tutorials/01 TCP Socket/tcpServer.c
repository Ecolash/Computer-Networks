#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080

char *format_time();

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;

    int newSocket;
    struct sockaddr_in newAddr;

    socklen_t addr_size;
    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    switch (sockfd) {
        case -1: printf("[-] Error in socket.\n"); exit(1);
        default: printf("[+] Server socket created.\n"); break;
    }

    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int binded = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    switch (binded) {
        case -1: printf("[-] Error in binding.\n"); exit(1);
        default: printf("[+] Binded to port %d.\n", PORT); break;
    }

    int listening = listen(sockfd, 10);
    switch (listening) {
        case -1: printf("[-] Error in listening.\n"); exit(1);
        default: printf("[+] Listening...\n"); break;
    }

    addr_size = sizeof(newAddr);
    newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
    switch (newSocket) {
        case -1: printf("[-] Error in accepting.\n"); exit(1);
        default: printf("[+] Connection accepted from %s:%d.\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port)); break;
    }


    printf("[*] Enter message: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;

    char *timeStr = format_time();
    send(newSocket, buffer, 1024, 0);
    printf("[%s] Message sent: %s\n", timeStr, buffer);

    close(sockfd);
    close(newSocket);
    return 0;
}

char *format_time()
{
    time_t currentTime;
    time(&currentTime);
    char *timeStr = ctime(&currentTime);
    timeStr[strcspn(timeStr, "\n")] = 0;
    return timeStr;
}