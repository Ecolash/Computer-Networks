#include <assert.h>
#include <pthread.h>
#include <semaphore.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include "ksocket.h"

pthread_t S_THREAD__;
pthread_t R_THREAD__;
pthread_t G_THREAD__;

int transmissionCnt = 0;

// -- For DEBUGGING purposes

void print_sock_info()
{
    printf("Socket ID: %d\n", sock_info->sock_id);
    printf("Allocated: %d\n", sock_info->allocated);
    printf("Error Number: %d\n", sock_info->err_no);
    printf("IP Address: %s\n", sock_info->ip_address);
    printf("Port: %d\n", sock_info->port);
}

void print_segment(struct Segment *seg)
{
    printf("T: %s\n", seg->type ? "DATA" : "ACK");
    printf("SEQ: %d\n", seg->seq_num);
    printf("RWND: %d\n", seg->rwnd);
    printf("LEN: %d\n", seg->len);
    printf("DATA: %s\n", seg->data);
}

// -- SEMAPHORE OPERATIONS

void release_mem(int sig)
{
    int c1 = pthread_cancel(S_THREAD__);
    int c2 = pthread_cancel(R_THREAD__);
    int c3 = pthread_cancel(G_THREAD__);
    if (c1 + c2 + c3 < 0) perror("[-] Error canceling threads");

    int d1 = shmdt(sock_info);
    int d2 = shmdt(SM);
    if (d1 + d2 < 0) perror("[-] Error detaching shared memory segments");

    int m1 = shmctl(shmid_SM, IPC_RMID, NULL);
    int m2 = shmctl(shmid_sock_info, IPC_RMID, NULL);
    if (m1 + m2 < 0) perror("[-] Error removing shared memory segments");

    int k1 = semctl(sem1, 0, IPC_RMID);
    int k2 = semctl(sem2, 0, IPC_RMID);
    int k3 = semctl(SM_sem, 0, IPC_RMID);
    int k4 = semctl(sock_info_sem, 0, IPC_RMID);
    if (k1 + k2 + k3 + k4 < 0) perror("[-] Error removing semaphores");

    printf("\n[!] Number of transmissions: %d\n", transmissionCnt);
    printf("[+] Memory released successfully!\n");
    printf("[+] Exiting...\n");
    sleep(30);
    exit(0);
}

void *S() {
while (1) {
    sleep(T / 2);
    P(SM_sem);

    for (int i = 0; i < N; i++) {
        if (SM[i].is_free == 1) continue; 
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SM[i].port);
        serv_addr.sin_addr.s_addr = inet_addr(SM[i].ip_address);

        int timeout = 0;
        int start = SM[i].swnd.start_seq;
        int end   = (start + SM[i].swnd.size) % 256;
        for (int j = start; j != end; j = (j + 1) % 256) {
            if (SM[i].timer[j] != -1) 
            {
                time_t T1 = time(NULL);
                time_t T2 = SM[i].timer[j];
                double delT = difftime(T1, T2);
                if (delT > T) timeout = 1;
            }
            if (timeout) break;
        }
       
        if (timeout) {
            printf("[!] TIMEOUT | RESENDING DATA\n");
            for (int j = start; j != end; j = (j + 1) % 256) {
                if (SM[i].swnd.wndw[j] != -1) {
                    int len = SM[i].SEND_MSG_SIZES[SM[i].swnd.wndw[j]];
                    char *msg = SM[i].SEND_BUFFER[SM[i].swnd.wndw[j]];

                    Segment _DATA_SEG_;
                    _DATA_SEG_.type = 1;
                    _DATA_SEG_.seq_num = j;
                    _DATA_SEG_.len = len;
                    memcpy(_DATA_SEG_.data, msg, len);

                    char segment[530];
                    bzero(segment, 530);
                    encode(&_DATA_SEG_, segment);
                    // print_segment(&_DATA_SEG_);

                    sendto(SM[i].udp_FD, segment, 530, 0, (SPTR)&serv_addr, sizeof(serv_addr));
                    printf("[>] SENT [ DATA | SEQ = %-3d | LEN = %-4d]\n", j, len);
                    transmissionCnt++;
                    SM[i].timer[j] = time(NULL);
                }
            }
        } else {
            int start = SM[i].swnd.start_seq;
            int end = (start + SM[i].swnd.size) % 256;
            for (int j = start; j != end; j = (j + 1) % 256) {
                if (SM[i].swnd.wndw[j] != -1 && SM[i].timer[j] == -1) 
                {
                    int len = SM[i].SEND_MSG_SIZES[SM[i].swnd.wndw[j]];
                    char *msg = SM[i].SEND_BUFFER[SM[i].swnd.wndw[j]];
                    
                    Segment _DATA_SEG_;
                    _DATA_SEG_.type = 1;
                    _DATA_SEG_.seq_num = j;
                    _DATA_SEG_.len = len;
                    memcpy(_DATA_SEG_.data, msg, len);

                    char segment[530];
                    bzero(segment, 530);
                    encode(&_DATA_SEG_, segment);
                    // print_segment(&_DATA_SEG_);

                    sendto(SM[i].udp_FD, segment, 530, 0, (SPTR)&serv_addr, sizeof(serv_addr));
                    printf("[>] SENT [ DATA | SEQ = %-3d | LEN = %-4d]\n", j, len);
                    transmissionCnt++;
                    SM[i].timer[j] = time(NULL);
                }
            }
        }
    }
    V(SM_sem);
}
}

void *R() {
    fd_set READ_FDs;
    int maxfd = -1;
    FD_ZERO(&READ_FDs);

    while (1) {
        fd_set PREV_FDs = READ_FDs;
        struct timeval _TSEL_ = {T, 0};
        int activity = select(maxfd + 1, &PREV_FDs, NULL, NULL, &_TSEL_);
        if (activity < 0) perror("[-] select() failed!");

        /*
        -------------------------------------------------------------
        TIMEOUT HANDLING PROCEDURE:
        -------------------------------------------------------------
        (1) Check for timeout in the send window.
        (2) If timeout, resend the message.
        (3) If no timeout, send the next message in the send window.
        -------------------------------------------------------------
        */
        if (activity == 0) { // -- TIMEOUT in select()
            FD_ZERO(&READ_FDs);
            maxfd = 0;
            P(SM_sem);
            for (int i = 0; i < N; i++) {
                if (SM[i].is_free == 1) continue;
                FD_SET(SM[i].udp_FD, &READ_FDs);

                maxfd = (maxfd > SM[i].udp_FD) ? maxfd : SM[i].udp_FD;
                if (SM[i].rwnd.size > 0) SM[i].nospace = 0;

                int currseq = SM[i].rwnd.start_seq;
                int lastseq = ((currseq + 256) - 1) % 256;  // Last in-order sequence number

                struct sockaddr_in cliaddr;
                cliaddr.sin_addr.s_addr = inet_addr(SM[i].ip_address);
                cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(SM[i].port);

                Segment _ACK_SEG_;
                _ACK_SEG_.type = 0;
                _ACK_SEG_.seq_num = lastseq;
                _ACK_SEG_.rwnd = SM[i].rwnd.size;

                char segment[530];
                encode(&_ACK_SEG_, segment);
                sendto(SM[i].udp_FD, segment, 530, 0, (SPTR)&cliaddr, sizeof(cliaddr));
                printf("[>] SENT [ ACK  | SEQ = %-3d | RWND = %-3d]\n", lastseq, SM[i].rwnd.size);
            }
            V(SM_sem);

        } else {
            P(SM_sem);
            for (int i = 0; i < N; i++) {
                if (FD_ISSET(SM[i].udp_FD, &PREV_FDs)) 
                {
                    // -- READ FROM SOCKET SM[i].udp_FD 

                    char buffer[530];
                    struct sockaddr_in cliaddr;
                    unsigned int len = sizeof(cliaddr);
                    int n = recvfrom(SM[i].udp_FD, buffer, 530, 0, (SPTR)&cliaddr, &len);

                    if (dropMessage()) continue;  
                    if (n < 0) { perror("recvfrom()"); continue; }
                    
                    Segment segment;
                    decode(buffer, &segment);
                    int seq = segment.seq_num;
                    int type = segment.type;

                    switch (type) {
                        case 0:  // SEGMENT TYPE: ACK
                            int rwnd = segment.rwnd;
                            printf("[<] RECV [ ACK  | SEQ = %-3d | RWND = %-3d]\n", seq, rwnd);

                            if (SM[i].swnd.wndw[seq] >= 0) {
                                int j = SM[i].swnd.start_seq;
                                while (j != (seq + 1) % 256) {
                                    SM[i].swnd.wndw[j] = -1;
                                    SM[i].timer[j] = -1;
                                    SM[i].SEND_BUFFER_SIZE++;
                                    j = (j + 1) % 256;
                                }
                                SM[i].swnd.start_seq = (seq + 1) % 256;
                            }
                            SM[i].swnd.size = rwnd;
                            break;
                        
                        case 1: // SEGMENT TYPE: DATA
                            int len = segment.len;
                            char data[512];
                            memcpy(data, segment.data, len);
                            printf("[<] RECV [ DATA | SEQ = %-3d | LEN = %-4d]\n", seq, len);

                            if (seq == SM[i].rwnd.start_seq) {
                                // In order message
                                int buff_ind = SM[i].rwnd.wndw[seq];
                                memcpy(SM[i].RECV_BUFFER[buff_ind], data, len);
                                SM[i].RECV_BUFFER_ISVALID[buff_ind] = 1;
                                SM[i].rwnd.size--;
                                SM[i].RECV_MSG_SIZES[SM[i].rwnd.wndw[seq]] = len;

                                // -- FIND NEXT IN-ORDER MESSAGE

                                int curr = SM[i].rwnd.start_seq;
                                int buff_idx = SM[i].rwnd.wndw[curr];
                                int valid = SM[i].RECV_BUFFER_ISVALID[buff_idx];

                                while (buff_idx >= 0 && valid == 1) {
                                    curr = (curr + 1) % 256;
                                    buff_idx = SM[i].rwnd.wndw[curr];
                                    valid = SM[i].RECV_BUFFER_ISVALID[buff_idx];
                                }

                                SM[i].rwnd.start_seq = curr;

                            } else {
                                int buff_ind = SM[i].rwnd.wndw[seq];
                                int is_valid = SM[i].RECV_BUFFER_ISVALID[buff_ind];
                                if (buff_ind >= 0 && is_valid == 0) {
                                    memcpy(SM[i].RECV_BUFFER[buff_ind], data, len);
                                    SM[i].RECV_BUFFER_ISVALID[buff_ind] = 1;
                                    SM[i].rwnd.size--;
                                    SM[i].RECV_MSG_SIZES[buff_ind] = len;
                                }
                                else { printf("[-] DUPLICATE MESSAGE \n"); }
                            }
                            if (SM[i].rwnd.size == 0) SM[i].nospace = 1;   // Nospace in the receive window
                            seq = (SM[i].rwnd.start_seq + 256 - 1) % 256;  // Last in-order message received

                            Segment _ACK_SEG_;
                            _ACK_SEG_.type = 0;
                            _ACK_SEG_.seq_num = seq;
                            _ACK_SEG_.rwnd = SM[i].rwnd.size;

                            char ack[530];
                            bzero(ack, 530);
                            encode(&_ACK_SEG_, ack);
                            sendto(SM[i].udp_FD, ack, 530, 0, (SPTR)&cliaddr, sizeof(cliaddr));
                            printf("[>] SENT [ ACK  | SEQ = %-3d | RWND = %-3d]\n", seq, SM[i].rwnd.size);
                            break;

                        default: // SEGMENT TYPE: UNKNOWN 
                            printf("[-] Unknown segment type received\n");
                            break;
                    }
                }
            }
            V(SM_sem);
        }
    }
    perror("[-] Error in R thread");
    pthread_exit(NULL);
}
/*
================================================================================
G THREAD : GARBAGE COLLECTOR
--------------------------------------------------------------------------------
(1) Runs indefinitely
(2) Periodically checks process status in the SM table after interval T.
(3) Iterates through SM table: Marks entry as free if process is not running.
*/

void *G() {
    while (1) {
        sleep(T);
        P(SM_sem);
        for (int i = 0; i < N; i++) 
        {
            struct SM_entry curr = SM[i];
            pid_t pid = curr.process_id;
            if (curr.is_free == 1) continue;                  
            if (kill(pid, 0) == 0) continue; 
            SM[i].is_free = 1;                             
        }
        V(SM_sem);
    }
}

int main() {
    srand(time(NULL));
    transmissionCnt = 0;
    signal(SIGINT, release_mem);

    /*
    3 threads to do the following tasks:
    - S_THREAD__ -> Thread handling sending of messages
    - R_THREAD__ -> Thread handling receiving of messages
    - G_THREAD__ -> Thread handling garbage collection of processes
    */ 

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    key_t K1 = KEY_SHMID_SOCK_INFO;
    key_t K2 = KEY_SHMID_SM;
    key_t K3 = KEY_SEM1;
    key_t K4 = KEY_SEM2;
    key_t K5 = KEY_SEM_SM;
    key_t K6 = KEY_SEM_SOCK_INFO;

    shmid_sock_info = shmget(K1, sizeof(K_SOCKET), 0666 | IPC_CREAT);
    shmid_SM = shmget(K2, sizeof(struct SM_entry) * N, 0666 | IPC_CREAT);
    sem1 = semget(K3, 1, 0666 | IPC_CREAT);
    sem2 = semget(K4, 1, 0666 | IPC_CREAT);
    SM_sem = semget(K5, 1, 0666 | IPC_CREAT);
    sock_info_sem = semget(K6, 1, 0666 | IPC_CREAT);

    sock_info = (K_SOCKET *)shmat(shmid_sock_info, 0, 0);
    SM = (struct SM_entry *)shmat(shmid_SM, 0, 0);
    
    // INITIAL CONFIGURATION
    for (int i = 0; i < N; i++) SM[i].is_free = 1;
    sock_info->allocated = 0;

    /*
    ============================================================================================================
    SEMAPHORE INITIALIZATION:
    ------------------------------------------------------------------------------------------------------------
    sem1 = 0 => Lock the process until the socket is allocated.
    sem2 = 0 => Lock the process until the socket is binded.
    SM_sem = 1 => Allow only one process to access the shared memory at a time.
    sock_info_sem = 1 => Allow only one process to access the socket info at a time.
    ============================================================================================================
    */
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    semctl(SM_sem, 0, SETVAL, 1);
    semctl(sock_info_sem, 0, SETVAL, 1);

    int pt1 = pthread_create(&S_THREAD__, &attr, S, NULL);
    int pt2 = pthread_create(&R_THREAD__, &attr, R, NULL);
    int pt3 = pthread_create(&G_THREAD__, &attr, G, NULL);
    if (pt1 || pt2 || pt3) { perror("[-] pthread_create() failed!"); exit(1); }

    while (1) {
        P(sem1);
        P(sock_info_sem);

        if (sock_info->allocated == 0) {
            int sock_id = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock_id == -1) 
            {
                printf("[-] Error in creating UDP socket\n");
                sock_info->sock_id = -1;
                sock_info->allocated = 0;
                sock_info->err_no = errno;
            } else 
            {
                printf("[+] Socket created successfully\n");
                sock_info->sock_id = sock_id;
                sock_info->allocated = 1;
            }
        } else {
            struct sockaddr_in serv_addr;
            serv_addr.sin_addr.s_addr = inet_addr(sock_info->ip_address);
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(sock_info->port);
            
            int binded = bind(sock_info->sock_id, (SPTR)&serv_addr, sizeof(serv_addr));
            if (binded == -1) 
            {
                printf("[-] Error in binding socket\n");
                close(sock_info->sock_id);
                sock_info->sock_id = -1;
                sock_info->allocated = 0;
                sock_info->err_no = errno;
            } else 
            {
                printf("[+] Socket binded successfully\n");
                sock_info->allocated = 2;
            }
        }
        V(sock_info_sem);
        V(sem2);
    }
}