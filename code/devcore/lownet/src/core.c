#include <lownet/core.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/poll.h>

naddr_ipv4 ln_ipv4(const char *ip, uint16_t port){
    naddr_ipv4 addr = {0};
    strcpy(addr.ip, ip);
    addr.port = port;
    return addr;
}

naddr_ipv6 ln_ipv6(const char *ip, uint16_t port){
    naddr_ipv6 addr = {0};
    strcpy(addr.ip, ip);
    addr.port = port;
    return addr;
}

naddr_t ln_make4(naddr_ipv4 ipv4){
    return (naddr_t){
        .ip.v4 = ipv4,
        .t = nADDR_IPV4
    };
}

naddr_t ln_make6(naddr_ipv6 ipv6){
    return (naddr_t){
        .ip.v6 = ipv6,
        .t = nADDR_IPV6
    };
}

// 1.2.3.4 -> ln_make4(ln_ipv4(ip, port))
// abcd::0efg:1234 -> ln_make6(ln_ipv6(ip, port))
// abcd.com -> ln_resolve(addr, port)
int ln_uni(const char *uni_addr, unsigned short port, naddr_t *out){
    if (!uni_addr || !out) return -1;

    struct in_addr test4;
    if (inet_pton(AF_INET, uni_addr, &test4) == 1) {
        *out = ln_make4(ln_ipv4(uni_addr, port));
        return 0;
    }

    struct in6_addr test6;
    if (inet_pton(AF_INET6, uni_addr, &test6) == 1) {
        *out = ln_make6(ln_ipv6(uni_addr, port));
        return 0;
    }

    if (ln_resolve(uni_addr, out) == 0) {
        if (out->t == nADDR_IPV4) {
            out->ip.v4.port = port;
        } else if (out->t == nADDR_IPV6) {
            out->ip.v6.port = port;
        }
        return 0;
    }

    return -1;
}

const char *ln_gip(const naddr_t *addr){
    switch (addr->t) {
        case nADDR_IPV4: return addr->ip.v4.ip;
        case nADDR_IPV6: return addr->ip.v6.ip;
    }

    return "";
}

uint16_t ln_gport(const naddr_t *addr){
    switch (addr->t) {
        case nADDR_IPV4: return addr->ip.v4.port;
        case nADDR_IPV6: return addr->ip.v6.port;
    }

    return 0;
}

naddr_t ln_uniq(const char *uni_addr, unsigned short port){
    if (!uni_addr) return (naddr_t){0};

    naddr_t out;
    if (0 > ln_uni(uni_addr, port, &out)) return (naddr_t){0};
    return out;
}


bool ln_addrcmp(naddr_t *a, naddr_t *b){
    if (a->t != b->t) return false;
    if (a->t == nADDR_IPV4){
        return a->ip.v4.port == b->ip.v4.port && (strcmp(a->ip.v4.ip, b->ip.v4.ip) == 0);
    }

    return a->ip.v6.port == b->ip.v6.port && (strcmp(a->ip.v6.ip, b->ip.v6.ip) == 0);
}

// NOTICE: port IS NOT set (== 0) after operation
int ln_resolve(const char *domain, naddr_t *output){
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if ( (he = gethostbyname( domain ) ) == NULL) {
        herror("gethostbyname");
        return -1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    char ip[INET6_ADDRSTRLEN] = {0};
    for(i = 0; addr_list[i] != NULL; i++){
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        break;
    }

    if (ip[0] == '\0') return -1;

    // is IPv6?
    if (strchr(ip, ':') != NULL){
        output->t = nADDR_IPV6;
        strncpy(output->ip.v6.ip, ip, INET6_ADDRSTRLEN);
        output->ip.v6.port = 0;
        return 0;
    }

    output->t = nADDR_IPV4;
    strncpy(output->ip.v4.ip, ip, INET_ADDRSTRLEN);
    output->ip.v4.port = 0;

    return 0;
}

// NOTICE: fd IS NOT set, only address
int ln_netfd(naddr_t *addr, nnet_fd *out){
    switch (addr->t){
        case nADDR_IPV4:{
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&out->addr;
            if (inet_pton(AF_INET, addr->ip.v4.ip, &(ipv4->sin_addr)) == 1) {
                ipv4->sin_family = AF_INET;
                ipv4->sin_port = htons(addr->ip.v4.port);
                out->addr_len = sizeof(struct sockaddr_in);
                return 0;
            }
            return -1;
        } break;

        case nADDR_IPV6:{
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&out->addr;
            if (inet_pton(AF_INET6, addr->ip.v6.ip, &(ipv6->sin6_addr)) == 1) {
                ipv6->sin6_family = AF_INET6;
                ipv6->sin6_port = htons(addr->ip.v6.port);
                out->addr_len = sizeof(struct sockaddr_in6);
                return 0;
            }
            return -1;
        } break;

        default: {
            return -1;
        }
    }

    return 0;
}

naddr_t ln_resolveq(const char *domain, unsigned int port){
    naddr_t addr;
    if (0 > ln_resolve(domain, &addr))
        return (naddr_t){0};

    addr.ip.v4.port = port;
    return addr;
}

naddr_t ln_nfd2addr(const nnet_fd *fd){
    static char str[INET6_ADDRSTRLEN];

    const struct sockaddr_storage *addr = &fd->addr;

    if (addr->ss_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), str, INET_ADDRSTRLEN);
        return ln_make4(ln_ipv4(
            str, ntohs(ipv4->sin_port)
        ));
    }
    else if (addr->ss_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)addr;
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), str, INET6_ADDRSTRLEN);
        return ln_make6(ln_ipv6(
            str, ntohs(ipv6->sin6_port)
        ));
    }

    fprintf(stderr, "[lownet] nfd2addr failed, ss_family is UNKNOWN: %d\n", addr->ss_family);
    return (naddr_t){0};
}

nnet_fd ln_netfdq(naddr_t *addr){
    nnet_fd out;
    memset(&out, 0, sizeof(out));
    if (0 > ln_netfd(addr, &out)) return (nnet_fd){.rfd = -1, .addr = {0}, .addr_len = 0};
    return out;
}

naddr_t ln_domain(const char *domain, unsigned port){
    naddr_t output;
    if (0 > ln_resolve(domain, &output)) return (naddr_t){0};
    if (output.t == nADDR_IPV4)
        output.ip.v4.port = port;
    else
        output.ip.v6.port = port;

    return output;
}

int ln_wait_netfd(nnet_fd *fd, int events, int timeout){
    struct pollfd fds[1] = {{.fd = fd->rfd, .events = events}};
    return poll(fds, 1, timeout);
}

naddr_t ln_from_uint32(uint32_t ip_bin, uint16_t port) {
    naddr_t addr;
    memset(&addr, 0, sizeof(addr));

    addr.t = nADDR_IPV4;
    addr.ip.v4.port = port;

    if (inet_ntop(AF_INET, &ip_bin, addr.ip.v4.ip, INET_ADDRSTRLEN) == NULL) {
        addr.ip.v4.ip[0] = '\0';
    }

    return addr;
}

uint32_t ln_to_uint32(naddr_t *addr) {
    if (addr->t != nADDR_IPV4) return 0;

    uint32_t ip_bin;
    if (inet_pton(AF_INET, addr->ip.v4.ip, &ip_bin) <= 0) {
        return 0;
    }

    return ip_bin;
}

naddr_t ln_hton(const naddr_t *addr){
    switch (addr->t) {
        case nADDR_IPV4:
            return ln_make4(ln_ipv4(
                addr->ip.v4.ip,
                htons(addr->ip.v4.port)
            ));

        case nADDR_IPV6:
            return ln_make6(ln_ipv6(
                addr->ip.v6.ip,
                htons(addr->ip.v6.port)
            ));
    }

    return (naddr_t){0};
}

naddr_t ln_ntoh(const naddr_t *addr){
    switch (addr->t) {
        case nADDR_IPV4:
            return ln_make4(ln_ipv4(
                addr->ip.v4.ip,
                ntohs(addr->ip.v4.port)
            ));

        case nADDR_IPV6:
            return ln_make6(ln_ipv6(
                addr->ip.v6.ip,
                ntohs(addr->ip.v6.port)
            ));
    }

    return (naddr_t){0};
}


static uint32_t murmurhash3_32(const char* key, uint32_t len, uint32_t seed) {
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;
    uint32_t r1 = 15;
    uint32_t r2 = 13;
    uint32_t m = 5;
    uint32_t n = 0xe6546b64;
    uint32_t h = seed;

    const uint32_t* blocks = (const uint32_t*)(key);
    int nblocks = len / 4;

    for (int i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        h ^= k;
        h = (h << r2) | (h >> (32 - r2));
        h = h * m + n;
    }

    const uint8_t* tail = (const uint8_t*)(key + nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << r1) | (k1 >> (32 - r1));
                k1 *= c2;
                h ^= k1;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

uint32_t ln_nfd2hash(const nnet_fd *fd){
    if (!fd) return 0;

    const struct sockaddr_storage *ss = &fd->addr;
    char data[18];
    size_t data_len = 0;

    if (ss->ss_family == AF_INET) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)ss;
        memcpy(data, &sin->sin_addr.s_addr, sizeof(uint32_t));
        data_len = sizeof(uint32_t);
    }
    else if (ss->ss_family == AF_INET6) {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)ss;
        memcpy(data, &sin6->sin6_addr.s6_addr, sizeof(struct in6_addr));
        data_len = sizeof(struct in6_addr);
    }
    else {
        fprintf(stderr, "[lownet] nfd2hash: unknown family %d\n", ss->ss_family);
        return 0;
    }

    uint16_t port = 0;
    if (ss->ss_family == AF_INET) {
        port = ((const struct sockaddr_in *)ss)->sin_port;
    } else {
        port = ((const struct sockaddr_in6 *)ss)->sin6_port;
    }
    memcpy(data + data_len, &port, sizeof(uint16_t));
    data_len += sizeof(uint16_t);

    return murmurhash3_32(data, data_len, 0xDEADBEF);
}

int ln_setopt(ln_socket *sock, int LEVEL, int OPT_NAME, int value){
    if (!sock) return -1;
    return setsockopt(sock->fd.rfd, LEVEL, OPT_NAME, &value, sizeof(value));
}
