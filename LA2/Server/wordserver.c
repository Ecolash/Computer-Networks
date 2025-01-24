/*=============================================================================================================
Assignment 2 Submission
Name: Tuhin Mondal
Roll number: 22CS10087
Link of the pcap file: https://drive.google.com/file/d/1hkY3sQ2QK1hJYJERApmz-CLt3XHnHZ_L/view?usp=drive_link
=============================================================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT    8080
#define MAX_BUFFER_SIZE 512

char *format_time()
{
    time_t currentTime;
    time(&currentTime);
    struct tm *localTime = localtime(&currentTime);
    static char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localTime);
    return timeStr;
}

int send_file(FILE* fp, int sockfd, struct sockaddr_in addr) 
{
    char buffer[MAX_BUFFER_SIZE];
    char CLIENT_REQUEST[MAX_BUFFER_SIZE];
    memset(buffer, '\0', MAX_BUFFER_SIZE);
    socklen_t addr_size = sizeof(addr);

    while(strcmp(buffer, "FINISH") != 0)
    {

        int recv_status = recvfrom(sockfd, CLIENT_REQUEST, MAX_BUFFER_SIZE, 0, (struct sockaddr*)&addr, &addr_size);
        switch (recv_status)
        {
            case -1: printf("[-] Error in receiving client request.\n"); return -1;
            default: printf("[%s] Client request : %s\n", format_time(), CLIENT_REQUEST); break;
        }

        int read = fscanf(fp, "%s", buffer);
        if (read == -1) { printf("[-] Error in reading file.\n"); return -1; }
        buffer[strcspn(buffer, "\n")] = 0;
        int send_status = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&addr, addr_size);
        switch (send_status)
        {
            case -1: printf("[-] Error in sending file.\n"); return -1;
            default: printf("[%s] Server response: %s\n", format_time(), buffer); break;
        }

    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    int PORT = DEFAULT_PORT;
    if (argc == 2) PORT = atoi(argv[1]);

    int sockfd;
    struct sockaddr_in si_me, si_other;
    char FILENAME[MAX_BUFFER_SIZE];
    memset(FILENAME, '\0', MAX_BUFFER_SIZE);
    socklen_t addr_size;

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
    int get_FILENAME = recvfrom(sockfd, FILENAME, MAX_BUFFER_SIZE, 0, (struct sockaddr*)&si_other, &addr_size);

    switch (get_FILENAME)
    {
        case -1: printf("[-] Error in receiving Filename.\n"); exit(1);
        default: printf("[%s] Filename received from client: %s\n", format_time(), FILENAME); break;
    }
    
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, '\0', MAX_BUFFER_SIZE);
    if (fopen(FILENAME, "r")) {
        FILE *fp = fopen(FILENAME, "r");
        int read = fscanf(fp, "%s", buffer);
        buffer[strcspn(buffer, "\n")] = 0;
        int send_status = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&si_other, addr_size);
        switch (send_status)
        {
            case -1: printf("[-] Error in sending response.\n"); exit(1);
            default: printf("[%s] Server response: %s\n", format_time(), buffer); break;
        }
        int status = send_file(fp, sockfd, si_other);
        switch(status)
        {
            case -1: printf("\n[-] File transfer failed!\n"); exit(EXIT_FAILURE);
            default: printf("\n[+] File transfer was successful!\n");
        }
        return 0;
    } 
    else 
    {
        strcpy(buffer, "NOT FOUND ");
        strcat(buffer, FILENAME);
    }

    int send_status = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&si_other, addr_size);
    switch (send_status)
    {
        case -1: printf("[-] Error in sending response.\n"); exit(1);
        default: printf("[%s] Server Response: %s\n\n", format_time(), buffer); exit(EXIT_SUCCESS);
    }
    close(sockfd);
    return 0;
}
