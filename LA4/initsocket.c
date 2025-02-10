// File: initsocket.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include "ksocket.h"

/*
  We assume that the SM array is defined (as static) in ksocket.c.
  For our purposes we declare it as extern here.
*/
extern ktp_socket_t SM[MAX_KTP_SOCKETS];

/*
  Global mutex for accessing SM. (A production implementation might use
  per–socket mutexes rather than one global one.)
*/
pthread_mutex_t sm_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
  Thread R: This thread continuously monitors all UDP file descriptors (one per
  allocated KTP socket in SM) for incoming messages. It uses select() with a timeout
  so that it can periodically:
    - Add newly created KTP sockets to its read set.
    - Check if a previously “nospace” receive–buffer now has space available.
  When a message is received it checks its type:
    - For a DATA message, if space exists the message is stored in the KTP socket’s
      receiver–side buffer (SM.recv_buffer) and an ACK is sent. If the buffer is full,
      the socket’s “nospace” flag is set.
    - For an ACK message, the sender–side window (swnd) is updated and the acknowledged
      message is removed from the send_buffer.
    - For a duplicate ACK, the sender window size is simply updated.
*/
void *thread_R_func(void *arg)
{
    fd_set readfds;
    int max_fd;
    struct timeval timeout;
    int i;

    while (1)
    {
        FD_ZERO(&readfds);
        max_fd = 0;

        /* Build the set of UDP file descriptors from the current SM */
        pthread_mutex_lock(&sm_mutex);
        for (i = 0; i < MAX_KTP_SOCKETS; i++)
        {
            if (SM[i].allocated)
            {
                FD_SET(SM[i].udp_fd, &readfds);
                if (SM[i].udp_fd > max_fd)
                    max_fd = SM[i].udp_fd;
            }
        }
        pthread_mutex_unlock(&sm_mutex);

        /* Wait with a timeout so we can periodically check the SM */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int ret = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0)
        {
            perror("select");
            continue;
        }
        else if (ret == 0)
        {
            /* Timeout: check for sockets that were previously full but now have space. */
            pthread_mutex_lock(&sm_mutex);
            for (i = 0; i < MAX_KTP_SOCKETS; i++)
            {
                if (SM[i].allocated && SM[i].bound)
                {
                    if (SM[i].nospace && SM[i].recv_count < KTP_BUFFER_SIZE)
                    {
                        /* Build duplicate ACK with the last acknowledged seq and updated rwnd */
                        ktp_packet_t dup_ack;
                        memset(&dup_ack, 0, sizeof(dup_ack));
                        dup_ack.type = KTP_MSG_DUP_ACK;
                        dup_ack.ack = SM[i].last_ack; // last ACKed seq number
                        dup_ack.rwnd = (uint16_t)(KTP_BUFFER_SIZE - SM[i].recv_count);
                        if (sendto(SM[i].udp_fd, &dup_ack, sizeof(dup_ack), 0,
                                   (struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr)) < 0)
                        {
                            perror("sendto (dup_ack)");
                        }
                        SM[i].nospace = 0; // reset the flag after sending duplicate ACK
                    }
                }
            }
            pthread_mutex_unlock(&sm_mutex);
            continue;
        }

        /* Some UDP fds are ready: process each. */
        pthread_mutex_lock(&sm_mutex);
        for (i = 0; i < MAX_KTP_SOCKETS; i++)
        {
            if (SM[i].allocated && FD_ISSET(SM[i].udp_fd, &readfds))
            {
                ktp_packet_t pkt;
                struct sockaddr_in src;
                socklen_t addrlen = sizeof(src);
                ssize_t n = recvfrom(SM[i].udp_fd, &pkt, sizeof(pkt), 0,
                                     (struct sockaddr *)&src, &addrlen);
                if (n < 0)
                {
                    perror("recvfrom");
                    continue;
                }

                /* Process the packet based on its type */
                if (pkt.type == KTP_MSG_DATA)
                {
                    /* DATA message: store it if there is room; else mark nospace */
                    if (SM[i].recv_count < KTP_BUFFER_SIZE)
                    {
                        memcpy(SM[i].recv_buffer[SM[i].recv_count], &pkt, MESSAGE_SIZE);
                        SM[i].recv_count++;
                    }
                    else
                    {
                        SM[i].nospace = 1;
                    }
                    /* Send back an ACK for the received message */
                    {
                        ktp_packet_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.type = KTP_MSG_ACK;
                        ack.ack = pkt.seq; // acknowledge this sequence number
                        ack.rwnd = (uint16_t)(KTP_BUFFER_SIZE - SM[i].recv_count);
                        if (sendto(SM[i].udp_fd, &ack, sizeof(ack), 0,
                                   (struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr)) < 0)
                        {
                            perror("sendto (ACK)");
                        }
                        SM[i].last_ack = pkt.seq;
                    }
                }
                else if (pkt.type == KTP_MSG_ACK)
                {
                    /* ACK message: update sender window and remove the acknowledged message.
                       (For simplicity we assume that the first message in send_buffer corresponds
                       to the ACK; in a full implementation you’d search by sequence number.) */
                    if (SM[i].send_count > 0)
                    {
                        int j;
                        for (j = 1; j < SM[i].send_count; j++)
                        {
                            memcpy(SM[i].send_buffer[j - 1], SM[i].send_buffer[j], MESSAGE_SIZE);
                            SM[i].send_timestamps[j - 1] = SM[i].send_timestamps[j];
                        }
                        SM[i].send_count--;
                    }
                    /* Also update swnd */
                    SM[i].swnd = pkt.rwnd;
                }
                else if (pkt.type == KTP_MSG_DUP_ACK)
                {
                    /* Duplicate ACK: simply update swnd */
                    SM[i].swnd = pkt.rwnd;
                }
            }
        }
        pthread_mutex_unlock(&sm_mutex);
    }
    return NULL;
}

/*
  Thread S: This thread periodically (every T/2 seconds, here T is taken as 2 seconds)
  checks every allocated KTP socket for:
    - Timed–out messages in the sender buffer. (For every message whose send timestamp is older
      than T seconds, the message is retransmitted and its timestamp is updated.)
    - Whether there is a pending message that can now be sent (i.e. if the current sender window
      swnd allows it). If so, it sends that message, adds it to the send_buffer, and records the
      current time.
*/
void *thread_S_func(void *arg)
{
    const int T = 2; // Timeout period in seconds
    int i;
    while (1)
    {
        /* Sleep for T/2 seconds */
        usleep((T * 1000000) / 2);

        pthread_mutex_lock(&sm_mutex);
        for (i = 0; i < MAX_KTP_SOCKETS; i++)
        {
            if (SM[i].allocated && SM[i].bound)
            {
                time_t now = time(NULL);
                int j;
                /* Check for timed–out messages in the sender buffer */
                for (j = 0; j < SM[i].send_count; j++)
                {
                    if ((now - SM[i].send_timestamps[j]) >= T)
                    {
                        if (sendto(SM[i].udp_fd, SM[i].send_buffer[j], MESSAGE_SIZE, 0,
                                   (struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr)) < 0)
                        {
                            perror("sendto (retransmit)");
                        }
                        else
                        {
                            SM[i].send_timestamps[j] = now; // update timestamp
                        }
                    }
                }
                /* If there is a pending message and the sender window allows sending a new message */
                if (SM[i].pending && (SM[i].send_count < SM[i].swnd))
                {
                    if (sendto(SM[i].udp_fd, SM[i].pending_send, MESSAGE_SIZE, 0,
                               (struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr)) < 0)
                    {
                        perror("sendto (new message)");
                    }
                    else
                    {
                        /* Add the message to send_buffer and record its timestamp */
                        memcpy(SM[i].send_buffer[SM[i].send_count], SM[i].pending_send, MESSAGE_SIZE);
                        SM[i].send_timestamps[SM[i].send_count] = now;
                        SM[i].send_count++;
                        SM[i].pending = 0; // clear the pending flag
                    }
                }
            }
        }
        pthread_mutex_unlock(&sm_mutex);
    }
    return NULL;
}

/*
  The main() function starts both threads and then simply waits.
*/
int main(void)
{
    pthread_t tid_R, tid_S;

    if (pthread_create(&tid_R, NULL, thread_R_func, NULL) != 0)
    {
        perror("pthread_create thread_R");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&tid_S, NULL, thread_S_func, NULL) != 0)
    {
        perror("pthread_create thread_S");
        exit(EXIT_FAILURE);
    }
    /* In a long–running process, you might do more work here.
       For now, we simply wait for the threads to finish (which they never do). */
    pthread_join(tid_R, NULL);
    pthread_join(tid_S, NULL);

    return 0;
}
