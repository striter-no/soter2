#include "_modules.h"
#include <stdint.h>

#ifndef SOTER2_HANDLERS_H

// -- watcher handlers
int soter2_hnd_ACK   (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PUNCH (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PING  (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);
int soter2_hnd_PONG  (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);
int soter2_hnd_GOSSIP(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);
int soter2_hnd_STATE (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);
int soter2_hnd_TURN  (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx);

typedef struct {
    rudp_dispatcher *rudp;
    watcher         *watcher;
    peers_db        *p_db;
    gossip_system   *g_syst;
    pvd_listener    *listener;
    pvd_sender      *sender;

    turn_client     *turn;
    state_system    *ssytem;
    int64_t          now_ms;
} app_context;

typedef struct {
    naddr_t  addr;
    uint32_t UID;
} app_peer_info;

#endif
#define SOTER2_HANDLERS_H
