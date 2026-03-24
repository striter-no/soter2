#include <rudp/connection.h>
#include <rudp/dispatcher.h>
#include <crypto/system.h>

#ifndef E2EE_CONNECTION_H
#define E2EE_CONNECTION_H

#define E2EE_OVERHEAD crypto_aead_chacha20poly1305_IETF_ABYTES

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
    sign          st_signature;
    unsigned char st_other_pubkey[CRYPTO_PUBKEY_BYTES];

    crypto_session_keys skeys;
    crypto_keypair      kp;
    e2ee_hs_state       hstate;
    int                 hs_as;

    uint64_t tx_nonce;
    uint64_t rx_nonce;
} e2ee_connection;

void e2ee_conn_end(e2ee_connection *conn);

int e2ee_conn_init(
    e2ee_connection *conn,
    sign             my_signature,
    unsigned char    st_other_pubkey[CRYPTO_PUBKEY_BYTES]
);

int e2ee_conn_load_keypair(
    e2ee_connection *conn,
    const char      *path
);

int e2ee_conn_save_keypair(
    e2ee_connection *conn,
    const char      *path
);

int e2ee_conn_upd_keypair(
    e2ee_connection *conn,
    const char      *path
);

int e2ee_encrypt(
    e2ee_connection *conn,
    void *data,
    size_t d_size,

    unsigned char outbuf[UDP_MTU - PROTOCOL_OVERHEAD],
    size_t       *outsz
);

int e2ee_decrypt(
    e2ee_connection *conn,

    const unsigned char inpbuf[UDP_MTU - PROTOCOL_OVERHEAD],
    size_t insz,

    unsigned char outbuf[UDP_MTU - PROTOCOL_OVERHEAD - E2EE_OVERHEAD],
    size_t *outsize
);

int e2ee_conn_handshake_init(
    e2ee_connection *conn,
    unsigned char outbuf[sizeof(e2ee_handshake)]
);

int e2ee_conn_handshake_resp(
    e2ee_connection *conn,
    unsigned char buf[sizeof(e2ee_handshake)]
);

#endif
