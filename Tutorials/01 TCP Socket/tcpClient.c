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

char* format_time();

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[1024];

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    switch (clientSocket) {
        case -1: printf("[-] Error in connection.\n"); exit(1);
        default: printf("[+] Client Socket is created.\n"); break;
    }

    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int connected = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    switch (connected) {
        case -1: printf("[-] Error in connection.\n"); exit(1);
        default: printf("[+] Connected to Server.\n"); break;
    }

    recv(clientSocket, buffer, 1024, 0);
    char *timeStr = format_time();
    printf("[%s] Server: %s\n", timeStr, buffer);
    close(clientSocket);
    return 0;
}

char* format_time() {
    time_t currentTime;
    time(&currentTime);
    char *timeStr = ctime(&currentTime);
    timeStr[strcspn(timeStr, "\n")] = 0; 
    return timeStr;
}