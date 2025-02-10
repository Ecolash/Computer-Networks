#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

const int PORT = 5050;
const int SIZE = 1024;

void handle_client(int clientfd) {
    int x1, x2, ans;
    char op, buffer[SIZE];
    int fdivby0, n;

    do {
        n = recv(clientfd, buffer, SIZE, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        sscanf(buffer, "%d", &x1);

        n = recv(clientfd, buffer, SIZE, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        sscanf(buffer, "%d", &x2);

        n = recv(clientfd, buffer, SIZE, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        sscanf(buffer, "%c", &op);

        fdivby0 = 0;
        switch (op) {
            case '+': ans = x1 + x2; break;
            case '-': ans = x1 - x2; break;
            case '*': ans = x1 * x2; break;
            case '%': ans = (x2 != 0) ? x1 % x2 : (fdivby0 = 1); break;
            case '/': ans = (x2 != 0) ? x1 / x2 : (fdivby0 = 1); break;
            default: sprintf(buffer, "ERROR - Invalid operator\n"); send(clientfd, buffer, strlen(buffer) + 1, 0); continue;
        }

        if (!fdivby0) sprintf(buffer, "%d", ans);
        else sprintf(buffer, "ERROR - Division by 0\n");

        send(clientfd, buffer, strlen(buffer) + 1, 0);
        n = recv(clientfd, buffer, SIZE, 0);
        buffer[n] = '\0';
        if (strcmp(buffer, "NO") == 0) break;
    } while (1);

    close(clientfd);
}

int main() {
    int serverfd, clientfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serverfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        close(serverfd);
        exit(EXIT_FAILURE);
    }

    if (listen(serverfd, 5) < 0) {
        perror("Listen failed");
        close(serverfd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Server Running .........\n");

    while (1) {
        len = sizeof(cliaddr);
        clientfd = accept(serverfd, (struct sockaddr*)&cliaddr, &len);
        if (clientfd < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[+] Client %s.%d connected\n\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        handle_client(clientfd);
    }

    close(serverfd);
    return 0;
}

