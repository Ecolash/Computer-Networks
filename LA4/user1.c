#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "ksocket.h"

#define BUFFER_SIZE 512
#define INPUT_FILE "test_200KB.txt"

int main(int argc, char *argv[]) 
{
    argcheck(argc, argv);

    int sockfd;
    struct sockaddr_in sADDR;
    char buffer[BUFFER_SIZE];
    char src_ip[16], dst_ip[16];
    
    strcpy(src_ip, argv[1]);
    strcpy(dst_ip, argv[3]);
    uint16_t src_port = atoi(argv[2]);
    uint16_t dst_port = atoi(argv[4]);

    printf("[+] Source IP address      : %s\n",   src_ip);
    printf("[+] Source port number     : %d\n",   src_port);
    printf("[+] Destination IP address : %s\n",   dst_ip);
    printf("[+] Destination port number: %d\n\n", dst_port);

    sADDR.sin_addr.s_addr = inet_addr(dst_ip);
    sADDR.sin_family = AF_INET;
    sADDR.sin_port = htons(dst_port);
    sockfd = k_socket(AF_INET, SOCK_KTP, 0);

    switch (sockfd) {
        case -1: perror("[-] Socket creation failed"); return EXIT_FAILURE;
        default: printf("[+] Socket created successfully\n");
    }

    int binded = k_bind(src_ip, src_port, dst_ip, dst_port);
    switch (binded) {
        case -1: perror("[-] Binding failed"); return EXIT_FAILURE;
        default: printf("[+] Binding successful\n");
    }

    char filename[256];
    /*
    while(1) 
    {
        printf("[!] Enter the filename to send: ");
        scanf("%255s", filename);
        if (access(filename, F_OK) != -1) break;
        else printf("[-] File does not exist. Please enter a valid filename.\n");
    } */
    strcpy(filename, INPUT_FILE);
    int fd = open(filename, O_RDONLY);

    switch (fd) {
        case -1: perror("[-] Failed to open file"); return EXIT_FAILURE;
        default: printf("[+] File opened successfully\n");
    }

    printf("[+] Starting file read and send process\n");
    sleep(5); // Display messages 

    int readlen, seq = 1, packetcnt = 0;
    buffer[0] = '0';

    while ((readlen = read(fd, buffer + 1, BUFFER_SIZE - 1)) > 0) {
        int sendlen;
        while (1) {
            while ((sendlen = k_sendto(sockfd, buffer, readlen + 1, 0, (struct sockaddr*)&sADDR, sizeof(sADDR))) < 0 && errno == ENOBUFS) {
                printf("[*] BUFFER FULL . . . \n");
                sleep(1);
            }
            if (sendlen >= 0) { printf("[+] SENT %d B [SEQ: %-3d]\n", sendlen, seq); break; }
            perror("[-] Failed to send data");
            return EXIT_FAILURE;
        }

        seq = (seq + 1) % 256;
        packetcnt++;
    }

    buffer[0] = '$';
    while (1) {
        int sendlen;
        while ((sendlen = k_sendto(sockfd, buffer, 1, 0, (struct sockaddr*)&sADDR, sizeof(sADDR))) < 0 && errno == ENOBUFS) {
            printf("[*] BUFFER FULL . . .\n");
            sleep(1);
        }

        if (sendlen >= 0) {
            packetcnt++;
            printf("[+] EOF sent successfully. Total messages sent: %d\n", packetcnt);
            break;
        }
        perror("[-] Failed to send EOF");
        return EXIT_FAILURE;
    }

    sleep(60); // Ensure all messages are sent
    printf("[+] File sent successfully\n");
    sleep(5);
    close(fd);

    switch (k_close(sockfd)) {
        case 0:  printf("[+] Socket closed successfully\n"); return EXIT_SUCCESS;
        default: perror("[-] Failed to close socket!"); return EXIT_FAILURE;
    }
}
