#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef LOWNET_CORE_H
typedef struct {
    char     ip[INET_ADDRSTRLEN];
    uint16_t port;
} naddr_ipv4;

typedef struct {
    char     ip[INET6_ADDRSTRLEN];
    uint16_t port;
} naddr_ipv6;

typedef enum {
    nADDR_IPV4,
    nADDR_IPV6
} naddr_type;

typedef struct {
    union {
        naddr_ipv4 v4;
        naddr_ipv6 v6;
    } ip;
    naddr_type t;
} naddr_t;

typedef struct {
    int rfd;
    struct sockaddr_storage addr;
    socklen_t               addr_len;
} nnet_fd;

typedef struct {
    nnet_fd  fd;
    naddr_t  addr;
} ln_socket;

naddr_ipv4 ln_ipv4(const char *ip, uint16_t port);
naddr_ipv6 ln_ipv6(const char *ip, uint16_t port);
naddr_t ln_make4(naddr_ipv4 ipv4);
naddr_t ln_make6(naddr_ipv6 ipv6);
naddr_t ln_uniq(const char *uni_addr, unsigned short port);
int ln_uni(const char *uni_addr, unsigned short port, naddr_t *out);

const char *ln_gip(const naddr_t *addr);
uint16_t ln_gport(const naddr_t *addr);

// NOTICE: port IS set to 0 after operation
int ln_resolve(const char *domain, naddr_t *output);

// NOTICE: fd IS NOT set, only address
int ln_netfd(naddr_t *addr, nnet_fd *out);

naddr_t  ln_nfd2addr(const nnet_fd *fd);
nnet_fd  ln_netfdq(naddr_t *addr);
naddr_t  ln_resolveq(const char *domain, unsigned port);

naddr_t  ln_hton(const naddr_t *addr);
naddr_t  ln_ntoh(const naddr_t *addr);

naddr_t  ln_from_uint32(uint32_t ip_bin, uint16_t port);
uint32_t ln_to_uint32(naddr_t *addr);

int      ln_wait_netfd(nnet_fd *fd, int events, int timeout);
uint32_t ln_nfd2hash(const nnet_fd *fd);

bool     ln_addrcmp(naddr_t *a, naddr_t *b);
int      ln_setopt(ln_socket *sock, int LEVEL, int OPT_NAME, int value);

#endif
#define LOWNET_CORE_H
