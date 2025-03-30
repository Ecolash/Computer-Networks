/*
    Simple Raw Socket Server in C
    Listens for incoming ICMP packets
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

int main() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    unsigned char buffer[BUFFER_SIZE];
    printf("Server listening for ICMP packets...\n");

    while (1) {
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        ssize_t packet_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&src_addr, &addr_len);

        if (packet_len < 0) {
            perror("Packet reception failed");
            continue;
        }

        printf("Received packet from: %s\n", inet_ntoa(src_addr.sin_addr));
        printf("ICMP Data (First 20 bytes): ");
        for (int i = 0; i < 20 && i < packet_len; i++)  printf("%02x ", buffer[i]);
        printf("\n");
    }

    close(sockfd);
    return 0;
}
