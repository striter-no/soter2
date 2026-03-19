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
    rudp_connection *real, 
    sign             my_signature, 
    unsigned char    st_other_pubkey[CRYPTO_PUBKEY_BYTES]
){
    if (!conn || !real) return -1;

    conn->conn    = real;
    conn->hstate  = E2EE_NOT_HANDSHAKED;
    conn->hs_as   = real->c_uid < real->s_uid;
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
    (void)conn; // idk, nothing to do
}


int e2ee_send(e2ee_connection *conn, void *data, size_t d_size){
    if (!conn) return -1;
    if (conn->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][send] cannot use this function before full handshake\n");
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

    protopack *copy = udp_make_pack(
        0, 
        conn->conn->s_uid,
        conn->conn->c_uid,
        PACK_DATA,
        ciphered,
        d_size + crypto_aead_chacha20poly1305_IETF_ABYTES
    );
    prot_array_unlock(&conn->conn->pkts_fhost);

    int r = rudp_conn_send(conn->conn, copy);
    // prot_array_unlock(&conn->conn->pkts_fhost);

    free(ciphered);
    free(copy);
    return r;
}

protopack *e2ee_recv(e2ee_connection *conn){
    if (!conn) return NULL;
    if (conn->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][recv] cannot use this function before full handshake\n");
        return NULL;
    }

    protopack *r = NULL;
    if (0 > rudp_conn_recv(conn->conn, &r)){
        // fprintf(stderr, "[e2ee][recv] failed to recv raw bytes\n");
        return NULL; // forward message
    }

    if (r->d_size < crypto_aead_chacha20poly1305_IETF_ABYTES){
        fprintf(stderr, "[e2ee][recv] d_size too small\n");
        free(r);
        return NULL;
    }

    unsigned char *decrypted = malloc(r->d_size - crypto_aead_chacha20poly1305_IETF_ABYTES);
    if (0 > crypto_decrypt(decrypted, r->data, r->d_size, &conn->rx_nonce, conn->skeys)){
        free(r);
        free(decrypted);
        fprintf(stderr, "[e2ee][recv] failed to decypher message\n");
        return NULL;
    }

    protopack *out = udp_make_pack(
        r->seq, r->h_from, r->h_to, r->packtype, 
        decrypted, 
        r->d_size - crypto_aead_chacha20poly1305_IETF_ABYTES
    );

    free(decrypted);
    free(r);
    return out;
}

int e2ee_wait(e2ee_connection *conn, int timeout){
    if (!conn) return -1;
    if (conn->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][wait] cannot use this function before full handshake\n");
        return -1;
    }

    return rudp_conn_wait(conn->conn, timeout);
}


int e2ee_conn_handshake_init(e2ee_connection *conn){
    if (!conn) return -1;

    e2ee_handshake hs;
    memcpy(hs.x25519_pubkey, conn->kp.public_key, CRYPTO_PUBKEY_BYTES);
    hs.nonce = randombytes_random();
    
    unsigned char signature_payload[CRYPTO_PUBKEY_BYTES + sizeof(uint32_t)] = {0};
    memcpy(signature_payload, &hs.nonce, sizeof(hs.nonce));
    memcpy(signature_payload + sizeof hs.nonce, hs.x25519_pubkey, CRYPTO_PUBKEY_BYTES);
    sign_data(&conn->st_signature, signature_payload, sizeof(signature_payload), hs.signature);
    
    protopack *p = udp_make_pack(
        0, // will auto-set in rudp_conn_send 
        conn->conn->s_uid, 
        conn->conn->c_uid, 
        PACK_DATA, &hs, sizeof(hs)
    );

    int r = rudp_conn_send(conn->conn, p);
    free(p);

    if (r == 0)
        conn->hstate = E2EE_HANDSHAKE_SENT;
    return r;
}

int e2ee_conn_handshake_wait(e2ee_connection *conn, int timeout){
    if (!conn) return -1;
    
    return rudp_conn_wait(conn->conn, timeout);
}

int e2ee_conn_handshake_resp(e2ee_connection *conn){
    if (!conn) return -1;

    protopack *p = NULL;
    if (0 > rudp_conn_recv(conn->conn, &p))
        return -1;

    if (p->d_size != sizeof(e2ee_handshake)){
        fprintf(stderr, "[e2ee][hs] resp failed, dsize is not sizeof(e2ee_handshake): %u\n", p->d_size);
        free(p);
        return -1;
    }

    e2ee_handshake hs;
    memcpy(&hs, p->data, sizeof(hs));
    free(p);

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
