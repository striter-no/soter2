#include <crypto/system.h>
#include <string.h>

int crypto_init(){
    return sodium_init();
}

int crypto_encrypt(
    unsigned char *out_ciphertext,
    const unsigned char *raw_data,
    size_t raw_size,
    uint32_t seq,
    crypto_session_keys sk
){
    unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES] = {0};
    memcpy(nonce, &seq, sizeof(seq));
    
    unsigned long long out_len;
    return crypto_aead_chacha20poly1305_ietf_encrypt(
        out_ciphertext, &out_len,
        raw_data, raw_size,
        NULL, 0,
        NULL, nonce, sk.tx
    );
}

int crypto_decrypt(
    unsigned char *out_raw,
    const unsigned char *ciphertext,
    size_t cipher_size,
    uint32_t seq,
    crypto_session_keys sk
){
    if (cipher_size < crypto_aead_chacha20poly1305_ietf_ABYTES) return -1;

    unsigned char nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES] = {0};
    memcpy(nonce, &seq, sizeof(seq));

    unsigned long long out_len;
    return crypto_aead_chacha20poly1305_ietf_decrypt(
        out_raw, &out_len,
        NULL,
        ciphertext, cipher_size,
        NULL, 0,
        nonce, sk.rx
    );
}