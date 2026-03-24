#include <e2ee/connection.h>
#include <sodium/crypto_aead_chacha20poly1305.h>
#include <sodium/randombytes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int e2ee_conn_init(
    e2ee_connection *conn,
    sign             my_signature,
    unsigned char    st_other_pubkey[CRYPTO_PUBKEY_BYTES]
){
    if (!conn || !st_other_pubkey) return -1;

    conn->hstate  = E2EE_NOT_HANDSHAKED;
    conn->hs_as   = crypto_pubkey_to_uid(my_signature.id_pub) < crypto_pubkey_to_uid(st_other_pubkey);
    conn->st_signature = my_signature;

    conn->kp = crypto_keypair_make();
    memcpy(conn->st_other_pubkey, st_other_pubkey, CRYPTO_PUBKEY_BYTES);

    conn->tx_nonce = 0;
    conn->rx_nonce = 0;

    return 0;
}

int e2ee_conn_load_keypair(
    e2ee_connection *conn,
    const char   *path
){
    if (!conn || !path) return -1;
    return crypto_keypair_load(&conn->kp, path);
}

int e2ee_conn_save_keypair(
    e2ee_connection *conn,
    const char   *path
){
    if (!conn || !path) return -1;
    return crypto_keypair_store(&conn->kp, path);
}

int e2ee_conn_upd_keypair(
    e2ee_connection *conn,
    const char   *path
){
    struct stat s;
    if (0 == stat(path, &s))
        return e2ee_conn_load_keypair(conn, path);
    else
        return e2ee_conn_save_keypair(conn, path);
}

void e2ee_conn_end(e2ee_connection *conn){
    memset(conn, 0, sizeof(*conn));
}

int e2ee_encrypt(
    e2ee_connection *conn,
    void *data,
    size_t d_size,

    unsigned char outbuf[UDP_MTU - PROTOCOL_OVERHEAD],
    size_t       *outsz
){
    if (!conn) return -1;
    if (conn->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][send] cannot use this function before full handshake\n");
        return -1;
    }

    size_t biggest_size = UDP_MTU - PROTOCOL_OVERHEAD - E2EE_OVERHEAD;
    if (d_size > biggest_size){
        fprintf(
            stderr,
            "[e2eee][send] data size exceeds %zu bytes (%zu bytes now)\n",
            biggest_size, d_size
        );
        return -1;
    }

    prot_array_lock(&conn->conn->pkts_fhost);
    unsigned char *ciphered = malloc(d_size + crypto_aead_chacha20poly1305_IETF_ABYTES);

    if (0 > crypto_encrypt(ciphered, data, d_size, &conn->tx_nonce, conn->skeys)){
        free(ciphered);
        fprintf(stderr, "[e2ee][send] failed to encrypt packet\n");
        prot_array_unlock(&conn->conn->pkts_fhost);
        return -1;
    }

    memcpy(outbuf, ciphered, d_size + crypto_aead_chacha20poly1305_IETF_ABYTES);
    *outsz = d_size + crypto_aead_chacha20poly1305_IETF_ABYTES;
    free(ciphered);

    prot_array_unlock(&conn->conn->pkts_fhost);
    return 0;
}

int e2ee_decrypt(
    e2ee_connection *conn,

    const unsigned char inpbuf[UDP_MTU - PROTOCOL_OVERHEAD],
    size_t insz,

    unsigned char outbuf[UDP_MTU - PROTOCOL_OVERHEAD - E2EE_OVERHEAD],
    size_t *outsize
){
    if (!conn) return -1;
    if (conn->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][recv] cannot use this function before full handshake\n");
        return -1;
    }

    if (insz < crypto_aead_chacha20poly1305_IETF_ABYTES){
        fprintf(stderr, "[e2ee][recv] d_size too small\n");
        return -1;
    }

    unsigned char *decrypted = malloc(insz - crypto_aead_chacha20poly1305_IETF_ABYTES);
    if (0 > crypto_decrypt(decrypted, inpbuf, insz, &conn->rx_nonce, conn->skeys)){
        free(decrypted);
        fprintf(stderr, "[e2ee][recv] failed to decypher message\n");
        return -1;
    }

    *outsize = insz - crypto_aead_chacha20poly1305_IETF_ABYTES;
    memcpy(outbuf, decrypted, *outsize);

    free(decrypted);
    return 0;
}

int e2ee_conn_handshake_init(
    e2ee_connection *conn,
    unsigned char outbuf[sizeof(e2ee_handshake)]
){
    if (!conn) return -1;

    e2ee_handshake hs;
    memcpy(hs.x25519_pubkey, conn->kp.public_key, CRYPTO_PUBKEY_BYTES);
    hs.nonce = randombytes_random();

    unsigned char signature_payload[CRYPTO_PUBKEY_BYTES + sizeof(uint32_t)] = {0};
    memcpy(signature_payload, &hs.nonce, sizeof(hs.nonce));
    memcpy(signature_payload + sizeof hs.nonce, hs.x25519_pubkey, CRYPTO_PUBKEY_BYTES);
    sign_data(&conn->st_signature, signature_payload, sizeof(signature_payload), hs.signature);

    memcpy(outbuf, &hs, sizeof(hs));
    conn->hstate = E2EE_HANDSHAKE_SENT;
    return 0;
}

int e2ee_conn_handshake_resp(
    e2ee_connection *conn,
    unsigned char buf[sizeof(e2ee_handshake)]
){
    if (!conn) return -1;

    e2ee_handshake hs;
    memcpy(&hs, buf, sizeof(hs));

    unsigned char signature_payload[CRYPTO_PUBKEY_BYTES + sizeof(uint32_t)] = {0};
    memcpy(signature_payload, &hs.nonce, sizeof(hs.nonce));
    memcpy(signature_payload + sizeof hs.nonce, hs.x25519_pubkey, CRYPTO_PUBKEY_BYTES);

    if (0 > sign_verify(signature_payload, sizeof(signature_payload), hs.signature, conn->st_other_pubkey)){
        fprintf(stderr, "[e2ee][hs] resp failed, handshake signature cannot be verified\n");
        return -1;
    }

    if (conn->hs_as){
        if (0 > crypto_handshake_server(hs.x25519_pubkey, &conn->kp, &conn->skeys)){
            fprintf(stderr, "[e2ee][hs] resp failed, handshake as server failed\n");
            return -1;
        }
    } else {
        if (0 > crypto_handshake_client(hs.x25519_pubkey, &conn->kp, &conn->skeys) ){
            fprintf(stderr, "[e2ee][hs] resp failed, handshake as client failed\n");
            return -1;
        }
    }

    conn->hstate = E2EE_HANDSHAKE_ENDED;
    return 0;
}
