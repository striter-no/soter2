#include <lownet/udp_sock.h>
#include <crypto/system.h>

#ifndef STATESERV_DT
#define STATESERV_DT 2
#endif

#ifndef STATE_H

#pragma pack(push, 1)
typedef struct {
    uint32_t ip;
    uint16_t port;
    uint32_t uid;
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES];
} state_peer;
#pragma pack(pop)

// from net to peer
state_peer state_info2peer(
    naddr_t addr, 
    uint32_t uid,
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES]
);

void state_peer2info(
    state_peer peer, 
    naddr_t *addr, 
    uint32_t *uid, 
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES]
);

#endif
#define STATE_H