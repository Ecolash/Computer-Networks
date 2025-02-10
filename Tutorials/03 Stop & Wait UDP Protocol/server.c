#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5000
#define DROP_PROBABILITY 0.3
#define MAXLINE 1024

#define ACK 0
#define SEQ 1
#define FIN 2

typedef struct frame
{
    int type; // ACK:0, SEQ:1 FIN:2
    int sq_no;
    int ack;
    char data[1024];
} Frame;

int drop() { return ((double)rand() / RAND_MAX) < DROP_PROBABILITY; }

int main()
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);
    int expected_seq = 0;

    Frame frame_recv, frame_send;
    srand(time(0));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("[+] Server is running...\n");

    while (1)
    {
        int n = recvfrom(sockfd, &frame_recv, sizeof(Frame), 0, (struct sockaddr *)&cliaddr, &len);

        if (drop())
        {
            printf("[-] Dropped packet with sequence number %d\n", frame_recv.sq_no);
            continue;
        }

        if (n > 0 && frame_recv.type == SEQ && frame_recv.sq_no == expected_seq)
        {
            printf("[+] Received packet: %s | Seq: %d\n", frame_recv.data, frame_recv.sq_no);

            expected_seq = 1 - expected_seq;
            frame_send.type = ACK;
            frame_send.ack = frame_recv.sq_no;

            sendto(sockfd, &frame_send, sizeof(Frame), 0, (struct sockaddr *)&cliaddr, len);
            printf("[+] ACK sent for sequence number %d\n", frame_recv.sq_no);
        }
        else
        {
            printf("[-] Duplicate or out-of-order packet ignored\n");
        }
    }

    close(sockfd);
}
