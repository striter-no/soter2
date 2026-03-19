#include <lownet/udp_sock.h>
#include <crypto/system.h>
#include <base/prot_queue.h>
#include <multithr/events.h>
#include <multithr/time.h>

#ifndef STATESERV_DT
#define STATESERV_DT 2
#endif

#ifndef STATE_H

typedef enum {
    REQUEST_CONNECTION
} state_rtype;

typedef struct __attribute__((packed)) {
    uint32_t ip;
    uint16_t port;
    uint32_t uid;
    unsigned char pubkey[CRYPTO_PUBKEY_BYTES];
    unsigned char signature[CRYPTO_SIGN_BYTES];

    state_rtype type;
    uint32_t    nonce;
    int64_t     timestamp;
} state_request;

typedef struct {
    prot_queue   new_state_ans;
    mt_eventsock new_state_fd;
} state_system;

int  state_sys_init(state_system *sys);
void state_sys_end (state_system *sys);

int state_sys_wait(state_system *sys, state_request *out, int timeout);
int state_sys_new_ans(state_system *sys, state_request *req);

state_request state_rcreate(
    uint32_t    ip,
    uint16_t    port,
    uint32_t    uid,
    state_rtype type,
    sign        s
);

state_request state_retranslate(state_request r);

int state_rreceive(
    unsigned char *data,
    state_request *out
);

#endif
#define STATE_H