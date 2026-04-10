#ifndef PROXY_SOCKS5_H
#define PROXY_SOCKS5_H

#include <arpa/inet.h>
#include <lownet/tcp_sock.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/poll.h>
#include <unistd.h>


typedef struct {
    char           domain[UINT8_MAX + 1];
    unsigned short port;
    int            domain_len;
} _s5_domain_address;

typedef enum {
    _S5_DOMAIN_ADDR,
    _S5_IP_ADDR
} _s5_addr_type;

typedef struct {
    union {
        _s5_domain_address domain;
        naddr_t            ip;
    } uni;
    _s5_addr_type type;
} s5_uni_addr;

int prx_socks5_handle_request(ln_socket *sock, s5_uni_addr *out_addr);

// * resolves domain-typed addresses
// * performs connection to the address, returns connected socket
int prx_resolve_address(s5_uni_addr address, ln_socket *sock);

// * blocking `while(working)`
// * spawns detached threads with `tunnel_loop`
// * place it into the task if you have any logic
//   after accept loop and outside the connection logic
int prx_socks5_accept_loop(
    ln_socket *proxy,
    int (*tunnel_iter)(ln_socket* client, ln_socket* remote, int who, void *state_holder),
    void (*connection_handler)(ln_socket* client, s5_uni_addr rem_address, ln_socket* output_socket, void *state_holder),
    void *state_holder,
    atomic_bool *is_active
);

#endif
