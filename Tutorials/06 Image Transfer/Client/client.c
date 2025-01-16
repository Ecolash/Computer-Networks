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
#define SIZE 20480

char *format_time() {
    time_t currentTime;
    time(&currentTime);
    struct tm *localTime = localtime(&currentTime);
    static char timeStr[9]; 
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localTime);
    return timeStr;
}

int send_file(char* filename, int sockfd) {
    FILE *file = fopen(filename, "rb"); 
    if (file == NULL) {
        printf("[-] Error: Could not open file '%s'.\n", filename);
        strcpy(filename, "NOT_FOUND");
        send(sockfd, filename, SIZE, 0);
        return -1;
    }

    send(sockfd, filename, SIZE, 0);
    fseek(file, 0L, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    printf("[+] File size: %.2f KB\n", fileSize / 1024.0);

    char buffer[SIZE];
    size_t bytesRead;
    printf("[+] Sending file '%s'...\n", filename);
    while ((bytesRead = fread(buffer, 1, SIZE, file)) > 0) {
        char *timeStr = format_time();
        float kbytes = (1.0 * bytesRead) / 1024.0;
        printf("[%s] Sending %2.2f KB...\n", timeStr, kbytes);
        if (send(sockfd, buffer, bytesRead, 0) == -1) {
            perror("[-] Error in sending data");
            fclose(file);
            return -1;
        }
    }

    bzero(buffer, SIZE);
    strcpy(buffer, "EOF");
    send(sockfd, buffer, SIZE, 0);
    printf("[+] File '%s' sent successfully.\n", filename);

    fclose(file);
    return 0;
}

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[SIZE];

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("[-] Error in socket creation");
        exit(1);
    }
    printf("[+] Client socket created successfully.\n");
    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[-] Error in connection");
        close(clientSocket);
        exit(1);
    }
    printf("[+] Connected to server.\n");

    char fileName[SIZE];
    printf("[*] Enter file name to send: ");
    scanf("%s", fileName);

    if (send_file(fileName, clientSocket) == -1) {
        printf("[-] Failed to send file.\n");
        close(clientSocket);
        exit(1);
    }

    close(clientSocket);
    printf("[+] Client socket disconnected.\n");
    return 0;
}
