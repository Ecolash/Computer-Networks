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

char *format_time()
{
    time_t currentTime;
    time(&currentTime);
    struct tm *localTime = localtime(&currentTime);
    static char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localTime);
    return timeStr;
}

int write_file(int sockfd, struct sockaddr_in addr, const char *filename) 
{
    int n = 0;
    char buffer[SIZE] = {0};
    socklen_t addr_size = sizeof(addr);
    FILE *file = fopen(filename, "w");

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
    fclose(file);
    return n;
}

int encrypt_file(const char *filename, const char *key)
{
    char enc_filename[SIZE];
    snprintf(enc_filename, SIZE, "%s.enc", filename);
    FILE *encrypted_file = fopen(enc_filename, "w");
    FILE *file = fopen(filename, "r");

    char buffer[SIZE] = {0};
    char encrypted[SIZE] = {0};

    char encrypt_map[KEYSIZE], decrypt_map[KEYSIZE];
    for (int i = 0; i < KEYSIZE; i++)
    {
        encrypt_map[i] = key[i];
        decrypt_map[key[i] - 'A'] = 'A' + i;
    }

    while (fgets(buffer, SIZE, file) != NULL)
    {
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            encrypted[i] = buffer[i];
            if (isalpha(buffer[i]))
            {
                int flag = islower(buffer[i]);
                buffer[i] = toupper(buffer[i]);
                encrypted[i] = encrypt_map[buffer[i] - 'A'];
                encrypted[i] = flag ? tolower(encrypted[i]) : encrypted[i];
            }
        }
        fprintf(encrypted_file, "%s", encrypted);
        bzero(buffer, SIZE);
        bzero(encrypted, SIZE);
    }
    fclose(file);
    fclose(encrypted_file);
    return 0;
}

int send_encrypted_file(const char *filename, int sockfd, struct sockaddr_in addr)
{
    char enc_filename[SIZE];
    snprintf(enc_filename, SIZE, "%s.enc", filename);
    char buffer[SIZE] = {0};
    FILE *file = fopen(enc_filename, "r");
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

void handle_client(int newSocket, struct sockaddr_in newAddr, const char* clientname) 
{
    char _ENCRYPTION_KEY[KEYSIZE + 1];
    int receive_key = recv(newSocket, _ENCRYPTION_KEY, KEYSIZE + 1, 0);
    switch(receive_key) {
        case -1: printf("[%s][-] Error in receiving encryption key.\n", clientname); exit(1);
        default: printf("[%s][+] Encryption key received: %s\n", clientname, _ENCRYPTION_KEY); break;
    }

    char filename[SIZE];
    memset(filename, 0, SIZE);
    sprintf(filename, "%s.%d.txt", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));

    int stop = 0;
    do {
        char file[SIZE];
        int get_file = recv(newSocket, file, SIZE, 0);
        file[get_file] = '\0';

        if (strcmp(file, "NOT_FOUND") == 0) printf("[%s][-] FILE NOT FOUND!\n", clientname);
        else {
            int status = write_file(newSocket, newAddr, filename);
            switch(status) {
                case -1: printf("[%s][-] Error in writing file.\n", clientname); exit(1);
                default: printf("[%s][+] %d packets transferred successfully.\n", clientname, status); break;
            }

            int encrypt = encrypt_file(filename, _ENCRYPTION_KEY);
            switch(encrypt) {
                case -1: printf("[%s][-] Error in encrypting file.\n", clientname); exit(1);
                default: printf("[%s][+] File encrypted successfully.\n", clientname); break;
            }

            int send_status = send_encrypted_file(filename, newSocket, newAddr);
            switch(send_status) {
                case -1: printf("[%s][-] Error in sending encrypted file.\n", clientname); exit(1);
                default: printf("[%s][+] Encrypted file sent successfully.\n", clientname); break;
            }
        }

        char response[SIZE];
        memset(response, '\0', SIZE);
        int recv_response = recv(newSocket, response, SIZE, 0);
        if (recv_response == -1) { printf("[%s][-] Error in receiving response.\n", clientname); break; }
        printf("[%s][+] Response received: %s\n", clientname, response);
        response[recv_response] = '\0';
        if (strcasecmp(response, "No") == 0) stop = 1;

    } while (stop == 0);

    close(newSocket);
    printf("[%s][+] Client disconnected.\n", clientname);
    exit(0);
}

int main() {
    int sockfd, newSocket;
    struct sockaddr_in serverAddr;

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

    while (1) {
        struct sockaddr_in newAddr;
        socklen_t addr_size = sizeof(newAddr);
        newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);

        switch (newSocket) {
            case -1: printf("[-] Error in accepting.\n"); exit(1);
            default: printf("[+] Connection accepted from %s:%d.\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port)); break;
        }

        if (fork() == 0) {
            close(sockfd);
            char clientname[SIZE];
            sprintf(clientname, "%s : %5d", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));
            handle_client(newSocket, newAddr ,clientname);
        } 
        else close(newSocket);
    }
    close(sockfd);
    return 0;
}
