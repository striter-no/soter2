#include <soter2/handlers.h>

int soter2_hnd_ACK(protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
    if (!_ctx) return -1;
    (void)nfd;

    app_context *ctx = _ctx;
    
    peer_info info;
    if (0 > peers_db_get(ctx->p_db, pck->h_from, &info)){
        fprintf(stderr, "[hnd][ack] `%u` is unknown\n", pck->h_from);
        return -1;
    }

    if (info.state == PEER_ST_INITED){
        protopack *msg = proto_msg_quick(ctx->rudp->self_uid, info.UID, 0, PACK_ACK);
        pvd_sender_send(s, msg, &info.nfd);
        free(msg);
    }

    // peers_db_schange(ctx->p_db, info.UID, PEER_ST_HANDSHAKING);
    peers_db_schange(ctx->p_db, info.UID, PEER_ST_ACTIVE);
    return 0;
}

int soter2_hnd_PUNCH(protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
    if (!_ctx) return -1;

    app_context *ctx = _ctx;
    
    peer_info info;
    if (0 > peers_db_get(ctx->p_db, pck->h_from, &info)){
        fprintf(stderr, "[hnd][punch] `%u` is unknown\n", pck->h_from);
        return -1;
    }

    peers_db_unfd(ctx->p_db, info.UID, nfd);

    if (info.state != PEER_ST_INITED && info.state != PEER_ST_NATPUNCHING){
        fprintf(
            stderr, 
            "[hnd][punch] check failed, `%u` state is not PEER_ST_INITED/PEER_ST_NATPUNCHING: %d\n", 
            pck->h_from, info.state
        );
        return -1;
    }

    protopack *msg = proto_msg_quick(ctx->rudp->self_uid, info.UID, 0, PACK_ACK);
    pvd_sender_send(s, msg, nfd);
    free(msg);

    if (info.state == PEER_ST_NATPUNCHING)
        return 0;

    peers_db_schange(ctx->p_db, info.UID, PEER_ST_NATPUNCHING);
    return 0;
}

int soter2_hnd_PING(protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    
    protopack *pong = proto_msg_quick(
        ctx->rudp->self_uid, pck->h_from, pck->seq, PACK_PONG
    );
    pvd_sender_send(s, pong, nfd);
    free(pong);

    if (0 > peers_db_utime(ctx->p_db, pck->h_from)){
        printf("ping failed!\n");
        // peers_db_remove(ctx->p_db, pck->h_from);
    }
    // printf("got ping\n");
    return 0;
}

int soter2_hnd_PONG(protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    (void)nfd;
    (void)s;

    if (0 > peers_db_utime(ctx->p_db, pck->h_from)){
        printf("pong failed!\n");
        // peers_db_remove(ctx->p_db, pck->h_from);
    }
    // printf("got pong\n");
    return 0;
}

int soter2_hnd_GOSSIP(protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    (void)nfd;
    (void)s;

    gossip_from_packet(ctx->g_syst, pck);

    return 0;
}

int soter2_hnd_STATE (protopack *pck, nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    (void)nfd;
    (void)s;

    // printf("got STATE packet...\n");
    if (pck->d_size != sizeof(state_request)){
        fprintf(stderr, "[hnd][state] bad packet size: %u\n", pck->d_size);
        return -1;
    }

    state_request req;
    if (0 > state_rreceive(pck->data, &req)){
        return -1;
    }

    if (req.uid == ctx->rudp->self_uid){
        fprintf(stderr, "[hnd][state] rejecting self-peer\n");
        return -1;
    }

    int64_t dt = mt_time_get_seconds() - req.timestamp;
    if (dt >= 30){
        fprintf(stderr, "[hnd][state] rejecting peer, too old (delta: %li)\n", dt);
        return -1;
    }

    printf("[hnd][state] new peer: %u\n", req.uid);
    state_sys_new_ans(ctx->ssytem, &req);
    return 0;
}
