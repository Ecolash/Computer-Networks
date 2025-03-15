#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <getopt.h>

#define DEFAULT_FILE    "tasks.txt"
#define RESULTS_FILE    "results.txt"
#define DEFAULT_PORT    6655
#define BUFFER_SIZE     1024
#define MAX_TASKS       1000

void ERROR(const char *msg)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "[-] %s", msg);
    perror(buf);
    exit(EXIT_FAILURE);
}

typedef struct
{
    int status[MAX_TASKS];   // 0: not processed, 1: processing, 2: completed
    char tasks[MAX_TASKS][512];
    int num_tasks;
    int next_task;
} Queue;

typedef Queue* QueuePtr;

Queue *task_queue;
int queue_mtx;
int file_mtx;

void P(int semid)
{
    struct sembuf op = {0, -1, SEM_UNDO};
    if (semop(semid, &op, 1) == -1)
    {
        perror("[-] P() error");
        exit(EXIT_FAILURE);
    }
}

void V(int semid)
{
    struct sembuf op = {0, 1, SEM_UNDO};
    if (semop(semid, &op, 1) == -1)
    {
        perror("[-] V() error");
        exit(EXIT_FAILURE);
    }
}

void terminate(int s)
{
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
    printf("[+] terminate(): Forked client handler terminated!\n");
}

void SET_NONBLOCKING(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    int nonblock = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (nonblock == -1) ERROR("Failed to set non-blocking mode");
    return;
}

int load_tasks(const char *filename)
{
    char line[512];
    task_queue->num_tasks = 0;
    task_queue->next_task = 0;
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;
    
    for (int i = 0; i < MAX_TASKS; i++) task_queue->status[i] = 0;    
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (task_queue->num_tasks < MAX_TASKS)
        {
            int curr = task_queue->num_tasks;
            line[strcspn(line, "\n")] = 0;
            strncpy(task_queue->tasks[curr], line, 512 - 1);
            task_queue->tasks[curr][512 - 1] = '\0';
            task_queue->num_tasks++;
        }
        else return -2;
    }
    fclose(fp);
    return task_queue->num_tasks;
}

void handle_client(int client_fd, int ID)
{
    char buffer[BUFFER_SIZE];
    char curr_task[512] = {0};
    int busy = 0;
    int curr_index = -1;
    int T = task_queue->num_tasks;

    while (1)
    {
        P(queue_mtx);
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        int next = task_queue->next_task;
        V(queue_mtx);

        if (n > 0)
        {
            buffer[strcspn(buffer, "\n")] = 0; 
            if (strcmp(buffer, "GET_TASK") == 0)
            {
                if (busy == 0)
                {
                    P(queue_mtx);
                    int found = 0;
                    for (int i = 0; i < T; i++) 
                    {
                        if (task_queue->status[i] != 0) continue;
                        curr_index = i;
                        strncpy(curr_task, task_queue->tasks[i], 512);
                        task_queue->status[i] = 1;
                        if (i == task_queue->next_task) task_queue->next_task++;
                        found = 1;
                        break;
                    }
                    V(queue_mtx);
                    if (found)
                    {
                        char task_msg[BUFFER_SIZE];
                        snprintf(task_msg, BUFFER_SIZE, "Task: %s", curr_task);
                        send(client_fd, task_msg, strlen(task_msg), 0);
                        printf("[+] Task [%d] assigned to client %d: %s\n", curr_index, ID, curr_task);
                        busy = 1;
                    }
                    else
                    {
                        char *msg = "No tasks available";
                        send(client_fd, msg, strlen(msg), 0);
                        printf("[+] No tasks available for client %d\n", ID);
                    }
                }
                else
                {
                    char *msg = "Already processing a task";
                    send(client_fd, msg, strlen(msg), 0);
                    printf("[+] Client %d already has a task\n", ID);
                }
            }
            else if (strncmp(buffer, "RESULT", 6) == 0)
            {
                int result;
                sscanf(buffer, "RESULT %d", &result);
                FILE *fp = fopen(RESULTS_FILE, "a");
                
                P(file_mtx);
                if (fp == NULL) perror("[-] Failed to open results file");
                fprintf(fp, "%s = %d\n", curr_task, result);
                fclose(fp);
                V(file_mtx);

                P(queue_mtx);
                task_queue->status[curr_index] = 2;
                V(queue_mtx);
                printf("[+] Result from client %d: %d\n", ID, result);
                busy = 0;
            }
            else if (strcmp(buffer, "exit") == 0)
            {
                printf("[+] Client %d exiting...\n", ID);
                break;
            }
        }
        else if (n == 0)
        {
            if (busy == 1)
            {
                printf("[-] Client %d disconnected while processing task: %s\n", client_fd, curr_task);
                P(queue_mtx);
                if (task_queue->status[curr_index] == 1) task_queue->status[curr_index] = 0;
                if (curr_index < task_queue->next_task) task_queue->next_task = curr_index;
                V(queue_mtx);
            }
            else printf("[+] Client %d disconnected\n", client_fd);
            break;
        }
        usleep(10000);
    }
    close(client_fd);
    exit(0);
}

bool completed()
{
    bool finished = true;
    P(queue_mtx);
    for (int i = 0; i < task_queue->num_tasks; i++)
    {
        if (task_queue->status[i] != 2)
        {
            finished = false;
            break;
        }
    }
    V(queue_mtx);
    return finished;
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    char tasks[256] = DEFAULT_FILE;
    signal(SIGCHLD, terminate);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0) if (i + 1 < argc) port = atoi(argv[++i]);
        if (strcmp(argv[i], "-f") == 0) if (i + 1 < argc) strncpy(tasks, argv[++i], sizeof(tasks) - 1);
    }

    tasks[sizeof(tasks) - 1] = '\0';

    int server_fd;
    int client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int opt = 1;

    int shmid  = shmget(IPC_PRIVATE, sizeof(Queue), IPC_CREAT | 0666);
    if (shmid == -1) ERROR("Shared memory allocation failed");

    task_queue = (Queue *)shmat(shmid, NULL, 0);
    if (task_queue == (void *)-1) ERROR("Shared memory attachment failed");

    int T = load_tasks(tasks);
    switch (T) {
        case -1: ERROR("Unable to read task file");
        case -2: ERROR("More than 1000 tasks");
        default: printf("[+] %d tasks loaded successfully!\n", task_queue->num_tasks);
    }

    queue_mtx = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    file_mtx = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (queue_mtx == -1) ERROR("Failed to create queue semaphore");
    if (file_mtx == -1) ERROR("Failed to create file semaphore");
    semctl(queue_mtx, 0, SETVAL, 1);
    semctl(file_mtx, 0, SETVAL, 1);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    switch (server_fd) {
        case -1: ERROR("Server socket creation failed");
        default: printf("[+] Server socket created successfully!\n");
    }

    int sockopt = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    switch (sockopt)
    {
        case -1: ERROR("Failed to set socket options");
        default: printf("[+] Socket options set successfully!\n");
    }
    
    SET_NONBLOCKING(server_fd);
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    int binded = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
    switch (binded) {
        case -1: ERROR("Error in binding"); 
        default: printf("[+] Binded to port %d\n", port); break;
    }

    int listening = listen(server_fd, 10);
    switch (listening) {
        case -1: ERROR("Error in listening.\n");
        default: printf("[+] Server listening on port %d\n", port);
    }
  
    int client_cnt = 0;
    while (1)
    {
        if (completed()) break;

        len = sizeof(struct sockaddr_in);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if (client_fd == -1) continue;
        
        client_cnt++;
        printf("[+] New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        SET_NONBLOCKING(client_fd);
        pid_t pid = fork();
        switch (pid)
        {
            case -1: close(client_fd); ERROR("Fork failed");
            case  0: close(server_fd); handle_client(client_fd, client_cnt);
            default: close(client_fd);
        }
        usleep(10000);
    }
    
    for(int i = 0; i < client_cnt; i++) wait(NULL);
    printf("[+] All tasks completed. Shutting down server...\n");

    return 0;
}