#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define LCDP_PROTOCOL 253  // Custom protocol number
#define LCDP_VERSION 1     // Version of our protocol

// Message types
#define LCDP_HELLO 1
#define LCDP_QUERY 2
#define LCDP_RESPONSE 3

// Maximum size for our packets
#define MAX_PACKET_SIZE 1024
#define RECV_TIMEOUT 5  // Timeout in seconds

// LCDP header structure
typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t sequence;
} __attribute__((packed)) lcdp_header_t;

// Node structure to store discovered servers
typedef struct node {
    char ip[INET_ADDRSTRLEN];
    char hostname[256];
    time_t last_seen;
    struct node *next;
} node_t;

// Global variables
static volatile int keep_running = 1;
static uint32_t sequence_counter = 0;
static node_t *node_list = NULL;

// Signal handler for graceful termination
void signal_handler(int sig) {
    keep_running = 0;
}

// Initialize and prepare raw socket
int init_raw_socket() {
    int sock_fd;
    int enable = 1;
    struct timeval tv;
    
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
    
    // Set receive timeout
    tv.tv_sec = RECV_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting SO_RCVTIMEO");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

// Add a node to the list or update if it exists
void add_or_update_node(const char *ip, const char *hostname) {
    node_t *current = node_list;
    time_t now = time(NULL);
    
    // Search for existing node
    while (current) {
        if (strcmp(current->ip, ip) == 0) {
            // Update existing node
            strcpy(current->hostname, hostname);
            current->last_seen = now;
            return;
        }
        current = current->next;
    }
    
    // Create new node
    node_t *new_node = (node_t *)malloc(sizeof(node_t));
    if (!new_node) {
        perror("Memory allocation failed");
        return;
    }
    
    strcpy(new_node->ip, ip);
    strcpy(new_node->hostname, hostname);
    new_node->last_seen = now;
    new_node->next = node_list;
    node_list = new_node;
}

// Free the node list
void free_node_list() {
    node_t *current = node_list;
    node_t *next;
    
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
    
    node_list = NULL;
}

// Display the list of discovered nodes
void display_nodes() {
    node_t *current = node_list;
    time_t now = time(NULL);
    int count = 0;
    
    printf("\n--- Discovered Nodes ---\n");
    if (!current) {
        printf("No nodes discovered yet.\n");
        return;
    }
    
    while (current) {
        printf("%d. IP: %s, Hostname: %s, Last seen: %ld seconds ago\n",
               ++count, current->ip, current->hostname, (now - current->last_seen));
        current = current->next;
    }
    printf("------------------------\n");
}

// Send a QUERY packet to a specific node
void send_query_packet(int sock_fd, const char *dest_ip) {
    struct sockaddr_in dest;
    char packet[MAX_PACKET_SIZE];
    struct iphdr *ip_header = (struct iphdr *)packet;
    lcdp_header_t *lcdp_header = (lcdp_header_t *)(packet + sizeof(struct iphdr));
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
    ip_header->tot_len = sizeof(struct iphdr) + sizeof(lcdp_header_t);
    ip_header->id = htons(54321);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = LCDP_PROTOCOL;
    ip_header->check = 0; // Will be calculated by the kernel
    ip_header->saddr = INADDR_ANY; // Source IP will be filled by the kernel
    ip_header->daddr = inet_addr(dest_ip);
    
    // Set LCDP header fields
    lcdp_header->version = LCDP_VERSION;
    lcdp_header->type = LCDP_QUERY;
    lcdp_header->length = htons(sizeof(lcdp_header_t));
    lcdp_header->sequence = htonl(sequence_counter++);
    
    // Calculate total packet size
    packet_size = ip_header->tot_len;
    
    // Send the packet
    if (sendto(sock_fd, packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("Error sending QUERY packet");
    } else {
        printf("QUERY packet sent to %s\n", dest_ip);
    }
}

// Process a HELLO packet
void process_hello(const struct iphdr *ip_header, const lcdp_header_t *lcdp_header, const char *payload) {
    char source_ip[INET_ADDRSTRLEN];
    
    // Extract source IP
    inet_ntop(AF_INET, &(ip_header->saddr), source_ip, INET_ADDRSTRLEN);
    
    // Add the node to our list
    add_or_update_node(source_ip, payload);
    
    printf("Received HELLO from %s (%s)\n", payload, source_ip);
}

// Process a RESPONSE packet
void process_response(const struct iphdr *ip_header, const lcdp_header_t *lcdp_header, const char *payload) {
    char source_ip[INET_ADDRSTRLEN];
    
    // Extract source IP
    inet_ntop(AF_INET, &(ip_header->saddr), source_ip, INET_ADDRSTRLEN);
    
    printf("\n--- Response from %s ---\n", source_ip);
    printf("%s\n", payload);
    printf("------------------------\n");
}

// Wait for and process a RESPONSE packet
int wait_for_response(int sock_fd, uint32_t expected_sequence) {
    char buffer[MAX_PACKET_SIZE];
    ssize_t packet_len;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    time_t start_time = time(NULL);
    
    while ((time(NULL) - start_time) < RECV_TIMEOUT) {
        packet_len = recvfrom(sock_fd, buffer, MAX_PACKET_SIZE, 0, 
                             (struct sockaddr *)&client_addr, &client_len);
        
        if (packet_len < 0) {
            if (errno == EINTR) {
                if (!keep_running) return 0;
                continue; // Interrupted by signal
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout waiting for response\n");
                return 0;
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
                char *payload = buffer + sizeof(struct iphdr) + sizeof(lcdp_header_t);
                
                // Check if it's a RESPONSE and has our sequence number
                if (ntohs(lcdp_header->type) == LCDP_RESPONSE && 
                    ntohl(lcdp_header->sequence) == expected_sequence) {
                    process_response(ip_header, lcdp_header, payload);
                    return 1;
                } else if (ntohs(lcdp_header->type) == LCDP_HELLO) {
                    // Also process any HELLO packets we receive
                    process_hello(ip_header, lcdp_header, payload);
                }
            }
        }
    }
    
    return 0;
}

// Listen for HELLO packets
void listen_for_hello(int sock_fd, int duration) {
    char buffer[MAX_PACKET_SIZE];
    ssize_t packet_len;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    time_t start_time = time(NULL);
    struct timeval tv_original, tv_listen;
    
    // Save original timeout
    socklen_t optlen = sizeof(tv_original);
    getsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_original, &optlen);
    
    // Set shorter timeout for listening
    tv_listen.tv_sec = 1;  // 1 second timeout
    tv_listen.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_listen, sizeof(tv_listen));
    
    printf("Listening for HELLO packets for %d seconds...\n", duration);
    
    while ((time(NULL) - start_time) < duration && keep_running) {
        packet_len = recvfrom(sock_fd, buffer, MAX_PACKET_SIZE, 0, 
                             (struct sockaddr *)&client_addr, &client_len);
        
        if (packet_len < 0) {
            if (errno == EINTR) {
                if (!keep_running) break;
                continue; // Interrupted by signal
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Just a timeout, continue listening
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
                char *payload = buffer + sizeof(struct iphdr) + sizeof(lcdp_header_t);
                
                // Process HELLO packets
                if (ntohs(lcdp_header->type) == LCDP_HELLO) {
                    process_hello(ip_header, lcdp_header, payload);
                }
            }
        }
    }
    
    // Restore original timeout
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_original, sizeof(tv_original));
    
    // Display nodes discovered
    display_nodes();
}

// Display menu and get user choice
int display_menu() {
    int choice;
    
    printf("\nLCDP Client Menu:\n");
    printf("1. Discover nodes (listen for HELLO packets)\n");
    printf("2. Display discovered nodes\n");
    printf("3. Query a node for information\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
    
    if (scanf("%d", &choice) != 1) {
        // Clear input buffer on invalid input
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return 0;
    }
    
    return choice;
}

// Main client function
int main(int argc, char *argv[]) {
    int sock_fd;
    int choice;
    char query_ip[INET_ADDRSTRLEN];
    int node_index;
    
    // Register signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    
    printf("LCDP Client starting...\n");
    
    // Initialize raw socket
    sock_fd = init_raw_socket();
    if (sock_fd < 0) {
        fprintf(stderr, "Failed to initialize raw socket. Did you run with sudo?\n");
        return EXIT_FAILURE;
    }
    
    // Main menu loop
    while (keep_running) {
        choice = display_menu();
        
        switch (choice) {
            case 1:
                // Discover nodes - listen for HELLO packets
                listen_for_hello(sock_fd, 20);  // Listen for 20 seconds
                break;
                
            case 2:
                // Display discovered nodes
                display_nodes();
                break;
                
            case 3: {
                // Query a node
                node_t *current = node_list;
                int count = 0;
                
                if (!current) {
                    printf("No nodes discovered yet. Please discover nodes first.\n");
                    break;
                }
                
                // Display nodes for selection
                printf("\nAvailable nodes:\n");
                while (current) {
                    printf("%d. %s (%s)\n", ++count, current->hostname, current->ip);
                    current = current->next;
                }
                
                printf("Enter node number to query: ");
                if (scanf("%d", &node_index) != 1 || node_index < 1 || node_index > count) {
                    printf("Invalid selection.\n");
                    break;
                }
                
                // Find the selected node
                current = node_list;
                count = 1;
                while (current && count < node_index) {
                    current = current->next;
                    count++;
                }
                
                if (current) {
                    uint32_t seq = sequence_counter;
                    send_query_packet(sock_fd, current->ip);
                    wait_for_response(sock_fd, seq);
                }
                break;
            }
                
            case 4:
                // Exit
                keep_running = 0;
                break;
                
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }
    
    // Clean up
    free_node_list();
    close(sock_fd);
    printf("\nLCDP Client stopped.\n");
    
    return EXIT_SUCCESS;
}