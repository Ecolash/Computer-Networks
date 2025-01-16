#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define MAX_DATA 4

void hamming_encode(int data[MAX_DATA], int encoded[7]) {
    encoded[2] = data[0];
    encoded[4] = data[1];
    encoded[5] = data[2];
    encoded[6] = data[3];

    encoded[0] = encoded[2] ^ encoded[4] ^ encoded[6];
    encoded[1] = encoded[2] ^ encoded[5] ^ encoded[6];
    encoded[3] = encoded[4] ^ encoded[5] ^ encoded[6];
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


void flip_bit(int encoded[7]) {
    srand(time(0));
    int pos = rand() % 7;
    printf("[*] Flipping bit at position %d\n", pos + 1);
    encoded[pos] ^= 1;
}


int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    memset(&serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int data[MAX_DATA];
    int connected = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    switch (connected)
    {
        case -1: printf("[-] Error in connection.\n"); exit(1);
        default: printf("[+] Connected to Server.\n"); break;
    }

    printf("Enter 4 bits of data (space-separated, 0 or 1): ");
    for (int i = 0; i < MAX_DATA; i++) {
        scanf("%d", &data[i]);
        if (data[i] != 0 && data[i] != 1) { printf("Invalid input. Please enter only 0 or 1.\n"); i--; }
    }

    int encoded[7];
    printf("[*] Original data: ");
    for (int i = 0; i < MAX_DATA; i++) printf("%d ", data[i]);
    printf("\n");

    hamming_encode(data, encoded);
    printf("[*] Encoded data: ");
    for (int i = 0; i < 7; i++) printf("%d ", encoded[i]);
    printf("\n");

    flip_bit(encoded);
    printf("[*] Data after bit flip: ");
    for (int i = 0; i < 7; i++) printf("%d ", encoded[i]);
    printf("\n");

    send(sock, encoded, sizeof(encoded), 0);
    char* timeStr = format_time();
    printf("[%s] Data sent to server.\n", timeStr);
    close(sock);    
    return 0;
}
