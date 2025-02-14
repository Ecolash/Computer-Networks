// USER-2 (RECEIVER)

#include "ksocket.h"

int main(int argc, char *argv[]) 
{
    argcheck(argc, argv);

    int sockfd;
    struct sockaddr_in sADDR;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size = sizeof(sADDR);
    char src_ip[16], dst_ip[16];

    strcpy(src_ip, argv[1]);
    strcpy(dst_ip, argv[3]);
    uint16_t src_port = atoi(argv[2]);
    uint16_t dst_port = atoi(argv[4]);

    printf("[+] Source IP address      : %s\n", src_ip);
    printf("[+] Source port number     : %d\n", src_port);
    printf("[+] Destination IP address : %s\n", dst_ip);
    printf("[+] Destination port number: %d\n\n", dst_port);

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
    printf("[+] Waiting for file transfer...\n\n");
    

    int recvlen = k_recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sADDR, &addr_size);
    if (recvlen <= 0) {
        printf("[-] Failed to receive filename\n");
        return EXIT_FAILURE;
    }

    char output_filename[270];
    snprintf(output_filename, sizeof(output_filename), "new_%d.txt", src_port);

    int fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("[-] Failed to open output file");
        return EXIT_FAILURE;
    }
    printf("[+] Receiving file: %s\n\n", output_filename);

    int total_received = 0;
    int packets_received = 0;

    while (1) {
        recvlen = k_recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sADDR, &addr_size);
        if (recvlen < 0) {
            perror("[-] Error receiving data");
            close(fd);
            return EXIT_FAILURE;
        }

        if (recvlen == 1 && buffer[0] == '$') break; 
        if (write(fd, buffer, recvlen) != recvlen) {
            perror("[-] Failed to write to file");
            close(fd);
            return EXIT_FAILURE;
        }

        total_received += recvlen;
        packets_received++;
        printf("[+] Packet %d received: %d bytes\n", packets_received, recvlen);
    }
    printf("\n");

    printf("[+] File received successfully (%d bytes in %d packets)\n", total_received, packets_received);
    sleep(5); 
    close(fd);
    k_close(sockfd);
    return EXIT_SUCCESS;
}
