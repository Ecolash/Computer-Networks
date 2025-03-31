#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/ip.h>

#define CLDP_HELLO            0x01
#define CLDP_QUERY            0x02
#define CLDP_RESPONSE         0x03      
#define ALL_METADATA          0xFF

#define BOLD "\033[1m"
#define RESET "\033[0m"

#define CLDP_PROTOCOL_NO      253
#define MAX_BUFFER_SIZE       2048  
#define MAX_QUERY             100
#define MAX_METADATA_SIZE     1024
#define MAX_QUERY_SIZE        100

#pragma pack(push,1)
struct cldp_header {
    uint8_t packet_type;
    uint8_t packet_length;
    uint16_t transaction_id;
    uint32_t reserved;
};
#pragma pack(pop)

int curr_transcn;
int TRANSACTIONS[MAX_QUERY];

void terminate()
{
    for(int i = 0; i < MAX_QUERY; i++) 
    {
        if (TRANSACTIONS[i] != -1) 
        {
            int transcn_id = TRANSACTIONS[i];
            printf("[-] Client terminated without processing QUERY ");
            printf("[Transaction ID : %-5d ]\n", transcn_id);
        }
    }
    printf("\n[!] Terminating CLDP client...\n");
    exit(EXIT_SUCCESS);
}

unsigned short checksum(unsigned short *addr, int len) {
    unsigned int sum = 0;
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }

    if (len == 1)  sum += *(unsigned char *)addr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum);
}

int find_active_transaction(uint16_t trans_id)
{
    for (int i = 0; i < MAX_QUERY; i++) {
        if (TRANSACTIONS[i] == trans_id) {
            TRANSACTIONS[i] = -1;
            return 1;
        }
    }
    return 0;
}

void send_query(int sockfd, struct sockaddr_in *server_addr){
    char packet[MAX_BUFFER_SIZE];
    bzero(packet, sizeof(packet));

    struct iphdr *IP_HEADER = (struct iphdr *)packet;

    IP_HEADER->version = 4;
    IP_HEADER->ihl = 5;
    IP_HEADER->tos = 0;
    IP_HEADER->tot_len = htons(sizeof(struct iphdr) + sizeof(struct cldp_header));
    IP_HEADER->id = htons(54321);

    IP_HEADER->ttl = 64;
    IP_HEADER->frag_off = 0;
    IP_HEADER->protocol = CLDP_PROTOCOL_NO;
    IP_HEADER->saddr = INADDR_ANY;
    IP_HEADER->daddr = server_addr->sin_addr.s_addr;
    IP_HEADER->check = checksum((unsigned short *)IP_HEADER, sizeof(struct iphdr));

    struct cldp_header *CLDP_HEADER = (struct cldp_header *)(packet + sizeof(struct iphdr));
    CLDP_HEADER->packet_type = CLDP_QUERY;
    CLDP_HEADER->packet_length = 0;
    CLDP_HEADER->transaction_id = curr_transcn++;
    CLDP_HEADER->reserved = ALL_METADATA;
    
    int i;
    for (i = 0; i < MAX_QUERY && TRANSACTIONS[i] != -1; i++);
    if (i >= MAX_QUERY) 
    {
        printf("[-] Maximum query limit is reached. Wait for responses...\n");
        printf("[-] Failed to handle transaction %d\n", CLDP_HEADER->transaction_id);
        return;
    }

    TRANSACTIONS[i] = CLDP_HEADER->transaction_id;
    int sent = sendto(sockfd, packet, sizeof(struct iphdr) + sizeof(struct cldp_header), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    switch(sent) {
        case -1:
            perror("sendto failed");
            exit(EXIT_FAILURE);

        default:
            printf("[+] Sent QUERY to server %s ", inet_ntoa(server_addr->sin_addr));
            printf("[Size = %-3d | Transaction ID = %-5d]\n", sent, CLDP_HEADER->transaction_id);
            return;
    }
}

void JSON_stringify(const char *JSON) 
{
    const char *p = JSON;
    printf(BOLD "\nMetadata:\n\n" RESET);
    while (*p) {
        if (*p == '"') {
            p++; 
            const char *key_start = p;
            while (*p && *p != '"') p++;

            int key_len = p - key_start;
            char key[128] = {0};
            strncpy(key, key_start, key_len);
            key[key_len] = '\0';

            while (*p && *p != ':') p++;
            if (*p == ':') p++;
            while (*p && (*p == ' ' || *p == '\t')) p++;

            char value[256] = {0};
            if (*p == '"') {
                p++; 
                const char *val_start = p;
                while (*p && *p != '"') p++;
                int val_len = p - val_start;

                strncpy(value, val_start, val_len);
                value[val_len] = '\0';
                if (*p == '"') p++;
            } 
            else 
            {
                const char *val_start = p;
                while (!(!*p || *p == ',' || *p == '}')) p++;
                int val_len = p - val_start;
                while (val_len > 0 && (val_start[val_len - 1] == ' ' || val_start[val_len - 1] == '\t')) val_len--;
                strncpy(value, val_start, val_len);
                value[val_len] = '\0';
            }
            printf("%s%-15s\033[0m:%s %s\n",BOLD, key, RESET, value);
        } else {
            p++;
        }
    }
    printf("\n");
}

int main(){
    srand(time(NULL));
    signal(SIGINT, terminate);
    char buffer[MAX_BUFFER_SIZE];
    curr_transcn = rand() % 65535;
    for (int i = 0; i < MAX_QUERY; i++) TRANSACTIONS[i] = -1;

    int sockfd = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL_NO);
    switch(sockfd) 
    {
        case -1: perror("[-] socket failed"); exit(EXIT_FAILURE);
        default: printf("[+] Socket created successfully\n");
    }

    int broadcast = 1;
    int setopt1 = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    switch(setopt1) 
    {
        case -1: perror("[-] setsockopt failed"); exit(EXIT_FAILURE);
        default: printf("[+] Socket options set to SO_BROADCAST\n");
    }

    int optval = 1;
    int setopt2 = setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval));
    switch(setopt2) 
    {
        case -1: perror("[-] setsockopt failed"); exit(EXIT_FAILURE);
        default: printf("[+] Socket options set to IP_HDRINCL\n");
    }
    
    int rcv_buffer = 64 << 10; // 64 KB
    int setopt3 = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcv_buffer, sizeof(rcv_buffer));
    switch(setopt3)
    {
        case -1: perror("[-] setsockopt SO_RCVBUF failed"); exit(EXIT_FAILURE);
        default: printf("[+] Socket options set to SO_RCVBUF (64 KB)\n");
    }


    struct sockaddr_in server_addr;
    printf("[+] CLDP client started - listening for HELLO messages...\n\n");
    
    while(1)
    {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        socklen_t addr_len = sizeof(server_addr);
        int recv = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);

        switch(recv) 
        {
            case -1: perror("[-] recvfrom failed"); continue;
            default: printf("[+] Received %d bytes from %s\n", recv, inet_ntoa(server_addr.sin_addr));
        }

        if (recv < (int)sizeof(struct iphdr)) {
            fprintf(stderr, "[-] Received packet is smaller than the IP header. Packet ignored.\n");
            continue;
        }

        struct iphdr *IP_HEADER = (struct iphdr *)buffer;
        int IP_HEADER_LEN = IP_HEADER->ihl << 2;
        int len = IP_HEADER_LEN + sizeof(struct cldp_header);

        if (recv < len) 
        {
            fprintf(stderr, "[-] Packet size (%d bytes) is less than expected (%d bytes). Packet ignored.\n", recv, len);
            continue;
        }

        if (IP_HEADER->protocol != CLDP_PROTOCOL_NO) {
            fprintf(stderr, "[-] Unexpected IP protocol (%d). Expected protocol %d.\n", IP_HEADER->protocol, CLDP_PROTOCOL_NO);
            continue;
        }

        struct cldp_header *CLDP_HEADER = (struct cldp_header *)(buffer + IP_HEADER_LEN);
        switch (CLDP_HEADER->packet_type)
        {
            case CLDP_HELLO:
                printf("[+] Received HELLO from %s. [Packet size = %d bytes]\n", inet_ntoa(server_addr.sin_addr), recv);
                send_query(sockfd, &server_addr);
                break;

            case CLDP_RESPONSE:
                int id = CLDP_HEADER->transaction_id;
                if (!find_active_transaction(id)) break;
                printf("[+] Received RESPONSE from %s. [Packet size = %d bytes, Transaction ID = %d]\n", inet_ntoa(server_addr.sin_addr), recv, id);
                JSON_stringify(buffer + IP_HEADER_LEN + sizeof(struct cldp_header));
                break;

            case CLDP_QUERY: break;
            default:
                fprintf(stderr, "[-] Unknown CLDP message type (%d) received from %s.\n", CLDP_HEADER->packet_type, inet_ntoa(server_addr.sin_addr));
                break;
        }
    }

    close(sockfd);
    return 0;
}