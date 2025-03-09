#include "ksocket.h"
#include <assert.h>

struct SM_entry * SM;
K_SOCKET * sock_info;

/*
===============================================================================
| Variable            | Description                                           |
|---------------------|-------------------------------------------------------|
| int sem1            | Semaphore ID for general synchronization.             |
| int sem2            | Semaphore ID for general synchronization.             |
| int sock_info_sem   | Semaphore ID for socket information sync.             |
| int SM_sem          | Semaphore ID for shared memory sync.                  |
| int shmid_sock_info | Shared memory ID for socket information.              |
| int shmid_SM        | Shared memory ID for general shared memory.           |
| struct sembuf pop   | Semaphore operation structure for 'wait' (P) ops.     |
| struct sembuf vop   | Semaphore operation structure for 'signal' (V) ops.   |
===============================================================================

pop and vop are used to perform wait and signal operations on the semaphores.
pop = {0, -1, 0} => wait operation
vop = {0, 1, 0}  => signal operation

 */
int sem1, sem2;
int sock_info_sem, SM_sem;
int shmid_sock_info, shmid_SM;

struct sembuf pop = {0, -1, 0};
struct sembuf vop = {0, 1, 0};

int access_SM() {
    key_t K1 = KEY_SHMID_SOCK_INFO;
    key_t K2 = KEY_SHMID_SM;
    key_t K3 = KEY_SEM1;
    key_t K4 = KEY_SEM2;
    key_t K5 = KEY_SEM_SM;
    key_t K6 = KEY_SEM_SOCK_INFO;

    if ((shmid_sock_info = shmget(K1, sizeof(K_SOCKET), 0666)) == -1     ||
        (shmid_SM = shmget(K2, sizeof(struct SM_entry) * N, 0666)) == -1 ||
        (sock_info_sem = semget(K6, 1, 0666)) == -1 ||
        (SM_sem = semget(K5, 1, 0666)) == -1 ||
        (sem1 = semget(K3, 1, 0666)) == -1   ||
        (sem2 = semget(K4, 1, 0666)) == -1) {
            perror("[-] Initprocess is not running.");
            return -1;
    }
    sock_info = (K_SOCKET *)shmat(shmid_sock_info, NULL, 0);
    SM = (struct SM_entry *)shmat(shmid_SM, NULL, 0);
    return 0;
}

void encode(struct Segment *seg, char *result)
{
    memset(result, 0, 530);
    int len = seg->len - 1;
    result[0] = seg->type ? '1' : '0';
    for (int i = 0; i < 8; i++) result[1 + i] = ((seg->seq_num >> (7 - i)) & 1) ? '1' : '0';
    for (int i = 0; i < 9; i++) result[9 + i] = ((seg->type == 0? seg->rwnd : len) >> (8 - i)) & 1 ? '1' : '0';
    if (seg->type == 1) memcpy(result + 18, seg->data, seg->len);
}

void decode(const char *str, struct Segment *seg)
{
    uint16_t val = 0;
    seg->type = (str[0] == '1')? 1 : 0;
    seg->rwnd = seg->len = 0;
    seg->seq_num = 0;

    for (int i = 0; i < 8; i++) seg->seq_num = (seg->seq_num << 1) | (str[1 + i] - '0');
    for (int i = 0; i < 9; i++) val = (val << 1) | (str[9 + i] - '0');
    if (seg->type == 0) seg->rwnd = val;
    else seg->len = val + 1;
    if (seg->type == 1) memcpy(seg->data, str + 18, seg->len);
}

int k_socket(int domain, int type, int protocol) 
{
    if (access_SM() == -1) return -1;
    if (domain != AF_INET) { errno = EINVAL; return -1; }
    if (type != SOCK_KTP) { errno = EINVAL; return -1; }
    
    // -- FIND A FREE SOCKET
    int ksock_ID = -1;
    P(SM_sem);
    for (int i = 0; i < N; i++) if (SM[i].is_free) { ksock_ID = i; break; }
    V(SM_sem);
    
    // -- NO FREE SOCKET FOUND
    P(sock_info_sem);
    if (ksock_ID == -1) {
        errno = ENOBUFS;
        sock_info->err_no = ENOBUFS;
        sock_info->allocated = 0;
        V(sock_info_sem);
        return -1;
    }
    V(sock_info_sem);
    V(sem1);

    // -- FREE SOCKET FOUND - Allocate the socket
    P(sem2);
    P(sock_info_sem);
    if (sock_info->sock_id == -1)
    {
        errno = sock_info->err_no;
        sock_info->allocated = 0;
        V(sock_info_sem);
        return -1;
    }
    V(sock_info_sem);

    P(SM_sem);
    SM[ksock_ID].is_free = 0;
    SM[ksock_ID].process_id = getpid();
    SM[ksock_ID].udp_FD = sock_info->sock_id;

    for (int j = 0; j < 256; j++)
    {
        SM[ksock_ID].swnd.wndw[j] = -1;
        SM[ksock_ID].timer[j] = -1;
        if (j > 0 && j <= 10) SM[ksock_ID].rwnd.wndw[j] = j - 1;
        else SM[ksock_ID].rwnd.wndw[j] = -1;
    }

    // Initialize the send and receive window default values
    SM[ksock_ID].swnd.size = 10;
    SM[ksock_ID].rwnd.size = 10;
    SM[ksock_ID].swnd.start_seq = 1;
    SM[ksock_ID].rwnd.start_seq = 1;
    SM[ksock_ID].SEND_BUFFER_SIZE = 10;

    for (int j = 0; j < 10; j++) SM[ksock_ID].RECV_BUFFER_ISVALID[j] = 0;
    SM[ksock_ID].RECV_BUFFER_PTR = 0;
    SM[ksock_ID].nospace = 0;
    V(SM_sem);

    P(sock_info_sem);
    sock_info->sock_id = -1;
    sock_info->allocated = 0; // Transfer of info done - reset the flag
    memset(sock_info->ip_address, 0, 16);
    V(sock_info_sem);
    return ksock_ID; 
}

int k_bind(char src_ip[], uint16_t src_port, char dst_ip[], uint16_t dst_port) {
    if (access_SM() == -1) return -1;
    if (!IP_check(src_ip) || !PORT_check2(src_port)) { errno = EINVAL; return -1; }
    if (!IP_check(dst_ip) || !PORT_check2(dst_port)) { errno = EINVAL; return -1; }

    P(SM_sem);
    int KSOCK_ID = -1;
    for (int i = 0; i < N && KSOCK_ID == -1; i++)
    {
        if (SM[i].is_free) continue;
        if (SM[i].process_id == getpid()) KSOCK_ID = i;
    }

    P(sock_info_sem);
    if (KSOCK_ID == -1) {
        errno = ENOBUFS;
        sock_info->allocated = 0;
        V(SM_sem);
        V(sock_info_sem);
        return -1;
    }

    sock_info->allocated = 1;
    sock_info->sock_id = SM[KSOCK_ID].udp_FD;
    strcpy(sock_info->ip_address, src_ip);
    sock_info->port = src_port;
    V(sock_info_sem); 
    V(sem1);

    P(sem2); 
    P(sock_info_sem);
    if (sock_info->sock_id == -1)
    {
        errno = sock_info->err_no;
        sock_info->allocated = 0;
        V(sock_info_sem);
        V(SM_sem);
        return -1;
    }
    V(sock_info_sem);

    strcpy(SM[KSOCK_ID].ip_address, dst_ip);
    SM[KSOCK_ID].port = dst_port;
    V(SM_sem);

    P(sock_info_sem);
    sock_info->sock_id = -1;
    sock_info->allocated = 0;
    memset(sock_info->ip_address, 0, 16);
    V(sock_info_sem);
    return 0;
}

ssize_t k_sendto(int k_sockfd, const void *buf, size_t len, int flags, const SPTR dst_addr, socklen_t addrlen) {
    if (access_SM() == -1) return -1; 
    P(SM_sem);
    struct sockaddr_in *addr_in = (struct sockaddr_in *)dst_addr;
    char *dst_ip = inet_ntoa(addr_in->sin_addr);
    uint16_t dst_port = ntohs(addr_in->sin_port);

    if (k_sockfd < 0 || k_sockfd >= N) { errno = EBADF; V(SM_sem); return -1; }
    if (SM[k_sockfd].is_free == 1)     { errno = EBADF; V(SM_sem); return -1; }

    if (strcmp(SM[k_sockfd].ip_address, dst_ip)) { errno = ENOTCONN; V(SM_sem); return -1; } // IP doesn't match
    if (SM[k_sockfd].port != dst_port) { errno = ENOTCONN; V(SM_sem); return -1; }           // Port doesn't match
    if (SM[k_sockfd].SEND_BUFFER_SIZE == 0) { errno = ENOBUFS; V(SM_sem); return -1; }

    int buff_index = -1;
    int seq = SM[k_sockfd].swnd.start_seq;
    while (SM[k_sockfd].swnd.wndw[seq] != -1) seq = (seq + 1) % 256;

    for (int i = 0; i < 10 && buff_index == -1; i++) {
        int occupied = 0;
        for (int j = 0; j < 256 && !occupied; j++) if (SM[k_sockfd].swnd.wndw[j] == i) occupied = 1;
        if (!occupied) buff_index = i;
    }

    if (buff_index == -1) { errno = ENOBUFS; V(SM_sem); return -1; }

    // printf("seq: %d\n", seq);
    // printf("buff_index: %d\n", buff_index);
    SM[k_sockfd].swnd.wndw[seq] = buff_index;
    memcpy(SM[k_sockfd].SEND_BUFFER[buff_index], buf, len);
    SM[k_sockfd].timer[seq] = -1;
    SM[k_sockfd].SEND_BUFFER_SIZE--;
    SM[k_sockfd].SEND_MSG_SIZES[buff_index] = len;
    V(SM_sem);
    return len;
}

ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, SPTR src_addr, socklen_t *addrlen) {
    if (access_SM() == -1) return -1; 
    P(SM_sem);
    if (sockfd < 0 || sockfd >= N) { errno = EBADF; V(SM_sem); return -1; }
    if (SM[sockfd].is_free == 1)  { errno = EBADF; V(SM_sem); return -1; }

    struct SM_entry *sm = &SM[sockfd];
    if (sm->RECV_BUFFER_ISVALID[sm->RECV_BUFFER_PTR]) 
    {
        int seq = -1;
        sm->rwnd.size++;
        sm->RECV_BUFFER_ISVALID[sm->RECV_BUFFER_PTR] = 0; 

        for (int i = 0; i < 256 && seq == -1; i++) if (sm->rwnd.wndw[i] == sm->RECV_BUFFER_PTR) seq = i;
        sm->rwnd.wndw[seq] = -1;
        sm->rwnd.wndw[(seq + 10) % 256] = sm->RECV_BUFFER_PTR;

        int n = sm->RECV_MSG_SIZES[sm->RECV_BUFFER_PTR];
        n = (len < n) ? len : n;
        memcpy(buf, sm->RECV_BUFFER[sm->RECV_BUFFER_PTR], n);
        sm->RECV_BUFFER_PTR = (sm->RECV_BUFFER_PTR + 1) % 10;
        V(SM_sem);
        return n;
    }
    errno = ENOMSG; 
    V(SM_sem);
    return -1;
}

int k_close(int sockfd) {
    if (access_SM() == -1) return -1;
    P(SM_sem);
    SM[sockfd].is_free = 1;
    V(SM_sem);
    return 0;
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

int PORT_check2(uint16_t port)
{
    int f1 = port < 0;
    int f2 = port > 65535;
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
