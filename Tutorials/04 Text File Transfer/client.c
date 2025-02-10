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

int send_file(char* filename, int sockfd, struct sockaddr_in addr) 
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("[-] Error in reading file.\n");
        bzero(filename, SIZE);
        strcpy(filename, "NOT_FOUND");
        send(sockfd, filename, SIZE, 0);
        return -1;
    }

    send(sockfd, filename, SIZE, 0);
    char buffer[SIZE] = {0};
    while (fgets(buffer, SIZE, file) != NULL) 
    {
        char *timeStr = format_time();
        printf("[%s] Sending Data: %s\n", timeStr, buffer);
        int sent = send(sockfd, buffer, SIZE, 0);
        switch(sent) {
            case -1: printf("[-] Error in sending data.\n"); return -1;
            default: break;
        }
        bzero(buffer, SIZE);
    }

    strcpy(buffer, "EOF");
    send(sockfd, buffer, SIZE, 0);
    fclose(file);
    return 0;
}

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

    char fileName[SIZE];
    printf("[*] Enter file name: ");
    scanf("%s", fileName);

    int sent = send_file(fileName, clientSocket, serverAddr);
    switch (sent) {
        case -1: printf("[-] Error in sending file.\n"); exit(1);
        default: printf("[+] File data sent successfully.\n"); break;
    }

    close(clientSocket);    
    printf("[+] Client socket disconnected from server\n");
    return 0;
}
