#ifndef KSOCKET_H
#define KSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

/*
 Definitions:
 =========================================================================
    SOCK_KTP        - Special socket type for KTP
    MAX_KTP_SOCKETS - Maximum number of KTP sockets
    KTP_FD_BASE     - Base file descriptor number for KTP sockets
    MESSAGE_SIZE    - Size of each message in the buffer
    KTP_BUFFER_SIZE - Number of messages in the buffer
 =========================================================================
 */
#define SOCK_KTP            9999
#define MAX_KTP_SOCKETS     100
#define KTP_FD_BASE         1000
#define MESSAGE_SIZE        512
#define KTP_BUFFER_SIZE     10
/*
 Error codes:
 =========================================================================
    KTP_ENOSPACE    - No free slot in SM or send buffer full
    KTP_ENOTBOUND   - Destination not matching bound destination
    KTP_ENOMESSAGE  - No message available in recv buffer
=========================================================================
 */
#define KTP_ENOSPACE    1
#define KTP_ENOTBOUND   2
#define KTP_ENOMESSAGE  3

extern int ktp_errno;

typedef struct
{
    int allocated;
    pid_t owner;  
    int udp_fd;   

    struct sockaddr_in src_addr;  
    struct sockaddr_in dest_addr; 
    int bound;                    

    char send_buffer[KTP_BUFFER_SIZE][MESSAGE_SIZE];
    char recv_buffer[KTP_BUFFER_SIZE][MESSAGE_SIZE];
    int send_count;
    int recv_count; 
} ktp_socket_t;

int k_socket(int domain, int type, int protocol);
int k_bind(int ktp_fd, const struct sockaddr_in *src, const struct sockaddr_in *dest);
ssize_t k_sendto(int ktp_fd, const void *buf, size_t len, int flags, const struct sockaddr_in *dest, socklen_t addrlen);
ssize_t k_recvfrom(int ktp_fd, void *buf, size_t len, int flags, struct sockaddr_in *src, socklen_t *addrlen);
int k_close(int ktp_fd);

#endif // KSOCKET_H