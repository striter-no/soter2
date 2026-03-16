#include <rudp/channels.h>
#include <rudp/dispatcher.h>
#include <crypto/system.h>

#ifndef E2EE_CHANNELS_H

typedef enum {
    E2EE_NOT_HANDSHAKED,
    E2EE_HANDSHAKE_SENT,
    E2EE_HANDSHAKE_ENDED
} e2ee_hs_state;

typedef struct __attribute__((packed)) {
    uint32_t      nonce;
    
    unsigned char x25519_pubkey[CRYPTO_PUBKEY_BYTES];
    unsigned char signature[CRYPTO_SIGN_BYTES];
} e2ee_handshake;

typedef struct {
    rudp_channel    *channel;
    rudp_dispatcher *disp;

    sign          st_signature;
    unsigned char st_other_pubkey[CRYPTO_PUBKEY_BYTES];

    crypto_session_keys skeys;
    crypto_keypair      kp;
    e2ee_hs_state       hstate;
    int                 hs_as;
} e2ee_channel;

int e2ee_chan_init(
    e2ee_channel    *chan,
    rudp_dispatcher *disp,
    rudp_channel    *real, 
    sign             my_signature, 
    unsigned char    st_other_pubkey[CRYPTO_PUBKEY_BYTES]
);

int e2ee_chan_load_keypair(
    e2ee_channel *chan,
    const char   *path
);

int e2ee_chan_save_keypair(
    e2ee_channel *chan,
    const char   *path
);

int e2ee_chan_upd_keypair(
    e2ee_channel *chan,
    const char   *path
);

void e2ee_chan_end(e2ee_channel *chan);

protopack *e2ee_recv(e2ee_channel *chan);
int e2ee_send(e2ee_channel *chan, void *data, size_t d_size);
int e2ee_wait(e2ee_channel *chan, int timeout);

int e2ee_chan_handshake_init(e2ee_channel *chan);
int e2ee_chan_handshake_wait(e2ee_channel *chan, int timeout);
int e2ee_chan_handshake_resp(e2ee_channel *chan);

#endif
#define E2EE_CHANNELS_H