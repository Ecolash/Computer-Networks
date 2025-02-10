#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ksocket.h"

int ktp_errno = 0;
static ktp_socket_t SM[MAX_KTP_SOCKETS];

int dropMessage(float p)
{
    float r = (float)rand() / (float)RAND_MAX;
    if (r < p) return 1;
    else return 0;
}

static int allocate_sm_slot(void)
{
    for (int i = 0; i < MAX_KTP_SOCKETS; i++)
    {
        if (!SM[i].allocated)
        {
            SM[i].allocated = 1;
            SM[i].owner = getpid();
            SM[i].send_count = 0;
            SM[i].recv_count = 0;
            SM[i].bound = 0;
            return i;
        }
    }
    return -1;
}


static int ktp_fd_to_index(int ktp_fd)
{
    int idx = ktp_fd - KTP_FD_BASE;
    if (idx < 0 || idx >= MAX_KTP_SOCKETS || !SM[idx].allocated) return -1;
    return idx;
}

int k_socket(int domain, int type, int protocol)
{
    if (type != SOCK_KTP) { errno = EPROTOTYPE; return -1; }

    int index = allocate_sm_slot();
    if (index == -1) { ktp_errno = KTP_ENOSPACE; return -1; }

    int udp_fd = socket(domain, SOCK_DGRAM, protocol);
    if (udp_fd < 0) { SM[index].allocated = 0; return -1; }
    SM[index].udp_fd = udp_fd;
    return index + KTP_FD_BASE;
}

int k_bind(int ktp_fd, const struct sockaddr_in *src, const struct sockaddr_in *dest)
{
    int idx = ktp_fd_to_index(ktp_fd);
    if (idx < 0) { errno = EBADF; return -1; }

    ktp_socket_t *ks = &SM[idx];
    if (bind(ks->udp_fd, (struct sockaddr *)src, sizeof(struct sockaddr_in)) < 0) return -1;

    ks->src_addr.sin_family = src->sin_family;
    ks->src_addr.sin_port = src->sin_port;
    ks->src_addr.sin_addr.s_addr = src->sin_addr.s_addr;
    memset(ks->src_addr.sin_zero, 0, sizeof(ks->src_addr.sin_zero));

    ks->dest_addr.sin_family = dest->sin_family;
    ks->dest_addr.sin_port = dest->sin_port;
    ks->dest_addr.sin_addr.s_addr = dest->sin_addr.s_addr;
    memset(ks->dest_addr.sin_zero, 0, sizeof(ks->dest_addr.sin_zero));

    ks->bound = 1;
    return 0;
}


ssize_t k_sendto(int ktp_fd, const void *buf, size_t len, int flags, const struct sockaddr_in *dest, socklen_t addrlen)
{
    int idx = ktp_fd_to_index(ktp_fd);
    if (idx < 0) { errno = EBADF; return -1;}

    ktp_socket_t *ks = &SM[idx];

    if (!ks->bound){ ktp_errno = KTP_ENOTBOUND; return -1; }
    if (dest->sin_addr.s_addr != ks->dest_addr.sin_addr.s_addr) { ktp_errno = KTP_ENOTBOUND; return -1; }
    if (dest->sin_port != ks->dest_addr.sin_port) { ktp_errno = KTP_ENOTBOUND; return -1; }
    if (ks->send_count >= KTP_BUFFER_SIZE) { ktp_errno = KTP_ENOSPACE; return -1; }
    if (len != MESSAGE_SIZE) { errno = EMSGSIZE; return -1; }

    for (size_t i = 0; i < MESSAGE_SIZE; i++) ks->send_buffer[ks->send_count][i] = ((char *)buf)[i];
    ks->send_count++;

    return (ssize_t)MESSAGE_SIZE;
}


ssize_t k_recvfrom(int ktp_fd, void *buf, size_t len, int flags, struct sockaddr_in *src, socklen_t *addrlen)
{
    int idx = ktp_fd_to_index(ktp_fd);
    if (idx < 0) { errno = EBADF; return -1; }

    ktp_socket_t *ks = &SM[idx];
    if (ks->recv_count == 0) { ktp_errno = KTP_ENOMESSAGE; return -1; }
    if (len < MESSAGE_SIZE) { errno = EMSGSIZE; return -1; }

    for (size_t i = 0; i < MESSAGE_SIZE; i++) ((char *)buf)[i] = ks->recv_buffer[0][i];
    if (src && addrlen && *addrlen >= sizeof(struct sockaddr_in)) memcpy(src, &ks->src_addr, sizeof(struct sockaddr_in));
    for (int i = 1; i < ks->recv_count; i++) memcpy(ks->recv_buffer[i - 1], ks->recv_buffer[i], MESSAGE_SIZE);
    ks->recv_count--;

    return (ssize_t)MESSAGE_SIZE;
}

int k_close(int ktp_fd)
{
    int idx = ktp_fd_to_index(ktp_fd);
    if (idx < 0) { errno = EBADF; return -1; }
    ktp_socket_t *ks = &SM[idx];

    close(ks->udp_fd);
    memset(ks, 0, sizeof(ktp_socket_t));
    return 0;
}
