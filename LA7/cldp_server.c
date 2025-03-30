/* cldp_server.c
 * Assignment 7 Submission
 * Name: <Your_Name>
 * Roll number: <Your_Roll_Number>
 *
 * This program implements the Custom Lightweight Discovery Protocol (CLDP) server.
 * It performs two functions:
 *  1. Every 10 seconds, it broadcasts a HELLO message to announce its presence.
 *  2. It listens for QUERY messages and responds with a RESPONSE containing its
 *     hostname and current timestamp.
 *
 * Usage: sudo ./cldp_server <interface_name> <server_ip>
 *   - <interface_name>: The network interface to bind to (e.g., wlp0s20f3)
 *   - <server_ip>: The IP address of that interface
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
#include <pthread.h>

#define CLDP_PROTOCOL 253
#define HELLO    0x01
#define QUERY    0x02
#define RESPONSE 0x03

#pragma pack(push, 1)
typedef struct {
    uint8_t msg_type;       // Message type (HELLO, QUERY, RESPONSE)
    uint8_t payload_len;    // Length of payload (if any)
    uint16_t transaction_id;// Transaction ID for matching queries/responses
    uint32_t reserved;      // Reserved field (set to 0)
} cldp_header_t;
#pragma pack(pop)

#define BUF_SIZE 1024

// Global variables (set from command-line arguments)
int sockfd;
char iface[32];
char server_ip[32];

//
// send_hello_thread: broadcasts HELLO messages every 10 seconds
//
void *send_hello_thread(void *arg) {
    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    // Use the broadcast address
    bcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    // Enable broadcast on the socket.
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt (SO_BROADCAST)");
        pthread_exit(NULL);
    }

    uint16_t transaction_id = 0;
    while (1) {
        char send_buffer[BUF_SIZE];
        memset(send_buffer, 0, BUF_SIZE);

        // Build IP header manually.
        struct ip *ip_hdr = (struct ip *)send_buffer;
        int ip_hdr_len = sizeof(struct ip);
        ip_hdr->ip_hl = ip_hdr_len / 4;
        ip_hdr->ip_v = 4;
        ip_hdr->ip_tos = 0;
        // No payload for HELLO: total length = IP header + CLDP header.
        int payload_len = 0;
        int total_len = ip_hdr_len + sizeof(cldp_header_t) + payload_len;
        ip_hdr->ip_len = htons(total_len);
        ip_hdr->ip_id = htons(0);
        ip_hdr->ip_off = 0;
        ip_hdr->ip_ttl = 64;
        ip_hdr->ip_p = CLDP_PROTOCOL;
        ip_hdr->ip_src.s_addr = inet_addr(server_ip);
        ip_hdr->ip_dst = bcast_addr.sin_addr;
        ip_hdr->ip_sum = 0; // Kernel may compute checksum

        // Build CLDP header.
        cldp_header_t *cldp_hdr = (cldp_header_t *)(send_buffer + ip_hdr_len);
        cldp_hdr->msg_type = HELLO;
        cldp_hdr->payload_len = payload_len;
        cldp_hdr->transaction_id = transaction_id++;
        cldp_hdr->reserved = 0;

        if (sendto(sockfd, send_buffer, total_len, 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr)) < 0) {
            perror("sendto HELLO");
        } else {
            printf("Broadcasted HELLO message\n");
        }

        sleep(10);
    }
    return NULL;
}

//
// process_query: if a QUERY is received, respond with a RESPONSE containing hostname and timestamp.
//
void process_query(char *buffer, int size) {
    struct ip *ip_hdr = (struct ip *)buffer;
    int ip_hdr_len = ip_hdr->ip_hl * 4;
    if (size < ip_hdr_len + sizeof(cldp_header_t)) {
        fprintf(stderr, "Received packet too small\n");
        return;
    }

    cldp_header_t *query_hdr = (cldp_header_t *)(buffer + ip_hdr_len);
    if (query_hdr->msg_type != QUERY)
        return;  // Only process QUERY messages here

    printf("Received QUERY from %s\n", inet_ntoa(ip_hdr->ip_src));

    // Build RESPONSE packet.
    char send_buffer[BUF_SIZE];
    memset(send_buffer, 0, BUF_SIZE);

    struct ip *ip_send = (struct ip *)send_buffer;
    int ip_send_len = sizeof(struct ip);
    // Get metadata: hostname and timestamp.
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%ld.%06ld", tv.tv_sec, tv.tv_usec);
    char payload[512];
    snprintf(payload, sizeof(payload), "hostname:%s,time:%s", hostname, time_str);
    uint8_t payload_len = (uint8_t)strlen(payload);

    int total_len = ip_send_len + sizeof(cldp_header_t) + payload_len;
    ip_send->ip_hl = ip_send_len / 4;
    ip_send->ip_v = 4;
    ip_send->ip_tos = 0;
    ip_send->ip_len = htons(total_len);
    ip_send->ip_id = htons(0);
    ip_send->ip_off = 0;
    ip_send->ip_ttl = 64;
    ip_send->ip_p = CLDP_PROTOCOL;
    ip_send->ip_src.s_addr = inet_addr(server_ip);
    ip_send->ip_dst = ip_hdr->ip_src;  // Reply to sender
    ip_send->ip_sum = 0;

    cldp_header_t *resp_hdr = (cldp_header_t *)(send_buffer + ip_send_len);
    resp_hdr->msg_type = RESPONSE;
    resp_hdr->payload_len = payload_len;
    resp_hdr->transaction_id = query_hdr->transaction_id;
    resp_hdr->reserved = 0;

    memcpy(send_buffer + ip_send_len + sizeof(cldp_header_t), payload, payload_len);

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = ip_hdr->ip_src;

    if (sendto(sockfd, send_buffer, total_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("sendto RESPONSE");
    } else {
        printf("Sent RESPONSE to %s\n", inet_ntoa(dest_addr.sin_addr));
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: sudo %s <interface_name> <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    strncpy(iface, argv[1], sizeof(iface) - 1);
    strncpy(server_ip, argv[2], sizeof(server_ip) - 1);
    printf("Using interface %s with server IP %s\n", iface, server_ip);

    sockfd = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    // Bind the socket to the given interface.
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface)) < 0) {
        perror("SO_BINDTODEVICE");
    }
    // Tell the kernel that we are including our own IP header.
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt (IP_HDRINCL)");
        exit(EXIT_FAILURE);
    }

    // Start HELLO broadcast thread.
    pthread_t hello_tid;
    if (pthread_create(&hello_tid, NULL, send_hello_thread, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    printf("CLDP Server is running and awaiting QUERY messages...\n");

    // Main loop: receive incoming packets and process QUERY messages.
    char recv_buffer[BUF_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    while (1) {
        int recv_len = recvfrom(sockfd, recv_buffer, BUF_SIZE, 0, (struct sockaddr *)&src_addr, &addr_len);
        if (recv_len < 0) {
            perror("recvfrom");
            continue;
        }
        // Process the packet if it is a QUERY.
        process_query(recv_buffer, recv_len);
    }

    close(sockfd);
    return 0;
}

