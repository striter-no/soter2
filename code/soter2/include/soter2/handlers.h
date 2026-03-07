#include "modules.h"

#ifndef SOTER2_HANDLERS_H

// -- watcher handlers
// -- non-RUDP
int soter2_hnd_ACK   (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PUNCH (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PING  (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PONG  (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_GOSSIP(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
int soter2_hnd_STATE (protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);

// -- RUDP handlers
// int soter2_hnd_DATA(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
// int soter2_hnd_RACK(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
// int soter2_hnd_HELLO(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
// int soter2_hnd_REJECT(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);
// int soter2_hnd_ACCEPT(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx);

typedef struct {
    rudp_dispatcher *rudp;
    watcher         *watcher;
    peers_db        *p_db;
    gossip_system   *g_syst;
    pvd_listener    *listener;
} app_context;

typedef struct {
    naddr_t  addr;
    uint32_t UID;
} app_peer_info;

#endif
#define SOTER2_HANDLERS_H