#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include <sys/sysinfo.h>   // For struct sysinfo
#include <sys/ioctl.h>     // For ioctl
#include <net/if.h>        // For struct ifreq, struct ifconf, SIOCGIFCONF, SIOCGIFFLAGS
#include <errno.h>         // For errno

#define CLDP_PROTO 253
#define BUFFER_SIZE 1024


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

int sockfd;
struct sockaddr_in broadcast_addr;

void handle_sigint(int sig) {
    printf("\nClosing socket and exiting...\n");
    close(sockfd);
    exit(0);
}

char* get_memory_usage() {
    static char buffer[256];
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        snprintf(buffer, sizeof(buffer), "Error retrieving memory info: %s", strerror(errno));
        return buffer;
    }
    double total_mem = info.totalram * info.mem_unit / (1024.0 * 1024.0);  // In MB
    double free_mem = info.freeram * info.mem_unit / (1024.0 * 1024.0);    // In MB
    double used_mem = total_mem - free_mem;
    double usage_percent = (used_mem / total_mem) * 100.0;
    
    snprintf(buffer, sizeof(buffer), "Memory usage: %.2f MB / %.2f MB (%.2f%%)",
             used_mem, total_mem, usage_percent);
    return buffer;
}

char* get_cpu_load() {
    static char load_str[50];
    double load;
    if (getloadavg(&load, 1) == -1) {
        strcpy(load_str, "N/A");
    } else {
        snprintf(load_str, sizeof(load_str), "CPU Load: %.2f", load);
    }
    return load_str;
}

char* get_system_time() {
    static char time_str[50];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(time_str, sizeof(time_str), "System Time: %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return time_str;
}

char* get_host_name(){
    static char host_name[100];
    char host[51];
    gethostname(host, 50);
    snprintf(host_name, sizeof(host_name), "Hostname: %s",host);
    return host_name;
}

char* get_network_status() {
    static char buffer[512];  // Static buffer persists across function calls
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    // Clear the buffer before use
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Network interfaces: ");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        return "Error creating socket for network interface check";
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
        close(sock);
        return "Error getting network interfaces";
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    char* ptr = buffer + strlen(buffer);

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) { 
                ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer),
                              "%s [%s], ", 
                              ifr.ifr_name, 
                              (ifr.ifr_flags & IFF_UP) ? "UP" : "DOWN");
                success = 1;
            }
        }
    }
    close(sock);

    if (!success) {
        return "No active network interfaces found";
    }
    

    if (ptr > buffer + strlen("Network interfaces: ")) {
        *(ptr - 2) = '\0';
    }    

    return buffer;
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

void *send_hello(void* arg){
    uint16_t transaction_id = rand()%65535;
    while(1){
        transaction_id++;
        char packet[BUFFER_SIZE];
        memset(packet, 0, BUFFER_SIZE);

        //Fill in IP header
        struct iphdr* ip_header = (struct iphdr*)packet;
        ip_header->version = 4;
        ip_header->ihl = 5;
        ip_header->tos = 0;
        ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct cldp_header));
        ip_header->ttl = 64;
        ip_header->protocol = CLDP_PROTO;
        ip_header->saddr=INADDR_ANY;
        ip_header->daddr = inet_addr("255.255.255.255");
        ip_header->check = calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));
        ip_header->frag_off = 0;
        ip_header->id=htons(54321);

        //Fill in CLDP Header
        struct cldp_header *cldp=(struct cldp_header*)(packet + sizeof(struct iphdr));
        cldp->type = CLDP_HELLO;
        cldp->length = 0;
        cldp->transaction_id = htons(transaction_id);
        cldp->reserved = 0;

        int sent_bytes=sendto(sockfd,packet,sizeof(struct iphdr)+ sizeof(struct cldp_header),0,
                        (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        
        if(sent_bytes<0){
            perror("sendto failed");
            exit(EXIT_FAILURE);
        }
        printf("[+] Broadcasted HELLO on the network [ Packet Size = %d ]\n",sent_bytes);

        sleep(10);
    }

    return NULL;
}

void handle_query(struct sockaddr_in *clientaddr,struct cldp_header* recv_cldp){
    char response_packet[BUFFER_SIZE];
    memset(response_packet,0,BUFFER_SIZE);

    struct iphdr *ip_header=(struct iphdr*)response_packet;
    ip_header->version = 4;
    ip_header->ihl = 5;
    ip_header->tos = 0;
    ip_header->id = htons(54321);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = CLDP_PROTO;
    ip_header->saddr = INADDR_ANY;
    ip_header->daddr = clientaddr->sin_addr.s_addr;
    ip_header->check = calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));

    struct cldp_header *cldp = (struct cldp_header*)(response_packet + sizeof(struct iphdr));
    cldp->type = CLDP_RESPONSE;
    cldp->transaction_id = recv_cldp->transaction_id;
    cldp->reserved = 0;

    //Fill data
    char *payload=(char*)(response_packet + sizeof(struct iphdr) + sizeof(struct cldp_header));
    switch (recv_cldp->reserved) {
        case HOSTNAME:
            strncpy(payload, get_host_name(), BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct cldp_header));
            break;
        case CPU_LOAD:
            strncpy(payload, get_cpu_load(), BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct cldp_header));
            break;
        case SYSTEM_TIME:
            strncpy(payload, get_system_time(), BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct cldp_header));
            break;
        case NETWORK_STATUS:
            strncpy(payload, get_network_status(), BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct cldp_header));
            break;
        case MEMORY_USAGE:
            strncpy(payload, get_memory_usage(), BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct cldp_header));
            break;
        default:
            printf("[-] Invalid metadata is being accessed\n");
            return;
    }
    cldp->length = strlen(payload);
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct cldp_header) + cldp->length);



    int sent_bytes=sendto(sockfd,response_packet,sizeof(struct iphdr)+ sizeof(struct cldp_header) + cldp->length,0,
                    (struct sockaddr*)clientaddr, sizeof(*clientaddr));
    
    if(sent_bytes<0){
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }

    printf("[+] Sent RESPONSE back to client. [Packet size = %d, Transaction ID = %d ]\n",sent_bytes,cldp->transaction_id);
    printf("[+] Payload = %s\n",payload);
}

int main(){
    srand(time(NULL));
    signal(SIGINT,handle_sigint);
    sockfd=socket(AF_INET, SOCK_RAW, CLDP_PROTO);
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

    struct sockaddr_in serveraddr,clientaddr;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=0;
    serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);

    broadcast_addr.sin_family=AF_INET;
    broadcast_addr.sin_port = 0;
    broadcast_addr.sin_addr.s_addr=inet_addr("255.255.255.255");

    printf("[+] CLDP server started. Sending HELLO every 10 seconds\n");

    pthread_t hello_thread;

    if(pthread_create(&hello_thread, NULL,send_hello,NULL)!=0){
        perror("[-] pthread_create");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];

    while(1){
        socklen_t addrlen=sizeof(clientaddr);
        int bytes_received=recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&clientaddr, &addrlen);
        if(bytes_received<0){
            perror("recvfrom failed");
            continue;
        }
        
        struct iphdr*  ip_header=(struct iphdr*)buffer;
        int ip_hdr_len = ip_header->ihl*4;
        if(bytes_received < ip_hdr_len + sizeof(struct cldp_header)){
            continue;
        }
        struct cldp_header *cldp = (struct cldp_header*)(buffer + ip_hdr_len);

        if(ip_header->protocol!=CLDP_PROTO){
            continue;
        }

        // printf("Raw message bytes: ");
        // for (int i = 0; i < 20; i++) {
        //     printf("%02x ", (unsigned char)buffer[i]);
        // }
        // printf("\n");

        switch(cldp->type){
            case CLDP_HELLO:
                printf("[+] Received HELLO from %s\n", inet_ntoa(clientaddr.sin_addr));
                break;
            case CLDP_QUERY:
                printf("[+] Received QUERY from %s. [Transaction ID = %d ]\n", inet_ntoa(clientaddr.sin_addr),cldp->transaction_id);
                handle_query( &clientaddr, cldp);
                break;
            case CLDP_RESPONSE:
                break;
            default:
                printf("[-] Unknown CLDP message. [ Type = %d ]\n",cldp->type);
        }       
    }

    close(sockfd);
    return 0;
}