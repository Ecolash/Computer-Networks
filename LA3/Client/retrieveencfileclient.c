#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define KEYSIZE     26
#define PORT        8080
#define SIZE        100

int check_key(const char *key)
{
    if (strlen(key) != KEYSIZE)
    {
        printf("[-] Key must be 26 characters long.\n");
        return 0;
    }
    int frequency[KEYSIZE] = {0};
    for (int i = 0; i < KEYSIZE; i++)
    {
        char ch = toupper(key[i]);
        if (ch < 'A' || ch > 'Z' || ++frequency[ch - 'A'] > 1) 
        {
            printf("[-] Key must contain unique alphabets.\n");
            return 0;
        }
    }
    return 1;
}

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
    char buffer[SIZE] = {0};
    FILE *file = fopen(filename, "r");
    if (file == NULL) 
    {
        printf("[-] NOT FOUND %s\n", filename);
        return -1;
    }
    while (fgets(buffer, SIZE, file) != NULL) 
    {
        char *timeStr = format_time();
        int sent = send(sockfd, buffer, SIZE, 0);
        switch(sent) {
            case -1: printf("[-] Error in sending data.\n"); return -1;
            default: break;
        }
        memset(buffer, 0, SIZE);
    }

    strcpy(buffer, "EOF");
    send(sockfd, buffer, strlen(buffer), 0);
    fclose(file);
    return 0;
}

int write_encrypted_file(int sockfd, struct sockaddr_in addr, const char *filename) 
{
    int n = 0;
    char enc_filename[SIZE];
    snprintf(enc_filename, SIZE, "%s.enc", filename);
    char buffer[SIZE] = {0};
    socklen_t addr_size = sizeof(addr);
    FILE *file = fopen(enc_filename, "w");

    while(1) {
        int received = recv(sockfd, buffer, SIZE, 0);
        switch(received) {
            case -1: printf("[-] Error in receiving data.\n"); return -1;
            default: break;
        }
        if (strcmp(buffer, "EOF") == 0) break;
        char *timeStr = format_time();
        fprintf(file, "%s", buffer);
        bzero(buffer, SIZE);
        n++;
    }
    printf("[+] Received %d packets from server.\n", n);
    fclose(file);
    return n;
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
    char ENCRYPTION_KEY[KEYSIZE + 1];
    int check = 0;

    do {
        printf("[*] Enter encryption key: ");
        scanf("%s", ENCRYPTION_KEY);
        ENCRYPTION_KEY[KEYSIZE] = '\0';
        check = check_key(ENCRYPTION_KEY);
    } while (!check);

    int send_status = send(clientSocket, ENCRYPTION_KEY, KEYSIZE + 1, 0);
    switch (send_status) {
        case -1: printf("[-] Error in sending encryption key.\n"); exit(1);
        default: printf("[+] Encryption key sent successfully.\n"); break;
    }

    int flag = 0;
    do {
        printf("[*] Enter file name: ");
        scanf("%s", fileName);
        FILE *file = fopen(fileName, "r");

        char file_response[SIZE];
        memset(file_response, 0, SIZE);

        if (file == NULL)
        {
            strcpy(file_response, "NOT_FOUND");

            send(clientSocket, file_response, strlen(file_response), 0);
            printf("[-] File: %s not found.\n", fileName);
        }
        else 
        {
            fclose(file);
            strcpy(file_response, "FOUND");
            send(clientSocket, file_response, strlen(file_response), 0);
            int send_status = send_file(fileName, clientSocket, serverAddr);
            switch (send_status) {
                case -1: printf("[-] Error in sending file name.\n"); exit(1);
                default: break;
            }

            int write_status = write_encrypted_file(clientSocket, serverAddr, fileName);
            switch (write_status) {
                case -1: printf("[-] Error in writing encrypted file.\n"); exit(1);
                default: printf("[+] Received file can be found in %s.enc\n", fileName); break;
            }
        }
        
        char response[SIZE];
        memset(response, '\0', SIZE);
        printf("\n[*] Do you want to send another file? (Yes/No): ");
        scanf("%s", response);
        if (strcasecmp(response, "No") == 0) flag = 1;
        send(clientSocket, response, strlen(response), 0);
    } while (flag == 0);

    close(clientSocket);    
    printf("[+] Client socket disconnected from server\n");
    return 0;
}
