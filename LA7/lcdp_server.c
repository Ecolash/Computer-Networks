#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#define LCDP_PROTOCOL 253  // Custom protocol number
#define LCDP_VERSION 1     // Version of our protocol

// Message types
#define LCDP_HELLO 1
#define LCDP_QUERY 2
#define LCDP_RESPONSE 3

// Maximum size for our packets
#define MAX_PACKET_SIZE 1024

// LCDP header structure
typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t sequence;
} __attribute__((packed)) lcdp_header_t;

// Global variables
static volatile int keep_running = 1;
static uint32_t sequence_counter = 0;
static char local_hostname[256];

// Signal handler for graceful termination
void signal_handler(int sig) {
    keep_running = 0;
}

// Function to get local IP address
int get_local_ip(char *interface_name, char *ip) {
    int fd;
    struct ifreq ifr;
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("Cannot create socket");
        return -1;
    }
    
    // Set interface name
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ-1);
    
    // Get IP address
    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        perror("Error getting IP address");
        close(fd);
        return -1;
    }
    
    strcpy(ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    
    close(fd);
    return 0;
}

// Initialize and prepare raw socket
int init_raw_socket() {
    int sock_fd;
    int enable = 1;
    
    // Create raw socket with our custom protocol
    sock_fd = socket(AF_INET, SOCK_RAW, LCDP_PROTOCOL);
    if (sock_fd < 0) {
        perror("Error creating raw socket");
        return -1;
    }
    
    // Enable IP header include
    if (setsockopt(sock_fd, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) < 0) {
        perror("Error setting IP_HDRINCL");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

// Craft and send a HELLO packet
void send_hello_packet(int sock_fd, const char *dest_ip) {
    struct sockaddr_in dest;
    char packet[MAX_PACKET_SIZE];
    struct iphdr *ip_header = (struct iphdr *)packet;
    lcdp_header_t *lcdp_header = (lcdp_header_t *)(packet + sizeof(struct iphdr));
    char *payload = packet + sizeof(struct iphdr) + sizeof(lcdp_header_t);
    int packet_size;
    
    // Prepare destination address
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(dest_ip);
    
    // Clear packet buffer
    memset(packet, 0, MAX_PACKET_SIZE);
    
    // Set IP header fields
    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = sizeof(struct iphdr) + sizeof(lcdp_header_t) + strlen(local_hostname) + 1;
    ip_header->id = htons(54321);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = LCDP_PROTOCOL;
    ip_header->check = 0; // Will be calculated by the kernel
    ip_header->saddr = INADDR_ANY; // Source IP will be filled by the kernel
    ip_header->daddr = inet_addr(dest_ip);
    
    // Set LCDP header fields
    lcdp_header->version = LCDP_VERSION;
    lcdp_header->type = LCDP_HELLO;
    lcdp_header->length = htons(sizeof(lcdp_header_t) + strlen(local_hostname) + 1);
    lcdp_header->sequence = htonl(sequence_counter++);
    
    // Set payload (hostname in this case)
    strcpy(payload, local_hostname);
    
    // Calculate total packet size
    packet_size = ip_header->tot_len;
    
    // Send the packet
    if (sendto(sock_fd, packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("Error sending HELLO packet");
    } else {
        printf("HELLO packet sent to %s\n", dest_ip);
    }
}

// Process query and send response
void process_query(int sock_fd, const struct iphdr *recv_ip_header, const lcdp_header_t *recv_lcdp_header) {
    struct sockaddr_in dest;
    char packet[MAX_PACKET_SIZE];
    struct iphdr *ip_header = (struct iphdr *)packet;
    lcdp_header_t *lcdp_header = (lcdp_header_t *)(packet + sizeof(struct iphdr));
    char *payload = packet + sizeof(struct iphdr) + sizeof(lcdp_header_t);
    int packet_size;
    char source_ip[INET_ADDRSTRLEN];
    time_t current_time;
    char time_str[100];
    double load_avg[3];
    char response_data[512];
    
    // Get source IP to respond to
    inet_ntop(AF_INET, &(recv_ip_header->saddr), source_ip, INET_ADDRSTRLEN);
    
    // Prepare destination address
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = recv_ip_header->saddr;
    
    // Get system information for response
    time(&current_time);
    strcpy(time_str, ctime(&current_time));
    time_str[strlen(time_str)-1] = '\0'; // Remove newline
    
    // Get load average as additional metric
    getloadavg(load_avg, 3);
    
    // Format response data
    snprintf(response_data, sizeof(response_data), 
             "Hostname: %s\nTimestamp: %s\nLoad Average (1,5,15 min): %.2f, %.2f, %.2f",
             local_hostname, time_str, load_avg[0], load_avg[1], load_avg[2]);
    
    // Clear packet buffer
    memset(packet, 0, MAX_PACKET_SIZE);
    
    // Set IP header fields
    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = sizeof(struct iphdr) + sizeof(lcdp_header_t) + strlen(response_data) + 1;
    ip_header->id = htons(54321);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = LCDP_PROTOCOL;
    ip_header->check = 0; // Will be calculated by the kernel
    ip_header->saddr = INADDR_ANY; // Source IP will be filled by the kernel
    ip_header->daddr = recv_ip_header->saddr;
    
    // Set LCDP header fields
    lcdp_header->version = LCDP_VERSION;
    lcdp_header->type = LCDP_RESPONSE;
    lcdp_header->length = htons(sizeof(lcdp_header_t) + strlen(response_data) + 1);
    lcdp_header->sequence = recv_lcdp_header->sequence; // Use the same sequence number as the query
    
    // Set payload with the response data
    strcpy(payload, response_data);
    
    // Calculate total packet size
    packet_size = ip_header->tot_len;
    
    // Send the response packet
    if (sendto(sock_fd, packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("Error sending RESPONSE packet");
    } else {
        printf("RESPONSE packet sent to %s\n", source_ip);
    }
}

// Thread function for periodic HELLO announcements
void *hello_thread(void *arg) {
    int sock_fd = *(int *)arg;
    
    while (keep_running) {
        // Send HELLO to broadcast address
        send_hello_packet(sock_fd, "255.255.255.255");
        
        // Sleep for 10 seconds as specified
        sleep(10);
    }
    
    return NULL;
}

// Main server function
int main(int argc, char *argv[]) {
    int sock_fd;
    char buffer[MAX_PACKET_SIZE];
    ssize_t packet_len;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char local_ip[INET_ADDRSTRLEN];
    pthread_t hello_tid;
    
    // Register signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    
    // Get hostname
    if (gethostname(local_hostname, sizeof(local_hostname)) != 0) {
        perror("Error getting hostname");
        strcpy(local_hostname, "unknown");
    }
    
    // Get local IP - using eth0 by default, change if needed
    if (get_local_ip("eth0", local_ip) < 0) {
        // Try alternative interface
        if (get_local_ip("wlan0", local_ip) < 0) {
            strcpy(local_ip, "127.0.0.1");
            printf("Using fallback IP: %s\n", local_ip);
        }
    }
    
    printf("LCDP Server starting on %s (%s)\n", local_hostname, local_ip);
    
    // Initialize raw socket
    sock_fd = init_raw_socket();
    if (sock_fd < 0) {
        fprintf(stderr, "Failed to initialize raw socket. Did you run with sudo?\n");
        return EXIT_FAILURE;
    }
    
    // Create thread for periodic HELLO messages
    if (pthread_create(&hello_tid, NULL, hello_thread, &sock_fd) != 0) {
        perror("Failed to create HELLO thread");
        close(sock_fd);
        return EXIT_FAILURE;
    }
    
    printf("Server is running. Press Ctrl+C to exit.\n");
    
    // Main loop - receive and process packets
    while (keep_running) {
        packet_len = recvfrom(sock_fd, buffer, MAX_PACKET_SIZE, 0, 
                             (struct sockaddr *)&client_addr, &client_len);
        
        if (packet_len < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal
            }
            perror("Error receiving packet");
            continue;
        }
        
        // Process only if it's a valid packet
        if (packet_len >= (sizeof(struct iphdr) + sizeof(lcdp_header_t))) {
            struct iphdr *ip_header = (struct iphdr *)buffer;
            
            // Check if it's our protocol
            if (ip_header->protocol == LCDP_PROTOCOL) {
                lcdp_header_t *lcdp_header = (lcdp_header_t *)(buffer + sizeof(struct iphdr));
                
                // Process based on message type
                switch (ntohs(lcdp_header->type)) {
                    case LCDP_QUERY:
                        printf("Received QUERY\n");
                        process_query(sock_fd, ip_header, lcdp_header);
                        break;
                    
                    case LCDP_HELLO:
                        // Just log HELLO messages from other nodes
                        char source_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(ip_header->saddr), source_ip, INET_ADDRSTRLEN);
                        printf("Received HELLO from %s\n", source_ip);
                        break;
                    
                    default:
                        // Ignore other message types
                        break;
                }
            }
        }
    }
    
    // Join the HELLO thread
    pthread_join(hello_tid, NULL);
    
    // Clean up
    close(sock_fd);
    printf("\nLCDP Server stopped.\n");
    
    return EXIT_SUCCESS;
}