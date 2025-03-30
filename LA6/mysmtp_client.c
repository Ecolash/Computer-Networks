/*
=============================================================================================================
Assignment 6 Submission
-------------------------------------------------------------------------------------------------------------
Name: Tuhin Mondal
Roll number: 22CS10087
-------------------------------------------------------------------------------------------------------------

Options:

-IP <IP>     : IP address of the server (default: 127.0.0.1)
-port <port> : Port number to connect to (default: 6655)

$ gcc mysmtp_client.c -o mysmtp_client
$ ./mysmtp_client -IP 192.168.1.100 -port 2525
=============================================================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define BUFFER_SIZE 1024

#define RED   "\033[0;31m"
#define YELLOW "\033[0;33m"
#define GREEN "\033[0;32m"
#define BOLD  "\033[1m"
#define RESET "\033[0m"

const char success[]   = GREEN BOLD "200 OK" RESET "\n";
const char nested[]    = YELLOW BOLD "403 FORBIDDEN " RESET YELLOW "Nested command not allowed" RESET "\n";
const char forbidden[] = YELLOW BOLD "403 FORBIDDEN " RESET YELLOW "Action not permitted" RESET "\n";
const char invalid[]   = YELLOW BOLD "400 ERR" RESET YELLOW " Invalid command syntax" RESET "\n";
const char not_found[] = RED BOLD "401 NOT FOUND" RESET "\n";
const char serv_err[]  = RED BOLD "500 SERVER ERROR" RESET "\n";

int main(int argc, char *argv[]) {

    char server_ip[16] = "127.0.0.1";
    int port = 2525;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-IP") == 0) if (i + 1 < argc) strcpy(server_ip, argv[++i]);
        if (strcmp(argv[i], "-port") == 0) if (i + 1 < argc) port = atoi(argv[++i]);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(1);
    }

    printf("\033[2J\033[H");
    printf("Successfully connected to the server " GREEN BOLD "%s : %d" RESET"\n", server_ip, port);
    char command_buffer[BUFFER_SIZE];
    char response_buffer[BUFFER_SIZE];
    int n;

    while (1)
    {
        printf("\n%s> ", BOLD);
        fflush(stdout); 

        n = read(STDIN_FILENO, command_buffer, BUFFER_SIZE - 1);
        if (n < 0) exit(EXIT_FAILURE);

        command_buffer[n] = '\0';
        command_buffer[strcspn(command_buffer, "\r\n")] = '\0';
        printf(RESET);

        int sent = send(socket_fd, command_buffer, strlen(command_buffer), 0);
        if (sent < 0) exit(EXIT_FAILURE);

        if (strncmp(command_buffer, "DATA", 4) == 0) 
        {
            n = recv(socket_fd, response_buffer, BUFFER_SIZE - 1, 0);
            if (n < 0) exit(EXIT_FAILURE);
            response_buffer[n] = '\0';
            printf("%s", response_buffer);
            
            if (strncmp(response_buffer, forbidden, strlen(forbidden)) == 0) continue;
            
            printf("Enter your message (end with a single dot '.'): \n");

            char message_line[BUFFER_SIZE];
            int msglen;
            while (1) 
            {
                msglen = 0;
                memset(message_line, 0, BUFFER_SIZE);
                msglen = read(STDIN_FILENO, message_line, BUFFER_SIZE - 1); 
                if (msglen < 0) { perror("read"); break; }
                if (msglen == 0) break;

                message_line[msglen] = '\0';
                message_line[strcspn(message_line, "\r\n")] = '\0';

                n = send(socket_fd, message_line, strlen(message_line), 0);
                if (n < 0) break;
                    
                if (strcmp(message_line, ".") == 0) break;
            }
        }
        
        n = recv(socket_fd, response_buffer, BUFFER_SIZE - 1, 0);
        if (n < 0) exit(EXIT_FAILURE);
        response_buffer[n] = '\0';
        printf("%s", response_buffer);
        response_buffer[strcspn(response_buffer, "\r\n")] = '\0';

        if (strncmp(response_buffer, "\033[0;32m200 Goodbye\033[0m", 23) == 0)
            break;
    }
    close(socket_fd);
    return 0;
}
