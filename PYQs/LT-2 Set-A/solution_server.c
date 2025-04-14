#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 9090
#define BUFFER_SIZE 1024

/* Compile and Run: 12 marks (do not give this mark if the code does not compile. If the code compiles that check the following test cases and give marks accrodingly. 

NB: The compilation should be done using a makefile. If the makefile is not available, then deduct 6 marks (50% or the marks for each of the following test cases)

1) Start the server and the client. Send a message from the client. Close the client through CTRL+C -- output is as expected: 2 marks
2) Start the server. Start one client and send a message. Start a second client. Send a message from both the clients. Close thes second client -- output as expected: 4 marks
3) Start the server. Start two clients and send messages from them in any order. Close the second client. Start a third client and send message from the third and the first clients. Close the first client through CTRL+C. Send a message from the first client: 6 marks

In the above test cases, if the message format are not exactly what has been asked, or some messages are missing, deduct 50% marks
*/


/*
Total: 8 marks

setsockopt for SO_REUSEADDR: 2 marks
setsockopt for SO_RCVBUFL: 2 marks
getsockopt : 2 marks
Check for errors: 2 marks
Don't give marks if the format is not correct
*/

void set_socket_options(int sockfd) {
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    int recv_buf_size = 8192;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size)) < 0) {
        perror("setsockopt(SO_RCVBUF) failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len = sizeof(recv_buf_size);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, &len) < 0) {
        perror("getsockopt(SO_RCVBUF) failed");
        exit(EXIT_FAILURE);
    }

    printf("Receive buffer size: %d bytes\n", recv_buf_size);
}

/*
Total: 8 marks

Socket call: 1 mark
fcntl: 2 marks (deduct one mark if F_GETFL not used)
Setting the address: 3 marks (deduct 1 mark if htons is not used)
Bind: 1 mark
Listen: 1 mark
(don't deduct marks if error checking is not done)
*/


int create_server_socket(int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    set_socket_options(sockfd);

    // Set to non-blocking
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

    if (listen(sockfd, 10) < 0) {
        perror("listen() failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

/*
Total: 8 marks

Extract client IP and port: 4 marks (deduct two marks if inet_ntoa or ntohs has not been used)
recv call: 1 mark
Check for message format: 1 mark
Check for normal disconnection (bytes_received = 0): 1 mark
Check for abrupt disconnection: 1 mark
*/

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len);

    char *client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);

    while (1) {
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Received from %s:%d - %s\n", client_ip, client_port, buffer);
            send(client_fd, buffer, bytes_received, 0);
        } else if (bytes_received == 0) {
            printf("Client from %s:%d has disconnected normally.\n", client_ip, client_port);
            break;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            } else {
                printf("Client from %s:%d has disconnected abruptly.\n", client_ip, client_port);
                break;
            }
        }
    }

    close(client_fd);
    exit(0); // Exit child process
}

/*
Total: 6 marks

Accept connection: 1 mark
Check for EWOULDBLOCK and EAGAIN: 1 mark
fork and handle the connection in child process: 4 marks
*/


int main() {
    int server_fd = create_server_socket(PORT);
    printf("Server is running on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // No pending connections
                usleep(100000); // Sleep for 100ms
                continue;
            } else {
                perror("accept() failed");
                continue;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd); // Close server socket in child
            handle_client(client_fd);
        } else if (pid > 0) {
            // Parent process
            close(client_fd); // Parent doesn't need client socket
        } else {
            perror("fork() failed");
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}

