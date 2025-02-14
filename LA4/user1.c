// USER-1 (SENDER)

#include "ksocket.h"

int main(int argc, char *argv[]) 
{
    argcheck(argc, argv);

    int sockfd;
    struct sockaddr_in sADDR;
    char buffer[BUFFER_SIZE];
    char src_ip[16], dst_ip[16];
    
    strcpy(src_ip, argv[1]);
    strcpy(dst_ip, argv[3]);
    uint16_t src_port = atoi(argv[2]);
    uint16_t dst_port = atoi(argv[4]);

    printf("[+] Source IP address      : %s\n", src_ip);
    printf("[+] Source port number     : %d\n", src_port);
    printf("[+] Destination IP address : %s\n", dst_ip);
    printf("[+] Destination port number: %d\n\n", dst_port);

    sADDR.sin_addr.s_addr = inet_addr(dst_ip);
    sADDR.sin_family = AF_INET;
    sADDR.sin_port = htons(dst_port);
    
    if ((sockfd = k_socket(AF_INET, SOCK_KTP, 0)) < 0) {
        perror("[-] Socket creation failed");
        return EXIT_FAILURE;
    }
    printf("[+] Socket created successfully\n");

    struct sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = inet_addr(src_ip);
    bind_addr.sin_port = htons(src_port);

    if (k_bind(sockfd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("[-] Binding failed");
        return EXIT_FAILURE;
    }
    printf("[+] Binding successful\n");

    char filename[256];
    while(1) {
        printf("[!] Enter the filename to send: ");
        if (scanf("%255s", filename) != 1) {
            printf("[-] Error reading filename\n");
            continue;
        }
        if (access(filename, F_OK) != -1) break;
        printf("[-] File does not exist. Please enter a valid filename.\n");
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[-] Failed to open file");
        return EXIT_FAILURE;
    }
    printf("[+] File opened successfully\n");

    buffer[0] = 'F';  
    strcpy(buffer + 1, filename);
    if (k_sendto(sockfd, buffer, strlen(filename) + 2, 0, (struct sockaddr*)&sADDR, sizeof(sADDR)) < 0) {
        perror("[-] Failed to send filename");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[+] Starting file transmission\n");
    int readlen;
    int total_sent = 0;
    int packets_sent = 0;

    while ((readlen = read(fd, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[readlen] = '\0';  
        int sendlen = k_sendto(sockfd, buffer, readlen, 0, (struct sockaddr*)&sADDR, sizeof(sADDR));
        if (sendlen < 0) {
            perror("[-] Failed to send data");
            close(fd);
            return EXIT_FAILURE;
        }

        total_sent += sendlen;
        packets_sent++;
        printf("[+] Packet %d sent: %d bytes\n", packets_sent, sendlen);
    }
    printf("\n");

    buffer[0] = '$';
    if (k_sendto(sockfd, buffer, 1, 0, (struct sockaddr*)&sADDR, sizeof(sADDR)) < 0) {
        perror("[-] Failed to send EOF marker");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[+] File sent successfully (%d bytes in %d packets)\n", total_sent, packets_sent);
    sleep(5);  
    close(fd);
    k_close(sockfd);
    return EXIT_SUCCESS;
}
