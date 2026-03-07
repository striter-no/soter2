#include <sodium.h>
#include "keys.h"
#include "signing.h"

#ifndef CRYPTO_SYSTEM_H

int crypto_init();

int crypto_encrypt(
    unsigned char *out_ciphertext,
    const unsigned char *raw_data,
    size_t raw_size,
    uint32_t seq,
    crypto_session_keys sk
);

int crypto_decrypt(
    unsigned char *out_raw,
    const unsigned char *ciphertext,
    size_t cipher_size,
    uint32_t seq,
    crypto_session_keys sk
);

#endif
#define CRYPTO_SYSTEM_H