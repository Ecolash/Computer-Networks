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

int main() {
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int x1, x2;
    char buffer[SIZE], op;
    char continue_response[SIZE];

    do {
        printf("\nEnter first number: ");
        scanf("%d", &x1);
        sprintf(buffer, "%d", x1);
        send(sockfd, buffer, strlen(buffer) + 1, 0);

        printf("Enter second number: ");
        scanf("%d", &x2);
        sprintf(buffer, "%d", x2);
        send(sockfd, buffer, strlen(buffer) + 1, 0);

        do {
            printf("Enter operator (+, -, *, /, %%): ");
            scanf(" %c", &op);
            if (strchr("+-*/%", op)) break;
            printf("Invalid operator. Please try again.\n\n");
        } while (1);

        sprintf(buffer, "%c", op);
        send(sockfd, buffer, strlen(buffer) + 1, 0);

        int n = recv(sockfd, buffer, SIZE, 0);
        buffer[n] = '\0';
        printf("Result: %d %c %d = %s\n\n", x1, op, x2, buffer);

        printf("\nDo you want to continue (YES/NO)? ");
        scanf("%s", continue_response);

        send(sockfd, continue_response, strlen(continue_response) + 1, 0);

    } while (strcmp(continue_response, "NO") != 0);

    close(sockfd);

    return 0;
}


