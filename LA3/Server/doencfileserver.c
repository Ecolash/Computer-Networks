#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define KEYSIZE              26
#define PORT                 5050
#define MAX_CHUNKSIZE        100

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
    char buffer[MAX_CHUNKSIZE];
    memset(buffer, '\0', MAX_CHUNKSIZE);

    socklen_t addr_size = sizeof(addr);
    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file < 0) {
        printf("[-] Error in opening file.\n");
        return -1;
    }

    while(1) {
        int received = recv(sockfd, buffer, MAX_CHUNKSIZE, 0); n++;
        switch(received) {
            case -1: printf("[-] Error in receiving data.\n"); close(file); return -1;
            default: break;
        }
        if (buffer[received - 1] == '$')
        {
            write(file, buffer, received - 1);
            break;
        }
        write(file, buffer, received);
        memset(buffer, '\0', MAX_CHUNKSIZE);
    }
    close(file);
    return n;
}

int encrypt_file(const char *filename, const char *key)
{
    char enc_filename[MAX_CHUNKSIZE];
    snprintf(enc_filename, MAX_CHUNKSIZE, "%s.enc", filename);
    FILE *encrypted_file = fopen(enc_filename, "w");
    FILE *file = fopen(filename, "r");

    char buffer[MAX_CHUNKSIZE] = {0};
    char encrypted[MAX_CHUNKSIZE] = {0};

    char encrypt_map[KEYSIZE], decrypt_map[KEYSIZE];
    for (int i = 0; i < KEYSIZE; i++)
    {
        encrypt_map[i] = key[i];
        decrypt_map[key[i] - 'A'] = 'A' + i;
    }

    while (fgets(buffer, MAX_CHUNKSIZE, file) != NULL)
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
        bzero(buffer, MAX_CHUNKSIZE);
        bzero(encrypted, MAX_CHUNKSIZE);
    }
    fclose(file);
    fclose(encrypted_file);
    return 0;
}

int send_encrypted_file(const char *filename, int sockfd, struct sockaddr_in addr)
{
    char enc_filename[MAX_CHUNKSIZE];
    snprintf(enc_filename, MAX_CHUNKSIZE, "%s.enc", filename);
    int file = open(enc_filename, O_RDONLY);
    if (file < 0) {
        printf("[-] Error in opening encrypted file.\n");
        return -1;
    }

    char buf[MAX_CHUNKSIZE];
    memset(buf, '\0', sizeof(buf));
    int read_bytes;
    while ((read_bytes = read(file, buf, MAX_CHUNKSIZE)) > 0) 
    {
        int sent = send(sockfd, buf, read_bytes, 0);
        switch(sent) {
            case -1: printf("[-] Error in sending data.\n"); close(file); return -1;
            default: break;
        }
        memset(buf, '\0', sizeof(buf));
    }

    strcpy(buf, "$");
    int sent = send(sockfd, buf, strlen(buf), 0);
    switch(sent) {
        case -1: printf("[-] Error in sending data.\n"); close(file); return -1;
        default: break;
    }
    close(file);
    return 0;
}

void handle_client(int newSocket, struct sockaddr_in newAddr, const char* clientname) 
{
    char filename[MAX_CHUNKSIZE];
    memset(filename, 0, MAX_CHUNKSIZE);
    sprintf(filename, "%s.%d.txt", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));

    int stop = 0;
    do {
        char file[MAX_CHUNKSIZE];
        char _ENCRYPTION_KEY[KEYSIZE + 1];
        int receive_key = recv(newSocket, _ENCRYPTION_KEY, KEYSIZE + 1, 0);
        switch(receive_key) {
            case -1: printf("[%s][-] Error in receiving encryption key.\n", clientname); exit(1);
            default: break;
        }

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
            default: printf("[%s][+] Encrypted file sent successfully.\n\n", clientname); break;
        }
    } while (stop == 0);
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
            char clientname[MAX_CHUNKSIZE];
            sprintf(clientname, "%s : %5d", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));
            handle_client(newSocket, newAddr ,clientname);
        } 
        else close(newSocket);
    }
    close(sockfd);
    return 0;
}