#include <netinet/in.h>
#include <stdbool.h>
#include <sodium.h>

#ifndef CRYPTO_KEYS_H

#define CRYPTO_PUBKEY_BYTES crypto_kx_PUBLICKEYBYTES

typedef struct {
    uint8_t public_key[crypto_kx_PUBLICKEYBYTES];
    uint8_t private_key[crypto_kx_SECRETKEYBYTES];
} crypto_keypair;

typedef struct {
    unsigned char rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx[crypto_kx_SESSIONKEYBYTES];
} crypto_session_keys;

crypto_keypair crypto_keypair_make();

uint32_t crypto_pubkey_to_uid       (const unsigned char *pubkey);
bool     crypto_pubkey_and_uid_check(const unsigned char *pubkey, uint32_t uid);

int   crypto_keypair_store(const crypto_keypair *kp, const char *path);
int   crypto_keypair_load (crypto_keypair *kp, const char *path);

char* crypto_encode_pubkey_uid(const uint8_t pubkey[32], uint32_t uid);
int   crypto_decode_pubkey_uid(const char* b64_str, uint8_t out_pubkey[32], uint32_t* out_uid);

#endif
#define CRYPTO_KEYS_H