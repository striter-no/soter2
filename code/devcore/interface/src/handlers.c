#include "soter2/daemons.h"
#include "soter2/systems.h"
#include <soter2/handlers.h>
#include <stdint.h>

int soter2_hnd_ACK(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
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

int soter2_hnd_PUNCH(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    if (!_ctx) return -1;

    app_context *ctx = _ctx;

    peer_info info;
    if (pck->h_from == ctx->rudp->self_uid){
        fprintf(stderr, "[hnd][punch] got punch from myself, dropping\n");
        return -1;
    }

    if (0 > peers_db_get(ctx->p_db, pck->h_from, &info)){
        printf("[hnd][punch] `%u` is unknown, adding peer\n", pck->h_from);

        // in sake of stability of the network and
        // my sanity we add new peer if we recevie punch from unknown peer

        light_peer_info linfo;
        if (pck->d_size != sizeof(linfo)){
            fprintf(stderr, "[hnd][punch] data size is not sizeof(light_peer_info): %u\n", pck->d_size);
            return -1;
        }
        memcpy(&linfo, pck->data, sizeof(linfo));

        peers_db_reconstruct(&info, &linfo);
        peers_db_add(ctx->p_db, info);
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

int soter2_hnd_PING(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;

    protopack *pong = proto_msg_quick(
        ctx->rudp->self_uid, pck->h_from, pck->seq, PACK_PONG
    );
    pvd_sender_send(s, pong, nfd);
    free(pong);

    if (0 > peers_db_utime(ctx->p_db, pck->h_from)){
        // peers_db_remove(ctx->p_db, pck->h_from);
    }
    // printf("got ping\n");
    return 0;
}

int soter2_hnd_PONG(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    (void)nfd;
    (void)s;

    if (0 > peers_db_utime(ctx->p_db, pck->h_from)){
        // printf("pong failed!\n");
        // peers_db_remove(ctx->p_db, pck->h_from);
    }
    // printf("got pong\n");
    return 0;
}

int soter2_hnd_GOSSIP(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    (void)nfd;
    (void)s;

    gossip_from_packet(ctx->g_syst, pck);

    return 0;
}

int soter2_hnd_STATE (protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    // (void)nfd;
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

    state_sys_new_ans(ctx->ssytem, &req, ln_nfd2addr(nfd));
    return 0;
}

int soter2_hnd_TURN(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;

    naddr_t addr = ln_nfd2addr(nfd);
    if (pck->d_size < sizeof(turn_request)){
        fprintf(stderr, "[hnd][turn] bad packet size: %u\n", pck->d_size);
        return -1;
    }

    turn_request *trn = turn_req_recv(pck->data, pck->d_size);
    if (!trn){
        fprintf(stderr, "[hnd][turn] failed to recv turn request\n");
        return -1;
    }

    // a.k.a connection ID
    uint32_t hsh = turn_pair_hash(pck->h_from, trn->to_UID);

    // protopack *outpck;
    // turn_bind_peer bpeer;
    switch (trn->type){
        case TURN_CLOSE_HOLE: {

            if (0 > turn_client_unbind(ctx->turn, hsh)){

                turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_FAIL, NULL);
                turn_client_qsend(ctx->turn, r);
                free(r);

                fprintf(stderr, "[hnd][turn] TURN_CLOSE_HOLE failed\n");
                free(trn);
                return -1;
            }

            turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_OK, NULL);
            turn_client_qsend(ctx->turn, r);
            free(r);

        } break;

        case TURN_OPEN_HOLE:  {
            if (sizeof(light_peer_info) != trn->d_size){

                turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_FAIL, NULL);
                turn_client_qsend(ctx->turn, r);
                free(r);

                fprintf(stderr, "[hnd][turn] TURN_OPEN_HOLE failed, unknown payload\n");
                free(trn);
                return -1;
            }

            light_peer_info linfo, origin_linfo;
            memcpy(&linfo, trn->data, trn->d_size);

            peer_info origin_info;
            if (0 > peers_db_get(ctx->p_db, pck->h_from, &origin_info)){
                turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_FAIL, NULL);
                turn_client_qsend(ctx->turn, r);
                free(r);

                fprintf(stderr, "[hnd][turn] TURN_OPEN_HOLE failed, unknown peer\n");
                free(trn);
                return -1;
            }

            peers_db_linfo(&origin_info, &origin_linfo);

            turn_bind_peer bpeer = {
                .linfo_connected = linfo,
                .linfo_origin    = origin_linfo
            };

            if (0 > turn_client_bind(ctx->turn, pck->h_from, &bpeer)){

                turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_FAIL, NULL);
                turn_client_qsend(ctx->turn, r);
                free(r);

                fprintf(stderr, "[hnd][turn] TURN_OPEN_HOLE failed\n");
                free(trn);
                return -1;
            }

            turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_OK, NULL);
            turn_client_qsend(ctx->turn, r);
            free(r);

            printf("[hnd][turn] Opened new HOLE\n");
        } break;

        case TURN_DATA_MSG: {
            protopack *outgoing = NULL;
            if (0 > turn_client_unwrap(ctx->turn, pck, &outgoing)){

                // turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_FAIL, NULL);
                // turn_client_qsend(ctx->turn, r);
                // free(r);

                fprintf(stderr, "[hnd][turn] TURN_DATA_MSG unwrap failed\n");
                free(trn);
                return -1;
            }

            listener_packet lst = {.from_who = *nfd, .pack = outgoing};
            pvd_listener_pass(ctx->listener, &lst);

            // turn_request *r = turn_client_req_sys(ctx->turn, &addr, pck->h_from, trn->TRUID, TURN_SST_OK, NULL);
            // turn_client_qsend(ctx->turn, r);
            // free(r);
        } break;

        case TURN_SYSTEM_MSG: {
            char *commentary = NULL;
            turn_sys_status status;
            uint32_t truid;
            if (0 > turn_client_unwrap_sys(ctx->turn, trn, &status, &commentary, &truid)){

                fprintf(stderr, "[hnd][turn] TURN_SYSTEM_MSG unwrap failed\n");
                free(trn);
                return -1;
            }

            if (commentary) {
                fprintf(stderr, "[hnd][turn][sys] commentary: %s\n", commentary);
                free(commentary);
            }
        }

        default:
            break;
    }

    free(trn);
    return 0;
}

// receiver of HELLO requests
int soter2_hnd_HELLO(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    // (void)s;
    // (void)nfd;

    peer_info info;
    light_peer_info linfo;
    if (pck->d_size != sizeof(linfo)){
        fprintf(stderr, "[hnd][hello] data size is not sizeof(light_peer_info): %u\n", pck->d_size);
        return -1;
    }

    memcpy(&linfo, pck->data, sizeof(linfo));
    peers_db_reconstruct(&info, &linfo);

    info.state = PEER_ST_ACTIVE;
    peers_db_add(ctx->p_db, info);

    protopack *pkt = proto_msg_quick(ctx->rudp->self_uid, pck->h_from, 0, PACK_HELLO2);
    pvd_sender_send(s, pkt, nfd);
    free(pkt);

    return 0;
}

// initiator of HELLO sequence
int soter2_hnd_HELLO2(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *_ctx){
    app_context *ctx = _ctx;
    (void)s;
    (void)nfd;

    peer_info info;
    if (0 > peers_db_get(ctx->p_db, pck->h_from, &info)){
        fprintf(stderr, "[hnd][hello2] hello answer from unknown peer\n");
        return -1;
    }

    peers_db_schange(ctx->p_db, pck->h_from, PEER_ST_ACTIVE);
    return 0;
}
