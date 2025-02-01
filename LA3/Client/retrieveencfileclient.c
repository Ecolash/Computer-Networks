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

int check_key(const char *key)
{
    int len = strlen(key);
    if (len != KEYSIZE) printf("[-] Key must be 26 characters long.\n");
    else return 1;
    return 0;
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
    int file = open(filename, O_RDONLY);
    if (file < 0) {
        printf("[-] Error in opening file.\n");
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
    int sent = send(sockfd, buf, 1, 0);
    switch(sent) {
        case -1: printf("[-] Error in sending data.\n"); close(file); return -1;
        default: break;
    }
    printf("[%s] File sent successfully.\n", format_time());
    close(file);
    return 0;
}

int write_encrypted_file(int sockfd, struct sockaddr_in addr, const char *filename) 
{
    int n = 0;
    char enc_filename[MAX_CHUNKSIZE];
    snprintf(enc_filename, MAX_CHUNKSIZE, "%s.enc", filename);
    char buffer[MAX_CHUNKSIZE];
    memset(buffer, '\0', MAX_CHUNKSIZE);

    socklen_t addr_size = sizeof(addr);
    int file = open(enc_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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
    printf("[%s] Received %d packets from server.\n", format_time(), n);
    printf("[%s] Encrypted File received successfully.\n", format_time());
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

    char fileName[MAX_CHUNKSIZE];
    char ENCRYPTION_KEY[KEYSIZE + 1];
    int check = 0;

    int flag = 0;
    do {
        printf("[*] Enter file name: ");
        scanf("%s", fileName);
        FILE *file = fopen(fileName, "r");

        char file_response[MAX_CHUNKSIZE];
        memset(file_response, 0, MAX_CHUNKSIZE);
        if (file == NULL) 
        {
            printf("[-] File: %s not found. Enter valid filename!\n", fileName);
            continue;
        }
       
        fclose(file);
        do {
            printf("[*] Enter encryption key: ");
            scanf("%s", ENCRYPTION_KEY);
            ENCRYPTION_KEY[KEYSIZE] = '\0';
            check = check_key(ENCRYPTION_KEY);
        } while (!check);

        int send_status2 = send(clientSocket, ENCRYPTION_KEY, KEYSIZE + 1, 0);
        switch (send_status2) {
            case -1: printf("\n[-] Error in sending encryption key.\n"); exit(1);
            default: printf("\n[%s] Encryption key sent successfully.\n", format_time()); break;
        }

	clock_t t1 = clock();
        int send_status = send_file(fileName, clientSocket, serverAddr);
        switch (send_status) {
            case -1: printf("[-] Error in sending file name.\n"); exit(1);
            default: break;
        }

        int write_status = write_encrypted_file(clientSocket, serverAddr, fileName);
        clock_t t2 = clock();
        switch (write_status) {
            case -1: printf("[-] Error in writing encrypted file.\n"); exit(1);
            default: printf("\n[+] Received file can be found in %s.enc\n", fileName); break;
        }
        
        double t = (double)(t2 - t1) * 1000.0 / (CLOCKS_PER_SEC);
        printf("[+] Total time taken to encrypt file: %f ms\n", t);

        char response[MAX_CHUNKSIZE];
        memset(response, '\0', MAX_CHUNKSIZE);
        printf("\n[*] Do you want to send another file? (Yes/No): ");
        scanf("%s", response);
        if (strcasecmp(response, "No") == 0) flag = 1;
        // send(clientSocket, response, strlen(response), 0);
        
    } while (flag == 0);


    printf("[+] Client socket disconnected from server\n");
    return 0;
}
