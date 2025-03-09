#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "ksocket.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <time.h>

int main(int argc, char *argv[]) 
{
    argcheck(argc, argv);

    int sockfd;
    struct sockaddr_in sADDR;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;
    char src_ip[16], dst_ip[16];

    strcpy(src_ip, argv[1]);
    strcpy(dst_ip, argv[3]);
    uint16_t src_port = atoi(argv[2]);
    uint16_t dst_port = atoi(argv[4]);

    printf("[+] Source IP address      : %s\n",   src_ip);
    printf("[+] Source port number     : %d\n",   src_port);
    printf("[+] Destination IP address : %s\n",   dst_ip);
    printf("[+] Destination port number: %d\n\n", dst_port);

    sADDR.sin_family = AF_INET;
    sADDR.sin_port = htons(dst_port);
    sADDR.sin_addr.s_addr = inet_addr(dst_ip);
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

    int recvlen;
    char filename[100];
    time_t last = time(NULL);
    sprintf(filename, "new_%d.txt", src_port);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    switch (fd) {
        case -1: perror("[-] Failed to open file"); return EXIT_FAILURE;
        default: printf("[+] File opened successfully\n");
    }

    printf("[+] Starting file transfer process...\n");
    while (1) {
        int waiting = 0;
        while ((recvlen = k_recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sADDR, &addr_size)) <= 0) {
            double diff = difftime(time(NULL), last);
            if (waiting == 0) { printf("[!] WAITING"); fflush(stdout); waiting = 1;}
            else {printf(" ."); fflush(stdout);}
            sleep(1);
            if (diff > 1000) {
                perror("[-] Timeout occurred");
                return EXIT_FAILURE;
            }
        }
        if (waiting) printf("\n");
        last = time(NULL);
        
        if (buffer[0] == '$') { printf("[+] End of file received\n"); break; }
        printf("[+] Received Packet [ SIZE: %-3d ] \n", recvlen);
        if (write(fd, buffer + 1, recvlen - 1) < 0) {
            perror("[-] Failed to write to file");
            return EXIT_FAILURE;
        }
    }

    close(fd);
    sleep(60); 
    printf("[+] File received successfully\n");
    sleep(5);

    switch (k_close(sockfd)) {
        case 0: printf("[+] Socket closed successfully\n"); break;
        default: perror("[-] Failed to close socket"); return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
