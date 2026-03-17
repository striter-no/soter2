#include <crypto/system.h>
#include <netinet/in.h>
#include <string.h>

int crypto_init(){
    return sodium_init();
}

int crypto_encrypt(
    unsigned char *out_ciphertext,
    const unsigned char *raw_data,
    size_t raw_size,
    uint64_t *nonce_counter,
    crypto_session_keys sk
){
    unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES] = {0};
    memcpy(nonce + 4, nonce_counter, sizeof(uint64_t));

    unsigned long long out_len;
    int r = crypto_aead_chacha20poly1305_ietf_encrypt(
        out_ciphertext, &out_len,
        raw_data, raw_size,
        NULL, 0,
        NULL, nonce, sk.tx
    );
    
    if (r == 0) (*nonce_counter)++;
    
    return r;
}

int crypto_decrypt(
    unsigned char *out_raw,
    const unsigned char *ciphertext,
    size_t cipher_size,
    uint64_t *nonce_counter,
    crypto_session_keys sk
){
    if (cipher_size < crypto_aead_chacha20poly1305_ietf_ABYTES) return -1;
    unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES] = {0};

    memcpy(nonce + 4, nonce_counter, sizeof(uint64_t));

    unsigned long long out_len;
    int r = crypto_aead_chacha20poly1305_ietf_decrypt(
        out_raw, &out_len,
        NULL,
        ciphertext, cipher_size,
        NULL, 0,
        nonce, sk.rx
    );

    if (r == 0) (*nonce_counter)++;
    return r;
}