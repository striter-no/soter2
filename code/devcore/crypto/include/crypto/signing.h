#include <sodium.h>

#ifndef CRYPTO_SIGNING_H

#define SIGN_BYTES crypto_sign_BYTES

typedef struct {
    unsigned char id_pub[crypto_sign_PUBLICKEYBYTES];
    unsigned char id_sec[crypto_sign_SECRETKEYBYTES];
} sign;

sign sign_gen();

int sign_data(
    sign *sgn,
    unsigned char *msg_to_sign,
    size_t         msg_len,
    unsigned char  out_signature[crypto_sign_BYTES]
);

int sign_verify(
    unsigned char *msg_signed,
    size_t         msg_len,
    unsigned char  in_signature[crypto_sign_BYTES],
    unsigned char  pub_key     [crypto_sign_PUBLICKEYBYTES]
);

int sign_store(const sign *kp, const char *path);
int sign_load(sign *kp, const char *path);

#endif
#define CRYPTO_SIGNING_H