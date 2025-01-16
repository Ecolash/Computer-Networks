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

int receive_file(int sockfd) {
    char filename[SIZE] = {0};
    char buffer[SIZE] = {0};

    recv(sockfd, filename, SIZE, 0);
    if (strcmp(filename, "NOT_FOUND") == 0) { printf("[-] File not found on client side.\n"); return -1; }

    printf("[+] Receiving file '%s'...\n", filename);
    FILE *file = fopen(filename, "wb");
    if (!file) { perror("[-] Error opening file for writing"); return -1;}

    while (1) {
        int bytesReceived = recv(sockfd, buffer, SIZE, 0);
        if (bytesReceived == 0) break;
        if (bytesReceived < 0) {
            perror("[-] Error in receiving data");
            fclose(file);
            return -1;
        }

        if (strcmp(buffer, "EOF") == 0) {
            printf("[+] End of file received.\n");
            break;
        }

        char *timeStr = format_time();
        float kbytes = (1.0 * bytesReceived) / 1024.0;
        printf("[%s] Received %2.2f KB...\n", timeStr, kbytes);
        fwrite(buffer, 1, bytesReceived, file);
        bzero(buffer, SIZE);
    }

    printf("[+] File '%s' received successfully.\n", filename);
    fclose(file);
    return 0;
}

int main() {
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr, newAddr;
    socklen_t addr_size;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)  { perror("[-] Error in socket creation"); exit(1); }
    printf("[+] Server socket created successfully.\n");

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    switch (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
        case 0: printf("[+] Binding to port %d successful.\n", PORT); break;
        default: perror("[-] Error in binding"); close(serverSocket); exit(1);
    }

    switch (listen(serverSocket, 10)) {
        case 0: printf("[+] Listening for incoming connections...\n"); break;
        default: perror("[-] Error in listening"); close(serverSocket); exit(1);
    }

    addr_size = sizeof(newAddr);
    newSocket = accept(serverSocket, (struct sockaddr*)&newAddr, &addr_size);
    switch (newSocket < 0) {
        case 0: printf("[+] Connection accepted from %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port)); break;
        default: perror("[-] Error in accepting connection"); close(serverSocket); exit(1);
    }

    int receive_status = receive_file(newSocket);
    switch (receive_status) {
        case 0:  printf("[+] File received successfully.\n"); break;
        case -1: printf("[-] Failed to receive file.\n"); break;
        default: printf("[-] Unknown error occurred.\n");
    }

    close(newSocket);
    close(serverSocket);
    printf("[+] Server socket closed.\n");
    return 0;
}
