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
#define SIZE 40

char *format_time()
{
    time_t currentTime;
    time(&currentTime);
    struct tm *localTime = localtime(&currentTime);
    static char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localTime);
    return timeStr;
}

int write_file(int sockfd, struct sockaddr_in addr) 
{
    int n = 0;
    char buffer[SIZE] = {0};
    char filename[SIZE] = {0};

    socklen_t addr_size = sizeof(addr);
    int filercv = recvfrom(sockfd, filename, SIZE, 0, (struct sockaddr*)&addr, &addr_size);
    switch(filercv) {
        case -1: printf("[-] Error in receiving file name.\n"); return -1;
        default: break;
    }

    if (strcmp(filename, "NOT_FOUND") == 0) {
        printf("[-] File not found on client.\n");
        return -1;
    }
    printf("[+] File name received: %s\n", filename);
    bzero(filename, SIZE);
    strcpy(filename, "server.txt");
    FILE *file = fopen(filename, "w");

    while(1) {
        int received = recvfrom(sockfd, buffer, SIZE, 0, (struct sockaddr*)&addr, &addr_size);
        switch(received) {
            case -1: printf("[-] Error in receiving data.\n"); return -1;
            default: break;
        }
        if (strcmp(buffer, "EOF") == 0) break;
        char *timeStr = format_time();
        fprintf(file, "%s", buffer);
        printf("[%s] Packet [%d] received\n", timeStr, ++n);
        bzero(buffer, SIZE);
    }
    printf("[+] %d packets received.\n", n);
    fclose(file);
    return 0;
}

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;

    int newSocket;
    struct sockaddr_in newAddr;
    socklen_t addr_size;

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
        default: printf("[+] Binded to port %d\n", PORT); break;
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

    int status = write_file(newSocket, newAddr);
    switch(status) {
        case -1: printf("[-] Error in writing file.\n"); exit(1);
        default: printf("[+] Received can be found in server.txt\n"); break;
    }
    close(sockfd);
    close(newSocket);
    return 0;
}

