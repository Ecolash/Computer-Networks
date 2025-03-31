#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <unistd.h>
#include <time.h>

#define CLDP_PROTO 253
#define BUFFER_SIZE 1024
#define MAX_QUERY 10

#define CLDP_HELLO 0x01
#define CLDP_QUERY 0x02
#define CLDP_RESPONSE 0x03

#define HOSTNAME 0x10
#define CPU_LOAD 0x11
#define SYSTEM_TIME 0x12
#define NETWORK_STATUS 0x13
#define MEMORY_USAGE 0x14

#pragma pack(push,1)
struct cldp_header {
    uint8_t type;
    uint8_t length;
    uint16_t transaction_id;
    uint32_t reserved;
};
#pragma pack(pop)

int last_transaction;
int current_transactions[MAX_QUERY];
int query_type;

void display_menu() {
    printf("Choose metadata to request:\n");
    printf("1. Hostname\n");
    printf("2. CPU Load\n");
    printf("3. System Time\n");
    printf("4. Network Status\n");
    printf("5. Memory Usage\n");
    printf("Enter your choice: ");
}

unsigned short calculate_checksum(unsigned short *addr, int len) {
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

int found(uint16_t trans_id){
    for(int i=0;i<MAX_QUERY;i++){
        if(current_transactions[i]==trans_id){
            current_transactions[i]=-1;
            return 1;
        }
    }
    return 0;
}

void send_query(int sockfd, struct sockaddr_in *server_addr, uint16_t trans_id){
    char packet[BUFFER_SIZE];
    memset(packet, 0, BUFFER_SIZE);

    struct iphdr *ip_header = (struct iphdr *)packet;
    ip_header->version = 4;
    ip_header->ihl = 5;
    ip_header->tos = 0;
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct cldp_header));
    ip_header->id = htons(54321);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = CLDP_PROTO;
    ip_header->saddr = INADDR_ANY;
    ip_header->daddr = server_addr->sin_addr.s_addr;
    ip_header->check = calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));


    struct cldp_header *cldp = (struct cldp_header *)(packet + sizeof(struct iphdr));
    cldp->type = CLDP_QUERY;
    cldp->length = 0;
    cldp->transaction_id = last_transaction;
    switch(query_type) {
        case 1:
            cldp->reserved = HOSTNAME;
            break;
        case 2:
            cldp->reserved = CPU_LOAD;
            break;
        case 3:
            cldp->reserved = SYSTEM_TIME;
            break;
        case 4:
            cldp->reserved = NETWORK_STATUS;
            break;
        case 5:
            cldp->reserved = MEMORY_USAGE;
            break;
        default:
            printf("[-] Invalid query type\n");
            return;
    }
    last_transaction++;
    int done=0;
    for(int i=0;i<MAX_QUERY;i++){
        if(current_transactions[i]==-1){
            current_transactions[i]=cldp->transaction_id;
            done=1;
            break;
        }
    }
    if(!done){
        printf("[-] Maximum query limit is reached.Wait for responses...\n");
        return;
    }


    int sent_bytes=sendto(sockfd,packet,sizeof(struct iphdr)+ sizeof(struct cldp_header),0,
                        (struct sockaddr*)server_addr, sizeof(*server_addr));

    // printf("Full packet (%d bytes): ", sent_bytes);
    // for (int i = 0; i < sent_bytes; i++) {
    //     printf("%02x ", (unsigned char)packet[i]);
    // }
    // printf("\n");
        
    if(sent_bytes<0){
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }
    printf("[+] Sent QUERY to server %s. [ Packet size = %d, Transaction ID = %d ]\n",inet_ntoa(server_addr->sin_addr),sent_bytes,cldp->transaction_id);
}


int main(){
    srand(time(NULL));

    int sockfd = socket(AF_INET, SOCK_RAW, CLDP_PROTO);
    if(sockfd<0){
        perror("[-] socket creation failed");
        exit(EXIT_FAILURE);
    }

    //Enable broadcast
    int enable_broadcast=1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(enable_broadcast))<0){
        perror("[-] setsockopt(SO_BROADCAST) failed");
        exit(EXIT_FAILURE);
    }

    //Enable IP_HDRINCL (since we're constructing the IP Header manually)
    int optval=1;
    if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval))<0){
        perror("[-] setsockopt(IP_HDRINCL) failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    int rcvbuf = 65536; // 64KB
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf))<0){
        perror("[-] setsockopt failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    for(int i=0;i<MAX_QUERY;i++){
        current_transactions[i]=-1;
    }


    struct sockaddr_in client_addr,server_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = 0;
    client_addr.sin_addr.s_addr = INADDR_ANY;

    // **Display menu and take user input**

    display_menu();
    scanf("%d", &query_type);

    if (query_type < 1 || query_type > 5) {
        printf("Invalid choice. Exiting...\n");
        close(sockfd);
        return 1;
    }


    printf("[+] CLDP client listening for HELLO messages...\n");
    char buffer[BUFFER_SIZE];
    last_transaction = rand()%65535;

    while(1){

        memset(buffer,0,BUFFER_SIZE);
        socklen_t addr_len=sizeof(server_addr);
        int bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &addr_len);
        if(bytes_received < 0){
            perror("[-] recvfrom failed");
            continue;
        }

        struct iphdr *ip_header = (struct iphdr*)buffer;
        int ip_hdr_len = ip_header->ihl*4;
        if(bytes_received < ip_hdr_len + sizeof(struct cldp_header)){
            continue;
        }
        struct cldp_header *cldp = (struct cldp_header*)(buffer + ip_hdr_len);

        if(ip_header->protocol != CLDP_PROTO){
            continue;
        }

        // printf("Raw message bytes: ");
        // for (int i = 0; i < bytes_received; i++) {
        //     printf("%02x ", (unsigned char)buffer[i]);
        // }
        // printf("\n");

        switch(cldp->type){
            case CLDP_HELLO:
                printf("[+] Received HELLO from %s. [ Packet size = %d ]\n", inet_ntoa(server_addr.sin_addr),bytes_received);
                send_query(sockfd, &server_addr, cldp->transaction_id);
                break;
            case CLDP_RESPONSE:
                if(!found(cldp->transaction_id)) break;
                printf("[+] Received RESPONSE from %s. [ Packet size = %d, Transaction ID = %d ]\n", inet_ntoa(server_addr.sin_addr),
                        bytes_received, cldp->transaction_id);
                printf("[+]\t%s\n\n",buffer + sizeof(struct iphdr) + sizeof(struct cldp_header));
                break;
            case CLDP_QUERY:
                break;
            default:
                printf("[-] Unknown CLDP message. [Type = %d ]\n",cldp->type);
        }
    }

    close(sockfd);
    return 0;

}