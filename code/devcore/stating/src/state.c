#include <stating/state.h>
#include <string.h>

state_peer state_info2peer(
    naddr_t addr, 
    uint32_t uid,
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES]
){
    state_peer state = {
        .ip   = ln_to_uint32(addr),
        .port = addr.ip.v4.port,
        .uid  = uid
    };
    memcpy(state.pubkey, pubkey, CRYPTO_PUBKEY_BYTES);

    return state;
}

void state_peer2info(
    state_peer peer, 
    naddr_t *addr, 
    uint32_t *uid, 
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES]
){
    *addr = ln_from_uint32(peer.ip, peer.port);
    *uid  = peer.uid;
    if (pubkey)
        memcpy(pubkey, peer.pubkey, CRYPTO_PUBKEY_BYTES);
}