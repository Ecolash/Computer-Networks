/* cldp_client.c
 * Assignment 7 Submission
 * Name: <Your_Name>
 * Roll number: <Your_Roll_Number>
 *
 * This program implements the CLDP client.
 * It sends a QUERY message to ask all active nodes for their hostname and timestamp,
 * and then listens for RESPONSE messages from servers.
 *
 * Usage: sudo ./cldp_client <interface_name> <client_ip> <destination_ip>
 *   - <interface_name>: The network interface to bind to (e.g., wlp0s20f3)
 *   - <client_ip>: The IP address of the client on that interface
 *   - <destination_ip>: The target for the QUERY (use broadcast "255.255.255.255" to query all)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define CLDP_PROTOCOL 253
#define HELLO    0x01
#define QUERY    0x02
#define RESPONSE 0x03

#pragma pack(push, 1)
typedef struct {
    uint8_t msg_type;
    uint8_t payload_len;
    uint16_t transaction_id;
    uint32_t reserved;
} cldp_header_t;
#pragma pack(pop)

#define BUF_SIZE 1024

//
// send_query: constructs and sends a QUERY message
//
void send_query(int sockfd, struct sockaddr_in *dest_addr, uint16_t transaction_id, const char *client_ip) {
    char send_buffer[BUF_SIZE];
    memset(send_buffer, 0, BUF_SIZE);

    struct ip *ip_hdr = (struct ip *)send_buffer;
    int ip_hdr_len = sizeof(struct ip);
    int payload_len = 0; // No extra payload for QUERY
    int total_len = ip_hdr_len + sizeof(cldp_header_t) + payload_len;

    ip_hdr->ip_hl = ip_hdr_len / 4;
    ip_hdr->ip_v = 4;
    ip_hdr->ip_tos = 0;
    ip_hdr->ip_len = htons(total_len);
    ip_hdr->ip_id = htons(0);
    ip_hdr->ip_off = 0;
    ip_hdr->ip_ttl = 64;
    ip_hdr->ip_p = CLDP_PROTOCOL;
    ip_hdr->ip_src.s_addr = inet_addr(client_ip);
    ip_hdr->ip_dst = dest_addr->sin_addr;
    ip_hdr->ip_sum = 0;

    cldp_header_t *cldp_hdr = (cldp_header_t *)(send_buffer + ip_hdr_len);
    cldp_hdr->msg_type = QUERY;
    cldp_hdr->payload_len = payload_len;
    cldp_hdr->transaction_id = transaction_id;
    cldp_hdr->reserved = 0;

    if (sendto(sockfd, send_buffer, total_len, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) < 0) {
        perror("sendto QUERY");
    } else {
        printf("Sent QUERY message\n");
    }
}

//
// receive_responses: listens for incoming RESPONSE messages and prints them
//
void receive_responses(int sockfd) {
    char recv_buffer[BUF_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    int n;
    while ((n = recvfrom(sockfd, recv_buffer, BUF_SIZE, 0, (struct sockaddr *)&src_addr, &addr_len)) > 0) {
        struct ip *ip_hdr = (struct ip *)recv_buffer;
        int ip_hdr_len = ip_hdr->ip_hl * 4;
        if (n < ip_hdr_len + sizeof(cldp_header_t))
            continue;
        cldp_header_t *hdr = (cldp_header_t *)(recv_buffer + ip_hdr_len);
        if (hdr->msg_type == RESPONSE) {
            int payload_len = hdr->payload_len;
            char payload[512] = {0};
            memcpy(payload, recv_buffer + ip_hdr_len + sizeof(cldp_header_t), payload_len);
            printf("Received RESPONSE from %s: %s\n", inet_ntoa(ip_hdr->ip_src), payload);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: sudo %s <interface_name> <client_ip> <destination_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *iface = argv[1];
    char *client_ip = argv[2];
    char *dest_ip = argv[3];
    printf("Using interface %s with client IP %s\n", iface, client_ip);

    int sockfd = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    // Bind socket to the specified interface.
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface)) < 0) {
        perror("SO_BINDTODEVICE");
    }
    // Allow broadcast.
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt (SO_BROADCAST)");
        exit(EXIT_FAILURE);
    }
    // Set IP_HDRINCL option.
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt (IP_HDRINCL)");
        exit(EXIT_FAILURE);
    }

    // Set up destination address (e.g., broadcast address "255.255.255.255").
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);

    uint16_t transaction_id = 1;
    // Send a QUERY message.
    send_query(sockfd, &dest_addr, transaction_id, client_ip);

    // Set a timeout for receiving responses.
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
        perror("setsockopt (SO_RCVTIMEO)");
    }

    printf("Waiting for responses...\n");
    receive_responses(sockfd);

    close(sockfd);
    return 0;
}

