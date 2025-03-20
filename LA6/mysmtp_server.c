/*
=============================================================================================================
Assignment 6 Submission 
-------------------------------------------------------------------------------------------------------------
Name: Tuhin Mondal
Roll number: 22CS10087
-------------------------------------------------------------------------------------------------------------

Options: -port <port> : Port number to listen on (default: 2525)

$ gcc mysmtp_server.c -o mysmtp_server
$ ./mysmtp_server 2525
=============================================================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define PORT 2525
#define BUFFER_SIZE 1024

#define GREEN "\033[0;32m"
#define RESET "\033[0m"
#define BOLD  "\033[1m"
#define CYAN  "\033[0;36m"
#define RED   "\033[0;31m"
#define YELLOW "\033[0;33m"

char *date() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    static char date_str[64];
    strftime(date_str, sizeof(date_str), "%d-%m-%Y", t);
    return date_str;
}

const char success[]   = GREEN BOLD "200 OK" RESET "\n";
const char nested[]    = YELLOW BOLD "403 FORBIDDEN " RESET YELLOW "Nested command not allowed" RESET "\n";
const char forbidden[] = YELLOW BOLD "403 FORBIDDEN " RESET YELLOW "Action not permitted" RESET "\n";
const char invalid[]   = YELLOW BOLD "400 ERR" RESET YELLOW " Invalid command syntax" RESET "\n";
const char not_found[] = RED BOLD "401 NOT FOUND" RESET "\n";
const char serv_err[]  = RED BOLD "500 SERVER ERROR" RESET "\n";

void handle_helo(int client_fd, const char *buffer) {
    char client_id[BUFFER_SIZE];
    if (sscanf(buffer, "HELO %s", client_id) != 1) {
        send(client_fd, invalid, strlen(invalid), 0);
        printf("Error: Invalid HELO syntax\n");
        return;
    }
    printf("HELO: %s\n", client_id);
    send(client_fd, success, strlen(success), 0);
}

void handle_mail_from(int client_fd, const char *buffer, char *sender) {
    char temp_email[BUFFER_SIZE] = {0};
    if (sscanf(buffer, "MAIL FROM:%s", temp_email) != 1) {
        send(client_fd, invalid, strlen(invalid), 0);
        printf("Error: Invalid MAIL FROM syntax\n");
        return;
    }
    
    temp_email[strcspn(temp_email, " \r\n")] = '\0';
    temp_email[BUFFER_SIZE - 1] = '\0';
    strcpy(sender, temp_email);
    printf("MAIL FROM: %s\n", sender);
    send(client_fd, success, strlen(success), 0);
}

void handle_rcpt_to(int client_fd, const char *buffer, char *recipient) {
    char temp_email[BUFFER_SIZE] = {0};
    if (sscanf(buffer, "RCPT TO:%s", temp_email) != 1) {
        send(client_fd, invalid, strlen(invalid), 0);
        printf("Error: Invalid RCPT TO syntax\n");
        return;
    }

    temp_email[strcspn(temp_email, " \r\n")] = '\0';
    temp_email[BUFFER_SIZE - 1] = '\0';
    strcpy(recipient, temp_email);
    printf("RCPT TO: %s\n", recipient);
    send(client_fd, success, strlen(success), 0);
}


void handle_client(int fd, struct sockaddr_in client_addr) 
{
    int client_fd = fd;
    char *client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    printf("Client connected from %s:%d\n\n", client_ip, client_port);

    char buffer[BUFFER_SIZE];
    char sender[256];
    char recipient[256];
    int state = 0; 

    bzero(buffer, sizeof(buffer));
    bzero(sender, sizeof(sender));
    bzero(recipient, sizeof(recipient));

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strncmp(buffer, "HELO ", 5) == 0)
        {
            handle_helo(client_fd, buffer);
            state = 1;
        }
        else if (strncmp(buffer, "MAIL FROM:", 10) == 0)
        {
            switch (state) {
                case 1:  handle_mail_from(client_fd, buffer, sender); state = 2; break;
                case 2:  send(client_fd, nested, strlen(nested), 0); printf("Error: Nested MAIL FROM received\n"); break;
                default: send(client_fd, forbidden, strlen(forbidden), 0); printf("Error: MAIL FROM received out of sequence\n");
            }
        }

        else if (strncmp(buffer, "RCPT TO:", 8) == 0)
        {
            switch (state)
            {
                case 1:  send(client_fd, forbidden, strlen(forbidden), 0); printf("Error: RCPT TO received before MAIL FROM\n"); break;
                case 2:  handle_rcpt_to(client_fd, buffer, recipient); state = 3; break;
                case 3:  send(client_fd, nested, strlen(nested), 0); printf("Error: Nested RCPT TO received\n"); break;
                default: send(client_fd, forbidden, strlen(forbidden), 0); printf("Error: RCPT TO received out of sequence\n"); break;
            }
        }
        else if (strcmp(buffer, "DATA") == 0)
        {
            if (state >= 3)
            {
                char message[4096];
                bzero(message, sizeof(message));
                send(client_fd, success, strlen(success), 0);
                printf("DATA\n");

                while (1)
                {
                    memset(buffer, 0, BUFFER_SIZE);
                    int m = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
                    if (m <= 0) break;
                    buffer[m] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = '\0';
                    if (strcmp(buffer, ".") == 0) break;
                    strcat(message, buffer);
                    strcat(message, "\n");
                }

                if (strlen(recipient) == 0)
                {
                    char msg[1000] = ": Recipient not specified\n";
                    char concat[1000];

                    strcpy(concat, not_found);
                    strcat(concat, msg);
                    send(client_fd, concat, strlen(concat), 0);
                    printf("Error: DATA received without recipient\n");
                }
                else
                {
                    char filename[512];
                    snprintf(filename, sizeof(filename), "mailbox/%s.txt", recipient);
                    FILE *fp = fopen(filename, "a+");
                    if (fp == NULL)
                    {
                        send(client_fd, serv_err, strlen(serv_err), 0);
                        printf("Error: Unable to open mailbox file\n");
                    }
                    else
                    {
                        int email_id = 1;
                        FILE *read_fp = fopen(filename, "r");
                        if (read_fp != NULL) 
                        {
                            char line[256];
                            while (fgets(line, sizeof(line), read_fp)) if (sscanf(line, "ID: %d", &email_id) == 1) email_id++;
                            fclose(read_fp);
                        }
                        
                        fprintf(fp, "ID: %d\n", email_id);
                        fprintf(fp, "From: %s\n", sender);
                        fprintf(fp, "To: %s\n", recipient);
                        fprintf(fp, "Date: %s\n", date());
                        fprintf(fp, "\n%s\n", message);
                        fprintf(fp, "---------------------------------END---------------------------------\n");
                        fclose(fp);

                        char msg[100] = "Message stored successfully\n";
                        char concat[1000];
                        strcpy(concat, success);
                        strcat(concat, msg);
                        send(client_fd, concat, strlen(concat), 0);
                        printf("DATA received, message stored.\n");
                    }
                }

                memset(sender, 0, sizeof(sender));
                memset(recipient, 0, sizeof(recipient));
                state = 1; 
            }
            else
            {
                send(client_fd, forbidden, strlen(forbidden), 0);
                printf("Error: DATA received out of sequence\n");
            }
        }

        else if (strncmp(buffer, "LIST ", 5) == 0)
        {
            char email[256];
            if (sscanf(buffer, "LIST %255s", email) != 1) {
                send(client_fd, invalid, strlen(invalid), 0);
                printf("Error: Invalid LIST command syntax\n");
                continue;
            }
            char filename[512];
            snprintf(filename, sizeof(filename), "mailbox/%s.txt", email);
            FILE *fp = fopen(filename, "r");
            if (!fp)
            {
                send(client_fd, not_found, strlen(not_found), 0);
                printf("LIST %s\nRecipient mailbox not found.\n", email);
            }
            else
            {
                char line[256];
                int current_id = 0;
                char sender_email[256];
                char response[4096];
                char date_str[64];

                bzero(sender_email, sizeof(sender_email));
                bzero(date_str, sizeof(date_str));
                bzero(response, sizeof(response));
                bzero(line, sizeof(line));

                strcpy(response, success);
                while (fgets(line, sizeof(line), fp)) 
                {
                    line[strcspn(line, "\r\n")] = '\0';
                    if (strncmp(line, "ID:", 3) == 0) {
                        sscanf(line, "ID: %d", &current_id);
                        if (fgets(line, sizeof(line), fp)) {
                            line[strcspn(line, "\r\n")] = '\0';
                            if (strncmp(line, "From:", 5) == 0)
                                sscanf(line, "From: %255s", sender_email);
                        }
                        
                        fgets(line, sizeof(line), fp);
                        if (fgets(line, sizeof(line), fp)) {
                            line[strcspn(line, "\r\n")] = '\0';
                            if (strncmp(line, "Date:", 5) == 0)
                                sscanf(line, "Date: %63[^\n]", date_str);
                        }
                        
                        while (fgets(line, sizeof(line), fp)) {
                            if (strstr(line, "---------------------------------END---------------------------------"))
                                break;
                        }
                        
                        char entry[512];
                        snprintf(entry, sizeof(entry), "%s%s%2d:%s Email from " CYAN "%-25s" RESET "[%s]\n", GREEN, BOLD, current_id, RESET, sender_email, date_str);
                        strcat(response, entry);
                    }
                }
                fclose(fp);
                send(client_fd, response, strlen(response), 0);
                printf("LIST %s\nEmails retrieved: List sent.\n", email);
            }
        }
        else if (strncmp(buffer, "GET_MAIL ", 9) == 0)
        {
            char email[256];
            int mail_id;
            if (sscanf(buffer, "GET_MAIL %s %d", email, &mail_id) < 2)
            {
                send(client_fd, invalid, strlen(invalid), 0);
                printf("Error: Invalid GET_MAIL command syntax\n");
            }
            else
            {
                char filename[512];
                snprintf(filename, sizeof(filename), "mailbox/%s.txt", email);
                FILE *fp = fopen(filename, "r");
                if (!fp) {
                    send(client_fd, not_found, strlen(not_found), 0);
                    printf("GET_MAIL %s %d: Mailbox not found.\n", email, mail_id);
                    continue;
                }

                char line[512], email_content[4096];
                bzero(email_content, sizeof(email_content));
                bzero(line, sizeof(line));
                int current_id = 0;
                int found = 0;
                
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = '\0';
                    if (strncmp(line, "ID:", 3) == 0) {
                        sscanf(line, "ID: %d", &current_id);
                        
                        if (current_id == mail_id) {
                            found = 1;
                            strcat(email_content, success);
                            strcat(email_content, "\n" CYAN);
                            strcat(email_content, line);
                            strcat(email_content, "\n");
                            while (fgets(line, sizeof(line), fp)) {
                                if (strstr(line, "---------------------------------END---------------------------------")) break;
                                line[strcspn(line, "\r\n")] = '\0';
                                strcat(email_content, line);
                                strcat(email_content, "\n");
                            }
                            break;
                        } else 
                        {
                            while (fgets(line, sizeof(line), fp)) 
                            {
                                if (strstr(line, "---------------------------------END---------------------------------")) break;
                            }
                        }
                    }
                }
                
                fclose(fp);
                if (found) {
                    strcat(email_content, RESET);
                    send(client_fd, email_content, strlen(email_content), 0);
                    printf("GET_MAIL %s %d: Email sent.\n", email, mail_id);
                } else {
                    send(client_fd, not_found, strlen(not_found), 0);
                    printf("GET_MAIL %s %d: Email ID not found.\n", email, mail_id);
                }
            }
        }
        else if (strcmp(buffer, "QUIT") == 0)
        {
            send(client_fd, "\033[0;32m200 Goodbye\033[0m\n", strlen("\033[0;32m200 Goodbye\033[0m\n"), 0);
            printf("Client disconnected.\n");
            break;
        }
        else
        {
            char msg[100] = "Command not recognized\n";
            char concat[1000];
            strcpy(concat, invalid);
            strcat(concat, msg);
            send(client_fd, concat, strlen(concat), 0);
            printf("Error: Command not recognized\n");
        }
    }
    close(client_fd);
    exit(0);
}

void terminate() {
    while (waitpid(-1, NULL, WNOHANG) > 0);
    printf("terminate() - Client handler closed!\n");
}

int main(int argc, char *argv[]) 
{
    int port = PORT;
    int mailbox = mkdir("mailbox", 0777);
    if (mailbox == -1) {
        if (errno != EEXIST)
        {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }

    signal(SIGCHLD, terminate);
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-port") == 0)
            if (i + 1 < argc) port = atoi(argv[++i]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    int ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0)
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    int binded = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (binded < 0)
    {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int listening = listen(server_fd, 10);
    if (listening < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);
    
    
    while (1) {
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        switch(pid) 
        {
            case -1: perror("fork"); close(client_fd); break;
            case  0: close(server_fd); handle_client(client_fd, client_addr); break;
            default: close(client_fd);break;
        }
    }

    close(server_fd);
    return 0;
}