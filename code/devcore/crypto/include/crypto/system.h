#include <sodium.h>
#include "keys.h"
#include "signing.h"
#include "handshaking.h"

#ifndef CRYPTO_SYSTEM_H

int crypto_init(void);

const char *crypto_fingerprint(const unsigned char pubkey[CRYPTO_PUBKEY_BYTES]);

int crypto_encrypt(
    unsigned char *out_ciphertext,
    const unsigned char *raw_data,
    size_t raw_size,
    uint64_t *nonce_counter,
    crypto_session_keys sk
);

int crypto_decrypt(
    unsigned char *out_raw,
    const unsigned char *ciphertext,
    size_t cipher_size,
    uint64_t *nonce_counter,
    crypto_session_keys sk
);

#endif
#define CRYPTO_SYSTEM_H