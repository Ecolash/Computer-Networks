#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5000
#define DROP_PROBABILITY 0.3
#define MAXLINE 1024
#define TIMEOUT 2

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
    struct sockaddr_in servaddr;
    socklen_t len = sizeof(servaddr);

    Frame frame_send, frame_recv;
    int seq_num = 0;
    srand(time(0));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    for (int i = 0; i < 5; i++)
    {
        // Prepare the frame to send
        frame_send.type = SEQ; // SEQ frame
        frame_send.sq_no = seq_num;
        snprintf(frame_send.data, sizeof(frame_send.data), "Packet %d", i + 1);

        if (drop())
        {
            printf("[-] Client dropped packet %d\n", seq_num);
        }
        else
        {
            sendto(sockfd, &frame_send, sizeof(Frame), 0, (struct sockaddr *)&servaddr, len);
            printf("[+] Sent packet: %s | Seq: %d\n", frame_send.data, frame_send.sq_no);
        }

        // Wait for ACK using simple loop with timeout
        int ack_received = 0;
        time_t start_time = time(NULL);

        while (difftime(time(NULL), start_time) < TIMEOUT)
        {
            int n = recvfrom(sockfd, &frame_recv, sizeof(Frame), MSG_DONTWAIT, (struct sockaddr *)&servaddr, &len);
            if (n > 0 && frame_recv.type == ACK && frame_recv.ack == seq_num)
            {
                printf("[+] Received ACK for sequence number %d\n", seq_num);
                ack_received = 1;
                seq_num = 1 - seq_num; // Toggle sequence number
                break;
            }
        }

        if (!ack_received)
        {
            printf("[-] No ACK or timeout, resending packet %d\n", seq_num);
            i--; // Resend the same packet
        }

        sleep(1); // Simulate delay
    }

    close(sockfd);
}