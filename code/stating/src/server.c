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
    prot_queue        requests;
    prot_queue        pending_peers;
    mt_eventsock      new_request;
} state_server;

static int state_handler(protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
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

    server_request servr = {r, *nfd};

    printf("[state_handler] request from %u\n", r.uid);
    prot_queue_push(&serv->requests, &servr);
    mt_evsock_notify(&serv->new_request);

    return 0;
}

static int uid_filter(size_t, void *elem, void *ctx){
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
    prot_queue_create(sizeof(server_request), &server.requests);
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

    while(soter2_irunning(&intr)){
        if (mt_evsock_wait(&server.new_request, 100) <= 0) continue;
        mt_evsock_drain(&server.new_request);
        
        server_request sr;
        while (prot_queue_pop(&server.requests, &sr) == 0) {
            printf("looping...\n");
            state_request r = sr.r;

            server_request peer;
            
            if (prot_queue_peek(&server.pending_peers, &peer) == 0) {
                if (r.uid == peer.r.uid){
                    printf("[main][loop] ignoring identical UIDs\n");
                    continue;
                }
                
                state_request to_new = state_retranslate(peer.r);

                protopack *pack_to_new = udp_make_pack(0, 0, r.uid, PACK_STATE, &to_new, sizeof(to_new));
                pvd_sender_send(&server.intr->sender, pack_to_new, &sr.nfd);
                free(pack_to_new);

                state_request to_old = state_retranslate(r);
                protopack *pack_to_peer = udp_make_pack(0, 0, peer.r.uid, PACK_STATE, &to_old, sizeof(to_old));
                pvd_sender_send(&server.intr->sender, pack_to_peer, &peer.nfd);
                free(pack_to_peer);

                printf("[pair] connected uid:%u <-> uid:%u\n", peer.r.uid, r.uid);
                
                prot_queue_pop(&server.pending_peers, NULL);
            } else {
                prot_array_filter(&server.pending_peers.arr, uid_filter, &r.uid);
                printf("[queue] uid:%u waiting for peer\n", r.uid);
                prot_queue_upush(&server.pending_peers, &sr);
            }
        }
    }

    soter2_intr_end(&intr);
}