#include <e2ee/channels.h>
#include <sodium/crypto_aead_chacha20poly1305.h>
#include <sodium/randombytes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int e2ee_chan_init(
    e2ee_channel    *chan,
    rudp_dispatcher *disp,
    rudp_channel    *real, 
    sign             my_signature, 
    unsigned char    st_other_pubkey[CRYPTO_PUBKEY_BYTES]
){
    if (!chan || !real) return -1;

    chan->disp    = disp;
    chan->channel = real;
    chan->hstate  = E2EE_NOT_HANDSHAKED;
    chan->hs_as   = real->client_uid < real->self_uid;
    chan->st_signature = my_signature;

    chan->kp = crypto_keypair_make();
    memcpy(chan->st_other_pubkey, st_other_pubkey, CRYPTO_PUBKEY_BYTES);

    return 0;
}

int e2ee_chan_load_keypair(
    e2ee_channel *chan,
    const char   *path
){
    if (!chan || !path) return -1;
    return crypto_keypair_load(&chan->kp, path);
}

int e2ee_chan_save_keypair(
    e2ee_channel *chan,
    const char   *path
){
    if (!chan || !path) return -1;
    return crypto_keypair_store(&chan->kp, path);
}

int e2ee_chan_upd_keypair(
    e2ee_channel *chan,
    const char   *path
){
    struct stat s;
    if (0 == stat(path, &s))
        return e2ee_chan_load_keypair(chan, path);
    else
        return e2ee_chan_save_keypair(chan, path);
}

void e2ee_chan_end(e2ee_channel *chan){
    (void)chan; // idk, nothing to do
}


int e2ee_send(e2ee_channel *chan, void *data, size_t d_size){
    if (!chan) return -1;
    if (chan->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][send] cannot use this function before full handshake\n");
        return -1;
    }

    // reformate data, chsum is not touched, calculated in sender
    unsigned char *ciphered = malloc(d_size + crypto_aead_chacha20poly1305_IETF_ABYTES);
    if (0 > crypto_encrypt(ciphered, data, d_size, chan->channel->next_seq, chan->skeys)){
        free(ciphered);

        fprintf(stderr, "[e2ee][send] failed to encrypt packet\n");
        return -1;
    }
    
    protopack *copy = udp_make_pack(
        chan->channel->next_seq, chan->channel->self_uid, chan->channel->client_uid, PACK_DATA, ciphered, d_size + crypto_aead_chacha20poly1305_IETF_ABYTES
    );

    int r = rudp_direct_send(chan->disp, chan->channel, copy);
    free(ciphered);
    free(copy);
    return r;
}

protopack *e2ee_recv(e2ee_channel *chan){
    if (!chan) return NULL;
    if (chan->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][recv] cannot use this function before full handshake\n");
        return NULL;
    }

    protopack *r = NULL;
    if (0 > rudp_channel_recv(chan->channel, &r))
        return NULL;

    if (r->d_size < crypto_aead_chacha20poly1305_IETF_ABYTES)
        return NULL;

    // decrypt data
    unsigned char *decrypted = malloc(r->d_size - crypto_aead_chacha20poly1305_IETF_ABYTES);
    if (0 > crypto_decrypt(decrypted, r->data, r->d_size, r->seq, chan->skeys)){
        free(r);
        
        fprintf(stderr, "[e2ee][recv] failed to decypher message\n");
        return NULL;
    }

    protopack *out = udp_make_pack(
        r->seq, r->h_from, r->h_to, r->packtype, decrypted, r->d_size - crypto_aead_chacha20poly1305_IETF_ABYTES
    );

    free(decrypted);
    free(r);
    return out;
}

int e2ee_wait(e2ee_channel *chan, int timeout){
    if (!chan) return -1;
    if (chan->hstate != E2EE_HANDSHAKE_ENDED) {
        fprintf(stderr, "[e2ee][wait] cannot use this function before full handshake\n");
        return -1;
    }

    return rudp_channel_wait(chan->channel, timeout);
}


int e2ee_chan_handshake_init(e2ee_channel *chan){
    if (!chan) return -1;

    e2ee_handshake hs;
    memcpy(hs.x25519_pubkey, chan->kp.public_key, CRYPTO_PUBKEY_BYTES);
    hs.nonce = randombytes_random();
    
    unsigned char signature_payload[CRYPTO_PUBKEY_BYTES + sizeof(uint32_t)] = {0};
    memcpy(signature_payload, &hs.nonce, sizeof(hs.nonce));
    memcpy(signature_payload + sizeof hs.nonce, hs.x25519_pubkey, CRYPTO_PUBKEY_BYTES);
    sign_data(&chan->st_signature, signature_payload, sizeof(signature_payload), hs.signature);
    
    protopack *p = udp_make_pack(
        chan->channel->next_seq, 
        chan->disp->self_uid, 
        chan->channel->client_uid, 
        PACK_DATA, &hs, sizeof(hs)
    );

    int r = rudp_dispatcher_send(chan->disp, p, &chan->channel->client_nfd);
    free(p);

    if (r == 0)
        chan->hstate = E2EE_HANDSHAKE_SENT;
    return r;
}

int e2ee_chan_handshake_wait(e2ee_channel *chan, int timeout){
    if (!chan) return -1;
    
    return rudp_channel_wait(chan->channel, timeout);
}

int e2ee_chan_handshake_resp(e2ee_channel *chan){
    if (!chan) return -1;

    protopack *p = NULL;
    if (0 > rudp_channel_recv(chan->channel, &p))
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

    if (0 > sign_verify(signature_payload, sizeof(signature_payload), hs.signature, chan->st_other_pubkey)){
        fprintf(stderr, "[e2ee][hs] resp failed, handshake signature cannot be verified\n");
        return -1;
    }

    if (chan->hs_as){
        if (0 > crypto_handshake_server(hs.x25519_pubkey, &chan->kp, &chan->skeys)){
            fprintf(stderr, "[e2ee][hs] resp failed, handshake as server failed\n");
            return -1;
        }
    } else {
        if (0 > crypto_handshake_client(hs.x25519_pubkey, &chan->kp, &chan->skeys) ){
            fprintf(stderr, "[e2ee][hs] resp failed, handshake as client failed\n");
            return -1;
        }
    }

    chan->hstate = E2EE_HANDSHAKE_ENDED;
    return 0;
}
