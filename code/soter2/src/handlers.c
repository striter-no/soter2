#include <soter2/handlers.h>

int soter2_hnd_ACK(protopack *pck, nnet_fd nfd, pvd_sender *s, void *_ctx){
    if (!_ctx) return -1;
    (void)nfd;

    // printf("[ack] new ACK\n");
    app_context *ctx = _ctx;
    
    peer_info info;
    if (0 > peers_db_get(ctx->p_db, pck->h_from, &info)){
        fprintf(stderr, "[hnd][ack] `%u` is unknown\n", pck->h_from);
        return -1;
    }

    if (info.state == PEER_ST_INITED){
        // printf("[ack] sended PACK_ACK\n");
        pvd_sender_send(
            s, 
            proto_msg_quick(ctx->rudp->self_uid, info.UID, 0, PACK_ACK),
            info.nfd
        );
    }

    // peers_db_schange(ctx->p_db, info.UID, PEER_ST_HANDSHAKING);
    peers_db_schange(ctx->p_db, info.UID, PEER_ST_ACTIVE);
    // printf("[ack] new active peer\n");
    return 0;
}

int soter2_hnd_PUNCH(protopack *pck, nnet_fd nfd, pvd_sender *s, void *_ctx){
    if (!_ctx) return -1;

    naddr_t addr = ln_nfd2addr(nfd);
    // printf("[punch] new PUNCH from %s:%u\n", addr.ip.v4.ip, addr.ip.v4.port);
    app_context *ctx = _ctx;
    
    peer_info info;
    if (0 > peers_db_get(ctx->p_db, pck->h_from, &info)){
        fprintf(stderr, "[hnd][punch] `%u` is unknown\n", pck->h_from);
        return -1;
    }

    peers_db_unfd(ctx->p_db, info.UID, nfd);
    // printf("[punch] uid: %u/%u\n", info.UID, pck->h_from);

    if (info.state != PEER_ST_INITED && info.state != PEER_ST_NATPUNCHING){
        fprintf(
            stderr, 
            "[hnd][punch] check failed, `%u` state is not PEER_ST_INITED/PEER_ST_NATPUNCHING: %d\n", 
            pck->h_from, info.state
        );
        return -1;
    }

    // printf("[punch] sended PACK_ACK\n");
    pvd_sender_send(
        s, 
        proto_msg_quick(ctx->rudp->self_uid, info.UID, 0, PACK_ACK),
        nfd
    );

    if (info.state == PEER_ST_NATPUNCHING)
        return 0;

    // printf("[punch] changed stats\n");
    peers_db_schange(ctx->p_db, info.UID, PEER_ST_NATPUNCHING);
    return 0;
}

int soter2_hnd_PING  (protopack *pck, nnet_fd nfd, pvd_sender *s, void *_ctx){return -1;}
int soter2_hnd_PONG  (protopack *pck, nnet_fd nfd, pvd_sender *s, void *_ctx){return -1;}
int soter2_hnd_GOSSIP(protopack *pck, nnet_fd nfd, pvd_sender *s, void *_ctx){return -1;}
int soter2_hnd_STATE (protopack *pck, nnet_fd nfd, pvd_sender *s, void *_ctx){return -1;}

// int soter2_hnd_DATA(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){return -1;}
// int soter2_hnd_RACK(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){return -1;}
// int soter2_hnd_HELLO(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){return -1;}
// int soter2_hnd_REJECT(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){return -1;}
// int soter2_hnd_ACCEPT(protopack *pck, nnet_fd nfd, pvd_sender *s, void *ctx){return -1;}