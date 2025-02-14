#ifndef KSOCKET_H
#define KSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <errno.h>


/*
====================================================================================================
MACRO DEFINITIONS
====================================================================================================
| Macro Name       | Value | Description                                                           |
----------------------------------------------------------------------------------------------------
| SOCK_KTP         | 3     | Socket type for MTP (MTP socket)                                      |
| BUFFER_SIZE      | 512   | Size of the buffer for sending and receiving messages                 |
| T                | 5     | Timeout value for retransmission                                      |
| p                | p     | Probability of dropping a message                                     |
| N                | 25    | Number of MTP sockets that can be created                             |
====================================================================================================

*/

#define SOCK_KTP 10087
#define BUFFER_SIZE 512
#define __MAX_RETRY__ 10
#define p 0.05
#define T 1
#define N 25

/*
SEGMENT FORMAT :
Each segment has a 20 bit header.

(a) First bit for type (0 = ACK, 1 = DATA)
(b) Next 8 bit for sequence number (0 - 255)
(c) Next 9 bits are rwnd size (for ACK) or data length(for DATA)
(d) Next 512 bytes of data (for DATA)

-----------------------------------------------------------------
| Index  | Description                                          |
|--------|------------------------------------------------------|
| 0      | Type bit (0 = ACK, 1 = DATA)                         |
| 1 - 8  | Sequence number in 8-bit binary                      |
| 9 - 17 | rwnd size in 9-bit binary (for ACK)                  |
| 9 - 17 | Message length in 9-bit binary (for DATA)            |
| 18 - x | Message data (Max 512 bytes)                         |
=================================================================
SIZE = 1 + 8 + 9 + 512 = 530 bytes (string encoding)
-----------------------------------------------------------------
*/
typedef struct Segment
{
    uint8_t type;
    uint8_t seq_num;
    uint16_t len;
    char data[512];
} Segment;

void encode(struct Segment *seg, char *result);
void decode(const char *str, struct Segment *seg);

int k_socket(int domain, int type, int protocol);
int k_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dst_addr, socklen_t addrlen);
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int sockfd);

int dropMessage();
int IP_check(char *ip);
int PORT_check(char *port);
void argcheck(int argc, char *argv[]);

#endif  // KSOCKET_H
