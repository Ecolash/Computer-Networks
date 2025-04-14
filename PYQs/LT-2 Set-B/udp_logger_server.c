#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define PORT 9090
#define BUFFER_SIZE 1024
#define LOG_FILE "udp_server.log"

/* Compilation and testing: 12 marks (deduct this complete marks if the code does not compile, if the code compile then give marks based on the following test cases):

NB: The compilation should be done using a makefile. If the makefile is not available, then deduct 6 marks (50% or the marks for each of the following test cases)


1) Run a single server and single client -- output is as expected: 2 marks

2) Run a single server and two clients -- start the first client and send a message, then start the second client and send message from both the clients -- output is as expected: 4 marks

3) Run a single server and three clients -- start the first client and send a message, then start the second client and send a message, then close the first client, finally start the third client and send messages from the second and the third clients -- output is as expected: 4 marks

4) The log file is correctly generated for all the above three cases: 2 marks
*/



/* 4 marks 
   2 for setsockopt, 
   2 for getsockopt, 
For setsockopt and getsockopt -- do not give marks if the format is incorrect 
*/

void set_udp_socket_options(int sockfd) {
    int snd_buf_size = 4096;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &snd_buf_size, sizeof(snd_buf_size)) < 0) {
        perror("setsockopt(SO_SNDBUF) failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(snd_buf_size);
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &snd_buf_size, &len) < 0) {
        perror("getsockopt(SO_SNDBUF) failed");
        exit(EXIT_FAILURE);
    }

    printf("Send buffer size: %d bytes\n", snd_buf_size);
}

/* 4 marks. Check that logging format is as asked in the QP. If logging has been done in a different format, deduct 2 marks */

void log_message(const char *client_ip, int client_port, const char *msg) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "[%s] From %s:%d - %s\n", timestamp, client_ip, client_port, msg);
        fclose(log_file);
    }

    printf("[%s] From %s:%d - %s\n", timestamp, client_ip, client_port, msg);
}


/*
Total: 8 marks

socket call: 1 mark
setting address parameters in server_addr: 2 marks (deduct one mark if htons is not used)
bind: 2 marks
fcntl: 3 marks (deduct 2 marks if F_GETFL has not been used)
*/


int create_udp_server_socket(int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    set_udp_socket_options(sockfd);

    // Set to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}


/*
Total: 16 marks

setting the timeout: 2 marks
select call: 2 marks
FD_SET and FD_ISSET: 2 marks
recv call: 1 mark
Extracting client address from recv: 4 marks
sendto: 2 marks
Check for EWOULDBLOCK: 1 mark
Check for other errors (select returns <0, recvfrom retiurns <0): 2 marks
*/

int main() {
    int sockfd = create_udp_server_socket(PORT);
    printf("UDP Logger Server running on port %d...\n", PORT);

    fd_set readfds;
    struct timeval timeout;
    char buffer[BUFFER_SIZE];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (ready < 0) {
            perror("select() failed");
            break;
        } else if (ready == 0) {
            // Timeout - nothing to do
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                              (struct sockaddr*)&client_addr, &addr_len);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                char *client_ip = inet_ntoa(client_addr.sin_addr);
                int client_port = ntohs(client_addr.sin_port);

                log_message(client_ip, client_port, buffer);

                char ack_message[BUFFER_SIZE];
                snprintf(ack_message, sizeof(ack_message), "ACKED %s", buffer);

                sendto(sockfd, ack_message, strlen(ack_message), 0,
                       (struct sockaddr*)&client_addr, addr_len);
            } else if (bytes_received < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("recvfrom() failed");
            }
        }
    }

    close(sockfd);
    return 0;
}

