#include "core.h"

#ifndef LOWNET_TCP_SOCK_H
#define LOWNET_TCP_SOCK_H

int  ln_tsock_new  (ln_socket *sck);
void ln_tsock_close(ln_socket *sck);
int  ln_tsock_bind (ln_socket *sck, naddr_t addr);

int ln_tsock_connect (ln_socket *sck, naddr_t addr);
int ln_tsock_connectx(ln_socket *sck, naddr_t addr, int timeout);

int  ln_tsock_listen(ln_socket *sck, int n);

// NOTICE: before calling set in `new_sock` addrlen!
int  ln_tsock_accept4(ln_socket *sck, ln_socket *new_sock);

ssize_t ln_tsock_read (ln_socket *sck, void *buf, size_t n);
ssize_t ln_tsock_write(ln_socket *sck, void *buf, size_t n);

ssize_t ln_tsock_readx (ln_socket *sck, size_t n, void *buf, int timeout_ms);
ssize_t ln_tsock_writex(ln_socket *sck, size_t n, void *buf, int timeout_ms);

#endif
