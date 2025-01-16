#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *format_time()
{
    time_t currentTime;
    time(&currentTime);
    char *timeStr = ctime(&currentTime);
    timeStr[strcspn(timeStr, "\n")] = 0;
    return timeStr;
}

int main(int argc, char **argv)
{
    int PORT;
    if (argc != 2)
    {
        printf("Usage: %s <PORT>\n", argv[0]);
        exit(1);
    }
    PORT = atoi(argv[1]);

    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_size;

    char buffer[1024];
    printf("Enter message: ");
    fgets(buffer, 1024, stdin);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    switch (sockfd)
    {
        case -1: printf("[-] Error in connection.\n"); exit(1);
        default: printf("[+] Client Socket is created.\n"); break;
    }

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");


    addr_size = sizeof(server_addr);
    sendto(sockfd, buffer, 1024, 0, (struct sockaddr*)&server_addr, addr_size);
    char *timeStr = format_time();
    printf("[%s] Sent to server: %s\n", timeStr, buffer);
    close(sockfd);
    return 0;
}