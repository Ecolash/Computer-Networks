#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>         
#include <time.h>

#include <sys/socket.h>
#include <sys/sysinfo.h>   
#include <sys/ioctl.h>     
#include <arpa/inet.h>
#include <linux/ip.h>

#define BROADCAST_ADDRESS  "255.255.255.255"
#define MAX_BUFFER_SIZE     1024
#define CLDP_PROTOCOL_NO    253

#define CLDP_HELLO          0x01
#define CLDP_QUERY          0x02
#define CLDP_RESPONSE       0x03
#define CLDP_ALL_METADATA   0xFF

#define JSON_FORMAT "{\"Hostname\": \"%s\",\"CPU Load\": \"%s\",\"System Time\": \"%s\",\"Memory Usage\": \"%s\",\"Uptime\": \"%s\"}"

#pragma pack(push,1)
struct cldp_header {
    uint8_t packet_type;
    uint8_t packet_length;
    uint16_t transaction_id;
    uint32_t reserved;
};
#pragma pack(pop)

int sockfd;
struct sockaddr_in broadcast_addr;

void terminate() {
    printf("\nClosing socket and exiting...\n");
    close(sockfd);
    exit(0);
}

char* get_uptime() {
    static char uptime_str[100];
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        snprintf(uptime_str, sizeof(uptime_str), "N/A");
        return uptime_str;
    }
    
    div_t div_result;
    div_result = div(info.uptime, 60);
    long total_minutes = div_result.quot;
    long seconds = div_result.rem;

    div_result = div(total_minutes, 60);
    long total_hours = div_result.quot;
    long minutes = div_result.rem;

    div_result = div(total_hours, 24);
    long days = div_result.quot;
    long hours = div_result.rem;
    snprintf(uptime_str, sizeof(uptime_str), "%ld days, %ld hours, %ld minutes, %ld seconds",  days, hours, minutes, seconds);
    return uptime_str;
}

char* get_memory_usage() {
    static char buffer[256];
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        snprintf(buffer, sizeof(buffer), "Error retrieving memory info: %s", strerror(errno));
        return buffer;
    }
    
    if(info.totalram == 0) {
        snprintf(buffer, sizeof(buffer), "Total memory reported as zero");
        return buffer;
    }
    
    double total_mem = (double)info.totalram * info.mem_unit / (1024 * 1024);  // convert to MB
    double free_mem = (double)info.freeram * info.mem_unit / (1024 * 1024);    // convert to MB
    double used_mem = total_mem - free_mem;
    double usage_percent = (used_mem / total_mem) * 100.0;
    
    snprintf(buffer, sizeof(buffer), "%.2f MB used / %.2f MB total (%.2f%%)", used_mem, total_mem, usage_percent);
    return buffer;
}

char* get_cpu_load() {
    static char load_str[100];
    double loads[3];
    if (getloadavg(loads, 3) == -1) snprintf(load_str, sizeof(load_str), "N/A");
    else 
    {
        double avg = (loads[0] + loads[1] + loads[2]);
        avg = avg / 3.0;
        snprintf(load_str, sizeof(load_str), "avg load: %.2f, loads: %.2f, %.2f, %.2f", avg, loads[0], loads[1], loads[2]);
    }
    return load_str;
}

char* get_system_time() {
    static char time_str[50];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d %04d-%02d-%02d", tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return time_str;
}

char* get_host_name(){
    static char host_name[100];
    int success = gethostname(host_name, sizeof(host_name));
    if (success == -1) snprintf(host_name, sizeof(host_name), "Unknown Host");
    else host_name[sizeof(host_name) - 1] = '\0'; 
    return host_name;
}

char* get_all_metadata() {
    static char JSON[MAX_BUFFER_SIZE];
    char* hostname = get_host_name();
    char* cpu_load = get_cpu_load();
    char* system_time = get_system_time();
    char* memory_usage = get_memory_usage();
    char* uptime = get_uptime();
    
    snprintf(JSON, MAX_BUFFER_SIZE, JSON_FORMAT, hostname, cpu_load, system_time, memory_usage, uptime);
    return JSON;
}

unsigned short checksum(unsigned short *addr, int len) {
    unsigned int sum = 0;
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }
    if (len == 1) sum += *(unsigned char *)addr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum);
}

void handle_query(struct sockaddr_in *CLIENT_ADDR, struct cldp_header* RECV_CLDP) {
    char PACKET[MAX_BUFFER_SIZE];
    bzero(PACKET, sizeof(PACKET));

    struct iphdr *IP_HEADER = (struct iphdr*)PACKET;
    IP_HEADER->version = 4;
    IP_HEADER->ihl = 5;
    IP_HEADER->tos = 0;
    IP_HEADER->id = htons(54321);
    IP_HEADER->frag_off = 0;
    IP_HEADER->ttl = 64;

    IP_HEADER->protocol = CLDP_PROTOCOL_NO;
    IP_HEADER->saddr = INADDR_ANY;
    IP_HEADER->daddr = CLIENT_ADDR->sin_addr.s_addr;

    struct cldp_header *CLDP_HEADER = (struct cldp_header*)(PACKET + sizeof(struct iphdr));
    CLDP_HEADER->packet_type = CLDP_RESPONSE;
    CLDP_HEADER->transaction_id = RECV_CLDP->transaction_id;
    CLDP_HEADER->reserved = 0;

    char *JSON_DATA = get_all_metadata();
    char *DATA_PAYLOAD = (char*)(PACKET + sizeof(struct iphdr) + sizeof(struct cldp_header));
    strncpy(DATA_PAYLOAD, JSON_DATA, MAX_BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct cldp_header));

    CLDP_HEADER->packet_length = strlen(DATA_PAYLOAD);
    int len = sizeof(struct iphdr) + sizeof(struct cldp_header) + CLDP_HEADER->packet_length;
    IP_HEADER->tot_len = htons(len);
    IP_HEADER->check = checksum((unsigned short *)IP_HEADER, sizeof(struct iphdr));

    int sent = sendto(sockfd, PACKET, len, 0,(struct sockaddr*)CLIENT_ADDR, sizeof(*CLIENT_ADDR));
    switch (sent) {
        case -1:
            perror("sendto failed");
            exit(EXIT_FAILURE);

        default:
            printf("[+] Sent RESPONSE to %s ", inet_ntoa(CLIENT_ADDR->sin_addr));
            printf("[Size = %d | Transaction ID = %d ]\n", sent, ntohs(CLDP_HEADER->transaction_id));
            printf("[+] JSON Response:\n%s\n\n", DATA_PAYLOAD);
            fflush(stdout);
            return;
    }
}


void *HELLO_THREAD__(){
    uint16_t transaction_id = rand() % 65535;
    printf("[+] HELLO Thread started. Sending HELLO every 10 seconds...\n");

    while (1)
    {
        transaction_id++;
        char packet[MAX_BUFFER_SIZE];
        bzero(packet, sizeof(packet));
        int len = sizeof(struct iphdr) + sizeof(struct cldp_header);

        struct iphdr* IP_HEADER = (struct iphdr*)packet;
        IP_HEADER->version = 4;
        IP_HEADER->ihl = 5;
        IP_HEADER->tos = 0;
        IP_HEADER->ttl = 64;
        IP_HEADER->frag_off = 0;
        IP_HEADER->tot_len = len;

        IP_HEADER->saddr = INADDR_ANY;
        IP_HEADER->protocol = CLDP_PROTOCOL_NO;
        IP_HEADER->daddr = inet_addr(BROADCAST_ADDRESS);
        IP_HEADER->check = checksum((unsigned short *)IP_HEADER, sizeof(struct iphdr));
        IP_HEADER->id = htons(54321);

        struct cldp_header *CLDP_HEADER = (struct cldp_header *)(packet + sizeof(struct iphdr));
        CLDP_HEADER->packet_type = CLDP_HELLO;
        CLDP_HEADER->packet_length = 0;
        CLDP_HEADER->transaction_id = htons(transaction_id);
        CLDP_HEADER->reserved = 0;

        int sent = sendto(sockfd, packet, len, 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
        switch (sent) {
            case -1:
                perror("sendto failed");
                exit(EXIT_FAILURE);

            default:
                printf("[+] Sent HELLO to %s ", inet_ntoa(broadcast_addr.sin_addr));
                printf("[Size = %d | Transaction ID = %d ]\n", sent, ntohs(CLDP_HEADER->transaction_id));
                fflush(stdout);
        }
        sleep(10);
    }
    return NULL;
}

int main(){
    srand(time(NULL));
    signal(SIGINT, terminate);
    sockfd = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL_NO);
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

    struct sockaddr_in serveraddr, clientaddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    memset(&clientaddr, 0, sizeof(clientaddr));
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDRESS);
    
    pthread_t hello_thread;
    printf("[+] CLDP Server started\n");
    int thread1 = pthread_create(&hello_thread, NULL, HELLO_THREAD__, NULL);
    if (thread1 != 0) {
        perror("[-] pthread_create failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_BUFFER_SIZE];
    while (1) {
        socklen_t addrlen = sizeof(clientaddr);
        int recv = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,(struct sockaddr *)&clientaddr, &addrlen);
        
        switch(recv) 
        {
            case -1: perror("[-] recvfrom failed"); continue;
            default: printf("[+] Received %d bytes from %s\n", recv, inet_ntoa(clientaddr.sin_addr));
        }

        struct iphdr *IP_HEADER = (struct iphdr *)buffer;
        int IP_HEADER_LEN = IP_HEADER->ihl << 2;

        int len = IP_HEADER_LEN + sizeof(struct cldp_header);
        if (recv < len) continue;
        struct cldp_header *CLDP_HEADER = (struct cldp_header *)(buffer + IP_HEADER_LEN);

        if (IP_HEADER->protocol != CLDP_PROTOCOL_NO) continue;
        int packet_type = CLDP_HEADER->packet_type;

        switch (packet_type) {
            case CLDP_RESPONSE: break;
            case CLDP_HELLO: printf("[+] Received HELLO from %s\n", inet_ntoa(clientaddr.sin_addr));  break;
            case CLDP_QUERY: printf("[+] Received QUERY from %s. [Transaction ID = %d]\n", inet_ntoa(clientaddr.sin_addr), CLDP_HEADER->transaction_id); handle_query(&clientaddr, CLDP_HEADER); break;
            default: printf("[-] Unknown CLDP message. [packet_type = %d]\n", CLDP_HEADER->packet_type);
        }
    }

    close(sockfd);
    return 0;
}