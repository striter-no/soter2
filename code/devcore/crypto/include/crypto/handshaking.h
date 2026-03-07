#include <sodium.h>
#include "keys.h"

#ifndef CRYPTO_HANDSHAKING_H

int crypto_handshake_server(
    const unsigned char *client_pubk,
    const crypto_keypair *kp, 
    crypto_session_keys  *sk
);

int crypto_handshake_client(
    const unsigned char *server_pubk,
    const crypto_keypair *kp,
    crypto_session_keys  *sk
);

#endif
#define CRYPTO_HANDSHAKING_H