#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define MAX_DATA 4

char *format_time()
{
    time_t currentTime;
    time(&currentTime);
    struct tm *localTime = localtime(&currentTime);
    static char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localTime);
    return timeStr;
}

void hamming_decode(int rec[7], int data[MAX_DATA]) {
    int p1 = rec[0] ^ rec[2] ^ rec[4] ^ rec[6];
    int p2 = rec[1] ^ rec[2] ^ rec[5] ^ rec[6];
    int p4 = rec[3] ^ rec[4] ^ rec[5] ^ rec[6];
    int err = p1 + (p2 << 1) + (p4 << 2);

    if (err) {
        char *timeStr = format_time();
        printf("[%s] Error detected at position %d. Correcting...\n", timeStr, err);
        rec[err - 1] ^= 1;
    }

    data[0] = rec[2];
    data[1] = rec[4];
    data[2] = rec[5];
    data[3] = rec[6];
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    switch (server_fd) {
        case -1: printf("[-] Error in socket.\n"); exit(1);
        default: printf("[+] Server socket created.\n"); break;
    }

    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    int binded = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    switch (binded) {
        case -1: printf("[-] Error in binding.\n"); exit(1);
        default: printf("[+] Binded to port %d.\n", PORT); break;
    }

    int listening = listen(server_fd, 3);
    switch (listening) {
        case -1: printf("[-] Error in listening.\n"); exit(1);
        default: printf("[+] Listening...\n"); break;
    }

    char *timeStr = format_time();
    printf("[%s] Server listening on port %d...\n", timeStr, PORT);

    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    switch (new_socket) {
        case -1: printf("[-] Error in accepting.\n"); exit(1);
        default: printf("[+] Connection accepted from %s:%d.\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port)); break;
    }

    int rec[7];
    read(new_socket, rec, sizeof(rec));

    timeStr = format_time();
    printf("[%s] Recieved encoded data: ", timeStr);
    for (int i = 0; i < 7; i++) printf("%d ", rec[i]);
    printf("\n");

    int data[MAX_DATA];
    hamming_decode(rec, data);

    timeStr = format_time();
    printf("[%s] Decoded data: ", timeStr);
    for (int i = 0; i < MAX_DATA; i++) printf("%d ", data[i]);
    printf("\n");

    close(new_socket);
    close(server_fd);    
    return 0;
}
