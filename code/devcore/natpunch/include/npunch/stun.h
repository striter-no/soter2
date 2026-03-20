#include <lownet/udp_sock.h>

#ifndef NATPUNCH_STUN_H

#define ATTR_MAPPED_ADDRESS        0x0001
#define ATTR_XOR_MAPPED_ADDRESS    0x0020
#define ATTR_XOR_MAPPED_ADDRESS_RFC5389 0x8020

#ifndef STUN_PARALLEL_RESOLUTION_LIMIT
#define STUN_PARALLEL_RESOLUTION_LIMIT 10
#endif

typedef struct {
    const char     uniaddr[253];
    unsigned short port;
} stun_addr;

int natp_request_stun(
    ln_usocket *client,
    naddr_t  *stun_addr
);

#endif
#define NATPUNCH_STUN_H