#include "core.h"

#ifndef LOWNET_UDP_SOCK_H
typedef struct {
    nnet_fd  fd;
    naddr_t  addr;
} ln_usocket;

int ln_usock_new(ln_usocket *cli);
void ln_usock_close(ln_usocket *cli);
int ln_usock_bind(ln_usocket *cli, naddr_t addr);

ssize_t ln_usock_recv(ln_usocket *cli, void *buf, size_t n, nnet_fd *from);
ssize_t ln_usock_send(ln_usocket *cli, const void *buf, size_t n, const nnet_fd *to);
#endif
#define LOWNET_UDP_SOCK_H