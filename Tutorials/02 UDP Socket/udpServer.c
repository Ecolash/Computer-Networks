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
    struct sockaddr_in si_me, si_other;
    socklen_t addr_size;
    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    switch (sockfd)
    {
        case -1: printf("[-] Error in connection.\n"); exit(1);
        default: printf("[+] Server Socket is created.\n"); break;
    }

    memset(&si_me, '\0', sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = inet_addr("127.0.0.1");

    int binded = bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me));
    switch (binded)
    {
        case -1: printf("[-] Error in binding.\n"); exit(1);
        default: printf("[+] Binded to port %d.\n", PORT); break;
    }

    printf("[+] Server running...\n");
    addr_size = sizeof(si_other);
    recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&si_other, &addr_size);
    char *timeStr = format_time();
    printf("[%s] Client: %s\n", timeStr, buffer);
    close(sockfd);
    return 0;
}
