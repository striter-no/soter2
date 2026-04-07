#include "soter2/daemons.h"
#include <soter2/interface.h>
#include <crypto/system.h>
#include <stating/state.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    state_request r;
    nnet_fd       nfd;
} server_request;

typedef struct {
    soter2_interface *intr;
    prot_queue        pending_peers;
    mt_eventsock      new_request;
} state_server;

static int state_handler(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    // extract data from pck
    state_server *serv = _ctx;
    (void)nfd;
    (void)s;

    if (pck->d_size != sizeof(state_request)){
        fprintf(stderr, "[state_handler] incoming packet dsize missmatch (%u != %zu)\n", pck->d_size, sizeof(state_request));
        return -1;
    }

    state_request r;
    if (0 > state_rreceive(pck->data, &r)){
        fprintf(stderr, "[state_handler] something wrong with signing/pubkey/uid in packet from %u\n", pck->h_from);
        return -1;
    }

    printf("[state_handler] request from %u\n", r.uid);

    // build array of all known peers
    size_t peers_count = 0;
    server_request *peers_array = NULL;

    // count peers
    prot_array_lock(&serv->pending_peers.arr);
    for (size_t i = 0; i < serv->pending_peers.arr.array.len; i++) {
        server_request *sr = (server_request *)_prot_array_at_unsafe(&serv->pending_peers.arr, i);
        if (sr->r.uid != r.uid) {
            peers_count++;
        }
    }
    prot_array_unlock(&serv->pending_peers.arr);

    // allocate buffer: 4 bytes for count + array of state_request
    size_t data_size = sizeof(uint32_t) + peers_count * sizeof(state_request);
    uint8_t *response_buffer = malloc(data_size);
    if (!response_buffer) {
        fprintf(stderr, "[state_handler] failed to allocate response buffer\n");
        return -1;
    }

    // write count in network order
    uint32_t net_count = htonl((uint32_t)peers_count);
    memcpy(response_buffer, &net_count, sizeof(uint32_t));

    // write peer requests
    prot_array_lock(&serv->pending_peers.arr);
    size_t offset = sizeof(uint32_t);
    for (size_t i = 0; i < serv->pending_peers.arr.array.len; i++) {
        server_request *sr = (server_request *)_prot_array_at_unsafe(&serv->pending_peers.arr, i);
        if (sr->r.uid == r.uid) continue; // skip self

        state_request translated = state_retranslate(sr->r);
        memcpy(response_buffer + offset, &translated, sizeof(state_request));
        offset += sizeof(state_request);
    }
    prot_array_unlock(&serv->pending_peers.arr);

    // send response back to requester
    protopack *pack_response = udp_make_pack(0, 0, r.uid, PACK_STATE, response_buffer, data_size);
    pvd_sender_send(&serv->intr->sender, pack_response, nfd);
    free(pack_response);
    free(response_buffer);

    // add requester to pending peers
    server_request servr = {r, *nfd};
    prot_queue_upush(&serv->pending_peers, &servr);

    return 0;
}

static int uid_filter(size_t _, void *elem, void *ctx){ (void)_;
    server_request *r = elem;
    uint32_t      *uid = ctx;

    return r->r.uid == *uid;
}

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "usage: %s [bind IP] [bind PORT]\n", argv[0]);
        return 0;
    }

    soter2_interface intr = {0};
    if (0 > soter2_intr_init(&intr)) {
        fprintf(stderr, "[main] failed to init interface\n");
        return -1;
    }

    state_server server = {0};
    server.intr = &intr;
    mt_evsock_new    (&server.new_request);
    prot_queue_create(sizeof(server_request), &server.pending_peers);

    soter2_ivtable vt = {0};
    vt.STATE.ctx = &server;
    vt.STATE.foo = &state_handler;

    soter2_intr_reset_handlers(&intr, vt);

    if (0 > ln_usock_bind(&intr.sock, ln_make4(ln_ipv4(argv[1], atoi(argv[2]))))){
        perror("[main] failed to bind");
        return -1;
    }

    if (0 > soter2_intr_run(&intr)){
        fprintf(stderr, "[main] failed to run interface\n");
        return -1;
    }

    printf("[main] running server...\n");

    // server now handles everything in state_handler, no main loop needed
    while(soter2_irunning(&intr)){
        // event-driven, nothing to do here
    }

    soter2_intr_end(&intr);
}
