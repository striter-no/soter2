#include "core.h"

#ifndef LOWNET_UDP_SOCK_H
#define LOWNET_UDP_SOCK_H

int  ln_usock_new  (ln_socket *cli);
void ln_usock_close(ln_socket *cli);
int  ln_usock_bind (ln_socket *cli, naddr_t addr);

ssize_t ln_usock_recv(ln_socket *cli, void *buf, size_t n, nnet_fd *from);
ssize_t ln_usock_send(ln_socket *cli, const void *buf, size_t n, const nnet_fd *to);

#endif
