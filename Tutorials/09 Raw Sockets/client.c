/*
    Simple Raw Socket Client in C
    Sends an ICMP Echo Request to a specified IP
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>
#include <errno.h>

// ICMP Header Structure
struct icmp_header {
    uint8_t type;         // ICMP type (8 for Echo Request)
    uint8_t code;         // Code (0 for Echo Request)
    uint16_t checksum;    // Checksum for error checking
    uint16_t id;          // ID for request
    uint16_t sequence;    // Sequence number
};

// Checksum Calculation
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;

    for (; len > 1; len -= 2) {
        sum += *buf++;
    }

    if (len == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (unsigned short)(~sum);
}

int main() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = 0;
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");

    // ICMP packet setup
    struct icmp_header icmp;
    memset(&icmp, 0, sizeof(icmp));
    icmp.type = 8;        // ICMP Echo Request
    icmp.code = 0;
    icmp.id = getpid();
    icmp.sequence = 1;
    icmp.checksum = checksum(&icmp, sizeof(icmp));

    if (sendto(sockfd, &icmp, sizeof(icmp), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("Packet send failed");
        close(sockfd);
        return 1;
    }

    printf("ICMP packet sent to %s\n", inet_ntoa(dest.sin_addr));

    close(sockfd);
    return 0;
}
