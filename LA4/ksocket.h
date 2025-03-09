#ifndef KSOCKET_H
#define KSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
| SOCK_KTP         | 5555  | Socket type for KTP (KTP socket)                                      |
| BUFFER_SIZE      | 512   | Size of the buffer for sending and receiving messages                 |
| T                | 5     | Timeout value for retransmission                                      |
| p                | p     | Probability of dropping a message                                     |
| N                | 25    | Number of KTP sockets that can be created                             |
| P(s)             | -     | Decrements (locks) the semaphore 's' using the 'pop' structure        |
| V(s)             | -     | Increments (unlocks) the semaphore 's' using the 'vop' structure      |
| KEY_FILE         | -     | File used for generating keys for shared memory and semaphores        |
====================================================================================================

Shared Memory Keys:

(1) KEY_SHMID_SOCK_INFO : Key for shared memory segment holding socket info K_SOCKET).
(2) KEY_SHMID_SM        : Key for shared memory segment holding an array of SM_entry structs.
(3) KEY_SEM1            : Key for semaphore SEM1
(4) KEY_SEM2            : Key for semaphore SEM2
(5) KEY_SEM_SM          : Key for semaphore used for protecting access to the SM shared memory.
(6) KEY_SEM_SOCK_INFO   : Key for semaphore used for protecting access to the socket info memory.
*/

#define SOCK_KTP 5555
#define BUFFER_SIZE 512
#define SEND_BUFF_SIZE 10
#define RECV_BUFF_SIZE 10
#define p 0.05 
#define T 5
#define N 15

#define KEY_FILE "makefile"
#define KEY_SHMID_SOCK_INFO ftok(KEY_FILE, 'A')
#define KEY_SHMID_SM        ftok(KEY_FILE, 'B')
#define KEY_SEM1            ftok(KEY_FILE, 'C')
#define KEY_SEM2            ftok(KEY_FILE, 'D')
#define KEY_SEM_SM          ftok(KEY_FILE, 'E')
#define KEY_SEM_SOCK_INFO   ftok(KEY_FILE, 'F')
                   
#define P(s) semop(s, &pop, 1)  
#define V(s) semop(s, &vop, 1)
#define SPTR struct sockaddr *

/*

WINDOW : STRUCTURE FOR MANAGING SEND AND RECEIVE WINDOWS
----------------------------------------------------------------------------------------------------
| S.No | Field Name                       | Description                                            |
|------|----------------------------------|--------------------------------------------------------|
| 1    | wndw                             | Array of sequence numbers for the window               |
| 2    | size                             | Size of the window                                     |
| 3    | start_seq                        | Start sequence number of the window                    |
----------------------------------------------------------------------------------------------------
*/

struct window {
    int wndw[256];
    int size;
    int start_seq;              
};

/*
 SM_ENTRY: SHARED MEMORY STRUCTURE FOR KTP SOCKET MANAGEMENT
 ----------------------------------------------------------------------------------------------------
 | S.No | Field Name                       | Description                                            |
 |------|----------------------------------|--------------------------------------------------------|
 | 1    | is_free                          | Indicates if KTP socket is free (1) or allocated (0)   |
 | 2    | process_id                       | Process ID of the process that created the KTP socket  |
 | 3    | udp_FD                           | UDP socket FD mapped to this KTP socket                |
 | 4    | ip_address                       | IPv4 address of the remote endpoint                    |
 | 5    | port                             | Port number of the remote endpoint                     |
 | 6    | SEND_BUFFER                      | Circular send buffer (10 slots, each 512 bytes)        |
 | 7    | SEND_BUFFER_SIZE                 | Current number of messages in the send buffer          |
 | 8    | SEND_MSG_SIZES                   | Length of each message stored in the send buffer       |
 | 9    | RECV_BUFFER                      | Circular receive buffer (5 slots, each 512 bytes)      |
 | 10   | RECV_BUFFER_PTR                  | Index for the next message to read from recv buffer    |
 | 11   | RECV_BUFFER_ISVALID              | Array of valid flag of each message in recv buffer     |
 | 12   | RECV_MSG_SIZES                   | Length of each message stored in the receive buffer    |
 | 13   | swnd                             | Send window for managing packet transmission           |
 | 14   | rwnd                             | Receive window for managing incoming packets           |
 | 15   | nospace                          | Flag indicating if recv buffer is full (1) or not (0)  |
 | 16   | timer                            | Timestamp of last sent message for each sequence no    |
 ----------------------------------------------------------------------------------------------------
 */

struct SM_entry
{
    int is_free;
    int nospace;

    int udp_FD;
    pid_t process_id;
    char ip_address[16];
    uint16_t port;

    char SEND_BUFFER[SEND_BUFF_SIZE][BUFFER_SIZE];
    int SEND_BUFFER_SIZE;
    int SEND_MSG_SIZES[SEND_BUFF_SIZE];

    char RECV_BUFFER[RECV_BUFF_SIZE][BUFFER_SIZE];
    int RECV_BUFFER_ISVALID[RECV_BUFF_SIZE];
    int RECV_BUFFER_PTR;
    int RECV_MSG_SIZES[10];

    struct window swnd;
    struct window rwnd;
    time_t timer[256];
};

typedef struct K_SOCKET{
    int allocated;
    int sock_id;
    char ip_address[16];
    uint16_t port;
    int err_no;
} K_SOCKET;

/*
SEGMENT FORMAT :
Each segment has a 18 bit header.

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
    uint16_t rwnd;
    uint16_t len;
    char data[BUFFER_SIZE];
} Segment;

/*
================================================================================================
| Shared Memory/Resource       | Purpose                                                       |
================================================================================================
| SM                           | Shared memory segment holding an array of SM_entry structs.   |
| sock_info                    | Shared memory segment holding socket info (K_SOCKET).        |
| shmid_sock_info              | ID of the shared memory segment holding socket info.          | 
| shmid_SM                     | Shared memory segment holding an array of SM_entry structs    |
| sem1, sem2                   | Semaphores used for synchronizing processes (signals).        |
| SM_sem                       | Semaphore for protecting access to the SM shared memory.      |
| sock_info_sem                | Semaphore for protecting access to the socket info memory.    |
------------------------------------------------------------------------------------------------

-- SM                  : stores session management data, with each SM_entry holding connection state info.
-- sock_info           : stores socket info like socket ID, IP address, port, and error state.
-- shmid_SM            : stores session management data, with each SM_entry holding connection state info.
-- SM_sem              : ensures that only one process modifies the SM segment at a time.
-- sock_info_sem       : ensures that socket metadata is not concurrently modified by multiple processes.
*/
extern struct SM_entry *SM;
extern K_SOCKET* sock_info;
extern int sem1, sem2;
extern int sock_info_sem, SM_sem;
extern int shmid_sock_info, shmid_SM;
extern struct sembuf pop, vop;

void encode(struct Segment *seg, char *result);
void decode(const char *str, struct Segment *seg);

int k_socket(int domain, int type, int protocol);
int k_bind(char src_ip[], uint16_t src_port, char dst_ip[], uint16_t dst_port);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const SPTR dst_addr, socklen_t addrlen);
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, SPTR src_addr, socklen_t *addrlen);
int k_close(int sockfd);

int dropMessage();
int IP_check(char *ip);
int PORT_check(char *port);
void argcheck(int argc, char *argv[]);

#endif  
// KSOCKET_H
