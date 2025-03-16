#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024
#define PORT 6655

int ERR_FLAG = 0;

/*
ERR_FLAG = 1 : Division by 0
ERR_FLAG = 2 : Unknown operator
ERR_FLAG = 3 : Incorrect format
*/

void ERROR(const char *msg)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "[-] %s", msg);
    perror(buf);
    exit(EXIT_FAILURE);
}

int calculate(const char *task)
{
    char op;
    int num1, num2;
    const char *ptr = task;
    ERR_FLAG = 0;

    if (strncmp(task, "Task:", 5) == 0) ptr += 5;
    if (sscanf(ptr, "%d %c %d", &num1, &op, &num2) != 3) {ERR_FLAG = 3; return 0; }

    int ans = 0;
    switch (op)
    {
        case '+': ans = num1 + num2; break;
        case '-': ans = num1 - num2; break;
        case '*': ans = num1 * num2; break;
        case '&': ans = num1 & num2; break;
        case '|': ans = num1 | num2; break;
        case '^': ans = num1 ^ num2; break;
        case '%': if (num2 != 0) ans = num1 % num2; else ERR_FLAG = 1; break;
        case '/': if (num2 != 0) ans = num1 / num2; else ERR_FLAG = 1; break;
        default: ERR_FLAG = 2;
    }
    if (ERR_FLAG != 0) return 0;
    printf("[+] Calculated %d %c %d = %d\n", num1, op, num2, ans);
    return ans;
}

int main(int argc, char* argv[])
{
    int port = PORT;
    int max_tasks = 1000;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0) if (i + 1 < argc) port = atoi(argv[++i]);
        if (strcmp(argv[i], "-n") == 0) if (i + 1 < argc) max_tasks = atoi(argv[++i]);
    }

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int waiting = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    switch (sockfd) {
        case -1: ERROR("Socket creation failed");
        default: printf("[+] Socket created successfully!\n");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    int connected = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
    switch (connected) {
        case -1: ERROR("[-] Error in connection.\n");
        default: printf("[+] Connected to server at %s:%d\n\n", SERVER_IP, port); 
    }

    int task_count = 0;
    while (task_count < max_tasks)
    {
        if (!waiting)
        {
            char *msg = "GET_TASK";
            send(sockfd, msg, strlen(msg), 0);
            printf("[+] Task request sent\n");
            waiting = 1;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (n > 0)
        {
            buffer[strcspn(buffer, "\n")] = 0;
            if (strncmp(buffer, "Task:", 5) == 0)
            {
                printf("[+] Received %s\n", buffer);
                int result = calculate(buffer);
                char result_msg[BUFFER_SIZE];
                snprintf(result_msg, BUFFER_SIZE, "RESULT %d %d", result, ERR_FLAG);

                send(sockfd, result_msg, strlen(result_msg), 0);
                printf("[+] Sent result: %d (ERR_FLAG: %d)\n\n", result, ERR_FLAG);
                task_count++;
                waiting = 0;
            }
            else if (strcmp(buffer, "No tasks available") == 0) break;
            else if (strcmp(buffer, "Already processing a task") == 0) printf("[.] Already processing a task\n");
            else printf("[-] Unexpected server message: %s\n", buffer);
        }

        int T = 1 + (rand() % 10);
        sleep(T);
    }

    if (task_count == max_tasks) printf("[+] Max Task Limit - exit()\n");
    else printf("[+] No Task Available - exit()\n");
    
    char exit_msg[5] = "EXIT";
    send(sockfd, exit_msg, strlen(exit_msg), 0);
    close(sockfd);

    printf("[+] Connection closed\n");
    exit(EXIT_SUCCESS);
}