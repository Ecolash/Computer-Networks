#include "ksocket.h"
#include <assert.h>
#include <signal.h>
#include <sys/time.h>

int timeout_flag = 0;
void alarm_handler(int signo) {  timeout_flag = 1; }

void encode(struct Segment *seg, char *result)
{
    memset(result, 0, 530);
    int len = seg->len - 1;
    result[0] = seg->type ? '1' : '0';
    for (int i = 0; i < 8; i++) result[1 + i] = ((seg->seq_num >> (7 - i)) & 1) ? '1' : '0';
    for (int i = 0; i < 9; i++) result[9 + i] = ((len) >> (8 - i)) & 1 ? '1' : '0';
    if (seg->type == 1) memcpy(result + 18, seg->data, seg->len);
}

void decode(const char *str, struct Segment *seg)
{
    uint16_t val = 0;
    seg->type = (str[0] == '1') ? 1 : 0;
    seg->len = 0;
    seg->seq_num = 0;

    for (int i = 0; i < 8; i++) seg->seq_num = (seg->seq_num << 1) | (str[1 + i] - '0');
    for (int i = 0; i < 9; i++) val = (val << 1) | (str[9 + i] - '0');
    if (seg->type == 1) seg->len = val + 1;

    if (seg->type == 1) memcpy(seg->data, str + 18, seg->len);
}

int k_socket(int domain, int type, int protocol)
{
    if (type != SOCK_KTP)
    {
        errno = EPROTOTYPE;
        return -1;
    }
    return socket(domain, SOCK_DGRAM, protocol);
}

int k_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) { return bind(sockfd, addr, addrlen); }
int k_close(int sockfd) { return close(sockfd); }

void send_segment(int sockfd, struct Segment *seg, const struct sockaddr *dst_addr, socklen_t addrlen)
{
    char encoded[530];
    encode(seg, encoded);
    if (!dropMessage())  sendto(sockfd, encoded, sizeof(encoded), 0, dst_addr, addrlen);
}

int receive_ack(int sockfd, uint16_t expected_seq, struct Segment *seg,  const struct sockaddr *dst_addr, socklen_t addrlen)
{
    struct Segment ack;
    char buffer[530];
    struct timeval tv;
    fd_set readfds;
    int MAX_ATTEMPTS = __MAX_RETRY__; 
    int retry_count = 0;

    while (retry_count < MAX_ATTEMPTS) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = T; 
        tv.tv_usec = 0;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        
        if (ready < 0) {
            if (errno == EINTR) continue;  
            return -1;  
        }
        
        if (ready == 0) {  
            printf("[-] TIMEOUT: Retransmitting... (%2d/%2d)\n",  retry_count + 1, MAX_ATTEMPTS);
            send_segment(sockfd, seg, dst_addr, addrlen); 
            retry_count++;
            continue;
        }

        ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (received < 0) {
            if (errno == EINTR) continue; 
            return -1; 
        }

        decode(buffer, &ack);
        if (ack.type == 0 && ack.seq_num == expected_seq) {
            return 0;  
        }
    }

    return -1;  
}

ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dst_addr, socklen_t addrlen)
{
    static uint16_t seq_num = 0;  
    struct Segment seg = {
        .type = 1,
        .seq_num = seq_num,
        .len = len,
        .data = {0}
    };
    memcpy(seg.data, buf, len);
    send_segment(sockfd, &seg, dst_addr, addrlen);
    
    if (receive_ack(sockfd, seq_num, &seg, dst_addr, addrlen) < 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    seq_num = (seq_num + 1) % 256;  
    return len;
}

ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags,  struct sockaddr *src_addr, socklen_t *addrlen)
{
    struct Segment seg;
    char buffer[530];
    static uint16_t expected_seq = 0; 

    while (1) {
        ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer), 0, src_addr, addrlen);
        if (received < 0) return -1;

        decode(buffer, &seg);
        if (seg.type == 1) {
            struct Segment ack = {0, seg.seq_num, 0, {0}};
            send_segment(sockfd, &ack, src_addr, *addrlen);

            if (seg.seq_num == expected_seq) {
                memcpy(buf, seg.data, (len < seg.len) ? len : seg.len);
                expected_seq = (expected_seq + 1) % 256; 
                return (len < seg.len) ? len : seg.len;
            }
            continue;
        }
        errno = ENOMSG;
        return -1;
    }
}

int dropMessage() {
    double r = (double)rand() / (double)RAND_MAX;
    if (r < p) return 1;
    return 0;
}

int IP_check(char *ip)
{
    struct sockaddr_in sa;
    int check = inet_pton(AF_INET, ip, &(sa.sin_addr));
    return (check != 0);
}

int PORT_check(char *port)
{
    int port_num = atoi(port);
    int f1 = port_num < 0;
    int f2 = port_num > 65535;
    return !(f1 || f2);
}


void argcheck(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <src_IP> <src_port> <dst_IP> <dst_port>\n", argv[0]);
        printf("-- <src_IP>    : Source IP address\n");
        printf("-- <src_port>  : Source port number\n");
        printf("-- <dst_IP>   : Destination IP address\n");
        printf("-- <dst_port> : Destination port number\n");
        exit(EXIT_FAILURE);
    }

    if (!IP_check(argv[1]) || !PORT_check(argv[2]) || !IP_check(argv[3]) || !PORT_check(argv[4])) {
        if (!IP_check(argv[1]))   printf("[-] Invalid source IP address\n");
        if (!IP_check(argv[3]))   printf("[-] Invalid destination IP address\n");
        if (!PORT_check(argv[2])) printf("[-] Invalid source port number\n");
        if (!PORT_check(argv[4])) printf("[-] Invalid destination port number\n");

        printf("\n[-] Terminating the program....\n");
        exit(EXIT_FAILURE);
    }
}
