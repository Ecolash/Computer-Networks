#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#define TCP_PORT   5555
#define UDP_PORT   5556
#define SERVER_IP  "127.0.0.1"



void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    int check = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (check == -1)
    {
        printf("fcntl() - FD = %d", fd);
        exit(EXIT_FAILURE);
    }
}

int get_result()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    set_nonblocking(sockfd);
    int reuse = 1;

    int opt1 = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in saddr = {0};

    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(UDP_PORT);

    bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));

    char msg[1000] = {0};
    int n = -1;
    while (n < 0)
    {
        n = recvfrom(sockfd, msg, 100, 0, NULL, NULL);
        if (n < 0)
        {
            printf("[.] Waiting for result...\n");
            sleep(1);
            if (errno == EAGAIN) continue;
            if (errno == EWOULDBLOCK)continue;
            perror("recvfrom | non-bloking");
            return -1;
        }
        if (n == 0)
        {
            printf("[-] Server disconnected!");
            return -1;
        }
        break;
    }

    msg[n] = '\0';
    printf("%s\n", msg);
    close(sockfd);
    return 0;
}

int main() 
{
    int sockfd;
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    saddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(TCP_PORT);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));

    int num;
    printf("[+] Connected to server successfully!\n");
    printf("Enter number to send: ");
    scanf("%d", &num);

    char buff[32] = {0};
    snprintf(buff, sizeof(buff), "%d", num);
    send(sockfd, buff, sizeof(buff), 0);
    printf("Sent number successfully!\n\n");

    int res = get_result();
    if(res == 0) printf("Result received successfully\n");
    return 0;
}