#include <sodium/randombytes.h>
#include <stating/state.h>
#include <string.h>

int state_sys_init(state_system *sys){
    if (!sys) return -1;

    if (0 > mt_evsock_new(&sys->new_state_fd))
        return -1;

    prot_queue_create(sizeof(state_sys_request), &sys->new_state_ans);
    return 0;
}

void state_sys_end (state_system *sys){
    if (!sys) return;

    mt_evsock_close(&sys->new_state_fd);
    prot_queue_end(&sys->new_state_ans);
}

int state_sys_new_ans(state_system *sys, const state_request *req, naddr_t server_from){
    if (!sys || !req) return -1;

    state_sys_request sreq = {
        .req = *req,
        .come_from = server_from
    };

    prot_queue_upush(&sys->new_state_ans, &sreq);
    mt_evsock_notify(&sys->new_state_fd);
    return 0;
}

int state_sys_wait(state_system *sys, state_request *out, naddr_t *from, int timeout){
    if (!sys || !out) return -1;
    int r = mt_evsock_wait(&sys->new_state_fd, timeout);
    if (r <= 0) return r;

    state_sys_request sreq;
    mt_evsock_drain(&sys->new_state_fd);
    prot_queue_pop(&sys->new_state_ans, &sreq);

    *out = sreq.req;
    *from = sreq.come_from;
    return 1;
}

state_request state_rcreate(
    naddr_t     addr,
    uint32_t    uid,
    state_rtype type,
    sign        s
){
    state_request req;
    memset(&req, 0, sizeof(req));

    req.addr  = ln_hton(&addr);
    req.uid   = htonl(uid);
    req.nonce = htonl(randombytes_random());
    req.type  = (uint8_t)(type);
    req.timestamp = htonll(mt_time_get_seconds());
    memcpy(req.pubkey, s.id_pub, CRYPTO_PUBKEY_BYTES);

    printf("[state][rcr] fingerprint: %s\n", crypto_fingerprint(req.pubkey));
    uint8_t sign_buff[sizeof(req) - CRYPTO_SIGN_BYTES] = {0};
    size_t  offset = 0;

    memcpy(sign_buff + offset, &req.nonce, sizeof(req.nonce)); offset += sizeof(req.nonce);
    memcpy(sign_buff + offset, &req.addr, sizeof(req.addr)); offset += sizeof(req.addr);
    memcpy(sign_buff + offset, &req.uid, sizeof(req.uid)); offset += sizeof(req.uid);
    memcpy(sign_buff + offset, &req.type, sizeof(req.type)); offset += sizeof(req.type);
    memcpy(sign_buff + offset, &req.timestamp, sizeof(req.timestamp)); offset += sizeof(req.timestamp);
    memcpy(sign_buff + offset, req.pubkey, CRYPTO_PUBKEY_BYTES);

    sign_data(&s, sign_buff, sizeof(sign_buff), req.signature);
    return req;
}

int state_rreceive(
    unsigned char *data,
    state_request *out
){
    state_request req;
    memcpy(&req, data, sizeof(req));

    uint8_t sign_buff[sizeof(req) - CRYPTO_SIGN_BYTES] = {0};
    size_t  offset = 0;

    memcpy(sign_buff + offset, &req.nonce, sizeof(req.nonce)); offset += sizeof(req.nonce);
    memcpy(sign_buff + offset, &req.addr, sizeof(req.addr)); offset += sizeof(req.addr);
    memcpy(sign_buff + offset, &req.uid, sizeof(req.uid)); offset += sizeof(req.uid);
    memcpy(sign_buff + offset, &req.type, sizeof(req.type)); offset += sizeof(req.type);
    memcpy(sign_buff + offset, &req.timestamp, sizeof(req.timestamp)); offset += sizeof(req.timestamp);
    memcpy(sign_buff + offset, req.pubkey, CRYPTO_PUBKEY_BYTES);

    printf("[state] fingerprint: %s\n", crypto_fingerprint(req.pubkey));
    if (0 > sign_verify(sign_buff, sizeof(sign_buff), req.signature, req.pubkey)){
        fprintf(stderr, "[state_rx] sign verification failed\n");
        return -1;
    }

    naddr_t net_ord = req.addr;
    req.addr  = ln_ntoh(&net_ord);
    req.uid   = ntohl(req.uid);
    req.nonce = ntohl(req.nonce);
    req.timestamp = ntohll(req.timestamp);

    if (!crypto_pubkey_and_uid_check(req.pubkey, req.uid)){
        fprintf(stderr, "[state_rr] pubkey & uid missmatch\n");
        return -1;
    }

    memcpy(out, &req, sizeof(req));
    return 0;
}

state_request state_retranslate(state_request req){
    state_request o;

    naddr_t addr = req.addr;
    o.addr  = ln_hton(&addr);
    o.uid   = htonl(req.uid);
    o.nonce = htonl(req.nonce);
    o.timestamp = htonll(req.timestamp);
    o.type = req.type;
    memcpy(o.pubkey, req.pubkey, CRYPTO_PUBKEY_BYTES);
    memcpy(o.signature, req.signature, CRYPTO_SIGN_BYTES);

    return o;
}
