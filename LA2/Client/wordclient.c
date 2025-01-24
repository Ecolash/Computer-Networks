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

int write_file(int sockfd, struct sockaddr_in addr, char *FILENAME) 
{
    char CLIENT_REQUEST[MAX_BUFFER_SIZE];
    char SERVER_RESPONSE[MAX_BUFFER_SIZE];
    memset(CLIENT_REQUEST, '\0', MAX_BUFFER_SIZE);
    memset(SERVER_RESPONSE, '\0', MAX_BUFFER_SIZE);

    int count = 0;
    socklen_t addr_size = sizeof(addr);
    FILE *fp = fopen("received.txt", "w");
    fprintf(fp, "HELLO\n");
    
    do {
        memset(CLIENT_REQUEST, '\0', MAX_BUFFER_SIZE);
        sprintf(CLIENT_REQUEST, "WORD%d", ++count);
        
        int send_status = sendto(sockfd, CLIENT_REQUEST, strlen(CLIENT_REQUEST), 0, (struct sockaddr*)&addr, addr_size);
        switch (send_status)
        {
            case -1: printf("[-] Error in sending client request.\n"); return -1;
            default: printf("[%s] Client request : %s\n", format_time(), CLIENT_REQUEST); break;
        }

        int recv_status = recvfrom(sockfd, SERVER_RESPONSE, MAX_BUFFER_SIZE, 0, (struct sockaddr*)&addr, &addr_size);
        switch (recv_status)
        {
            case -1: printf("[-] Error in receiving server response.\n"); return -1;
            default: printf("[%s] Server response: %s\n", format_time(), SERVER_RESPONSE); break;
        }

        if (strcmp(SERVER_RESPONSE, "FINISH") != 0) {
            fprintf(fp, "%s\n", SERVER_RESPONSE);
            memset(SERVER_RESPONSE, '\0', MAX_BUFFER_SIZE);
        }
    } while (strcmp(SERVER_RESPONSE, "FINISH") != 0);

    fprintf(fp, "FINISH\n");
    fclose(fp);
    return count;
}

int main(int argc, char **argv)
{
    int PORT = DEFAULT_PORT;
    if (argc == 2) PORT = atoi(argv[1]);

    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_size;

    char FILENAME[MAX_BUFFER_SIZE];
    char SERVER_RESPONSE[MAX_BUFFER_SIZE];
    memset(FILENAME, '\0', MAX_BUFFER_SIZE);
    memset(SERVER_RESPONSE, '\0', MAX_BUFFER_SIZE);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    switch (sockfd)
    {
        case -1: printf("[-] Error in connection.\n"); exit(EXIT_FAILURE);
        default: printf("[+] Client Socket is created.\n"); break;
    }

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("[*] Enter filename: ");
    fgets(FILENAME, MAX_BUFFER_SIZE, stdin);
    FILENAME[strcspn(FILENAME, "\n")] = 0;

    addr_size = sizeof(server_addr);
    int send_FILENAME = sendto(sockfd, FILENAME, strlen(FILENAME), 0, (struct sockaddr*)&server_addr, addr_size);

    switch (send_FILENAME)
    {
        case -1: printf("[-] Error in sending filename.\n"); exit(EXIT_FAILURE);
        default: printf("[%s] Filename sent to server: %s\n", format_time(), FILENAME); break;
    }

    int recv_status = recvfrom(sockfd, SERVER_RESPONSE, MAX_BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &addr_size);
    switch(recv_status)
    {
        case -1: printf("[-] Error in receiving server response.\n"); exit(EXIT_FAILURE);
        default: printf("[%s] Server response: %s\n", format_time(), SERVER_RESPONSE); break;
    }

    if (strcmp(SERVER_RESPONSE, "HELLO") == 0)
    {
        int status = write_file(sockfd, server_addr, FILENAME);
        switch(status)
        {
            case -1: printf("\n[-] Error in writing file.\n"); exit(EXIT_FAILURE);
            default: printf("\n[+] File written successfully. %d packets transferred.\n", status); break;
        }
        return 0;
    }
    else 
    {
        printf("\n[-] FILE NOT FOUND!\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    close(sockfd);
    return 0;
}