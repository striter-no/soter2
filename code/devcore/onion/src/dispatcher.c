#include "onion/packets.h"
#include "rudp/_modules.h"
#include <onion/dispatcher.h>
#include <stdint.h>

// -- lifetime cycle

int onion_dispatcher_init(
    onion_dispatcher *disp,
    peers_db *pdb,
    gossip_system *gsp
){
    if (!disp || !pdb || !gsp) return -1;

    if (0 > prot_table_create(
        sizeof(uint32_t),
        sizeof(rele_circut),
        DYN_OWN_BOTH, &disp->circutes
    )) return -1;

    if (0 > prot_table_create(
        sizeof(uint32_t),
        sizeof(onion_connection),
        DYN_OWN_BOTH, &disp->current_connections
    )) return -1;

    if (0 > prot_queue_create(
        sizeof(listener_packet), // {pkt, nnetfd}
        &disp->request_packets
    )) return -1;

    if (0 > mt_evsock_new(&disp->request_ev)) return -1;

    if (0 > prot_queue_create(
        sizeof(listener_packet), // {pkt, nnetfd}
        &disp->response_packets
    )) return -1;

    if (0 > mt_evsock_new(&disp->response_ev)) return -1;

    if (0 > prot_queue_create(
        sizeof(listener_packet), // {pkt, nnetfd}
        &disp->exit_packets
    )) return -1;

    if (0 > mt_evsock_new(&disp->exit_ev)) return -1;

    // no queue for incoming packets because of function
    // `onion_dispatcher_serve_packet` (providing packets straight to it)

    disp->gsp = gsp;
    disp->pdb = pdb;

    return 0;
}

int onion_dispatcher_end (onion_dispatcher *disp){
    if (!disp) return -1;

    dyn_pair *pr = NULL;
    for (size_t inx = 0; NULL != (pr = prot_table_iterate(&disp->circutes, &inx)); ){
        rele_circut_clear(pr->second);
    }

    listener_packet pkt;
    while (0 == prot_queue_pop(&disp->exit_packets, &pkt)) {
        if (pkt.pack) free(pkt.pack);
    }

    while (0 == prot_queue_pop(&disp->request_packets, &pkt)) {
        if (pkt.pack) free(pkt.pack);
    }

    while (0 == prot_queue_pop(&disp->response_packets, &pkt)) {
        if (pkt.pack) free(pkt.pack);
    }

    prot_queue_end(&disp->exit_packets);
    mt_evsock_close(&disp->exit_ev);

    prot_queue_end(&disp->response_packets);
    mt_evsock_close(&disp->response_ev);

    prot_queue_end(&disp->request_packets);
    mt_evsock_close(&disp->request_ev);

    prot_table_end(&disp->circutes);
    prot_table_end(&disp->current_connections);
    return 0;
}

int onion_connection_new(onion_connection *conn, rele_circut *circ, uint32_t owner_uid, nnet_fd owner_nfd){
    if (!conn) return -1; // circute can be NULL for middle nodes
    conn->current_relay = 0;
    conn->circ = circ;
    conn->owner_uid = owner_uid;
    conn->owner_nfd = owner_nfd;
    conn->next_hop_uid = UINT32_MAX;
    conn->next_hop_nfd = (nnet_fd){0};
    conn->next_hop_nfd.rfd = -1;

    return 0;
}

// -- connection managment

int onion_connection_circgen(onion_dispatcher *disp, uint32_t circ_uid, int nodes_n){
    if (!disp) return -1;
    if (nodes_n < 1) return -1;

    onion_connection conn;
    if (0 > onion_dispatcher_getconn(disp, circ_uid, &conn))
        return -1;

    if (nodes_n < 3)
        fprintf(stderr, "[onion][warning] unsafe circut generated with %d nodes\n", nodes_n);

    dyn_array excl = dyn_array_create(sizeof(uint32_t));
    for (int i = 0; i < nodes_n; i++) {
        uint32_t uid = onion_get_next_uid(disp, &excl);
        if (uid == UINT32_MAX) {
            fprintf(stderr, "[onion][error] failed to find a suitable peer for circut generation\n");
            dyn_array_end(&excl);
            return -1;
        }

        peer_info info;
        if (0 > peers_db_get(disp->pdb, uid, &info)){
            fprintf(stderr, "[onion][error] failed to get peer info for circut generation\n");
            dyn_array_end(&excl);
            return -1;
        }

        dyn_array_push(&excl, &uid);
        rele_circut_extend(conn.circ, &info);
    }

    dyn_array_end(&excl);
    return 0;
}

uint32_t onion_get_next_uid(onion_dispatcher *disp, dyn_array *excl){
    if (!disp) return UINT32_MAX;

    peer_info *snapshot = NULL;
    size_t sz = peers_db_snapshot(disp->pdb, &snapshot);

    // shuffling peers
    for(int i = sz - 1;  i > 0; i--) {
        int random = rand() % (i+1);

        peer_info tmp = snapshot[i];
        snapshot[i] = snapshot[random];
        snapshot[random] = tmp;
    }

    for (size_t i = 0; i < sz; i++){
        peer_info *peer = &snapshot[i];
        if (peer->state == PEER_ST_ACTIVE && (dyn_array_index(excl, &i) == SIZE_MAX)) {
            free(snapshot);
            return peer->UID;
        }
    }

    free(snapshot);
    return UINT32_MAX;
}

bool onion_dispatcher_check(onion_dispatcher *disp, uint32_t circ_uid){
    if (!disp) return false;

    rele_circut *circ = prot_table_get(&disp->circutes, &circ_uid);
    return circ != NULL;
}

int onion_dispatcher_getcirc(onion_dispatcher *disp, uint32_t circ_uid, rele_circut *circ){
    if (!disp) return -1;

    rele_circut *_circ = prot_table_get(&disp->circutes, &circ_uid);
    if (!_circ) return -1;

    *circ = *_circ;
    return 0;
}

int onion_dispatcher_getconn(onion_dispatcher *disp, uint32_t circ_uid, onion_connection *conn){
    if (!disp) return -1;

    onion_connection *_conn = prot_table_get(&disp->current_connections, &circ_uid);
    if (!_conn) return -1;

    *conn = *_conn;
    return 0;
}

int onion_dispatcher_close(onion_dispatcher *disp, uint32_t circ_uid){
    if (!disp) return -1;

    onion_connection conn;
    if (0 > onion_dispatcher_getconn(disp, circ_uid, &conn))
        return -1;

    rele_circut_clear(conn.circ);
    prot_array_end(&conn.e2ee_conns);

    if (0 > prot_table_remove(&disp->circutes, &circ_uid))
        return -1;

    if (0 > prot_table_remove(&disp->current_connections, &circ_uid))
        return -1;

    return 0;
}

// -- packet serving

int onion_dispatcher_serve_packet(onion_dispatcher *disp, protopack *pkt, const nnet_fd *from_whom){
    if (!disp || !pkt || pkt->d_size != sizeof(onion_packet)) return -1;

    onion_packet onipkt;
    if (0 > onion_pack_deserial(&onipkt, pkt->data))
        return -1;

    switch (onipkt.cmd) {
        case ONION_CMD_CREATE: {
            uint32_t new_circ_uid = onipkt.circut_uid;
            if (onion_dispatcher_check(disp, new_circ_uid))
                return -1;

            rele_circut new_circ;
            if (0 > rele_circut_new(&new_circ, new_circ_uid))
                return -1;

            onion_connection new_conn;
            if (0 > onion_connection_new(&new_conn, &new_circ, pkt->h_from, *from_whom))
                return -1;

            prot_table_set(&disp->circutes, &new_circ_uid, &new_circ);
            prot_table_set(&disp->current_connections, &new_circ_uid, &new_conn);
        } break;
        case ONION_CMD_EXTEND: {
            if (onipkt.d_size != sizeof(light_peer_info)) return -1;

            onion_connection conn;
            if (0 > onion_dispatcher_getconn(disp, onipkt.circut_uid, &conn)) return -1;
            if (conn.owner_uid != pkt->h_from) return -1;

            peer_info pinfo;
            if (0 > peers_db_reconstruct(&pinfo, (light_peer_info *)onipkt.cell)) return -1;

            if (!peers_db_check(disp->pdb, pinfo.UID)){
                return -1; // TODO: may be connect to this peer?
            }

            // in case we are initializer or end node
            if (conn.next_hop_uid == UINT32_MAX) {
                // we are not trying to construct route by ourselfs
                conn.next_hop_uid = pinfo.UID;
                conn.next_hop_nfd = pinfo.nfd;

                prot_table_set(&disp->current_connections, &onipkt.circut_uid, &conn);

                // sender must save in pinfo nfd to next hop, not end addressant!
                _onion_dispatcher_request(disp, pkt, &pinfo.nfd);


                printf("[onion] Extending circuit to peer %u\n", conn.next_hop_uid);
            } else {
                // conn.next_hop_uid is 100% exists due if ( next_hop == UINT32_MAX )
                // we are keeping original pinfo, so no information is lost
                onipkt.circut_uid = conn.next_hop_uid;

                _onion_dispatcher_request(disp, pkt, &conn.next_hop_nfd);
                printf("[onion] Forwarding EXTEND to peer %u\n", conn.next_hop_uid);
            }
        } break;
        case ONION_CMD_CLOSE: {
            onion_connection conn;
            if (0 > onion_dispatcher_getconn(disp, onipkt.circut_uid, &conn))
                return -1;

            if (conn.owner_uid != pkt->h_from)
                return -1;

            return onion_dispatcher_close(disp, onipkt.circut_uid);
        } break;
        case ONION_CMD_DATA: {
            onion_connection conn;
            if (0 > onion_dispatcher_getconn(disp, onipkt.circut_uid, &conn)) return -1;

            // forward propogation (request)
            if (pkt->h_from == conn.owner_uid) {
                if (conn.next_hop_uid != UINT32_MAX) {
                    onipkt.circut_uid = conn.next_hop_uid;

                    peer_info info;
                    if (0 > peers_db_get(disp->pdb, conn.next_hop_uid, &info)){
                        fprintf(stderr, "[onion][data] something went wrong when trying to get next_hop_uid, disconnect could occured with %u\n", conn.next_hop_uid);
                        return -1;
                    }

                    _onion_dispatcher_request(disp, pkt, &conn.next_hop_nfd);
                } else {

                    peer_info info;
                    if (0 > peers_db_get(disp->pdb, pkt->h_from, &info)){
                        fprintf(stderr, "[onion][data] something went wrong when trying to get sender, disconnect could occured with %u\n", pkt->h_from);
                        return -1;
                    }

                    _onion_dispatcher_exit_packet(disp, pkt, &info.nfd);
                    printf("[onion] Exit node received payload: %d bytes\n", onipkt.d_size);
                }
            }

            // backward propogation (response)
            else if (pkt->h_from == conn.next_hop_uid) {
                // we are sender/receiver

                peer_info info;
                if (0 > peers_db_get(disp->pdb, pkt->h_from, &info)){
                    fprintf(stderr, "[onion][data] something went wrong when trying to get sender, disconnect could occured with %u\n", pkt->h_from);
                    return -1;
                }

                if (conn.circ != NULL){
                    _onion_dispatcher_response(disp, pkt, &conn.owner_nfd);
                    return 0;
                }
                // we are middle node
                _onion_dispatcher_request(disp, pkt, &conn.owner_nfd);
            }
            else {
                fprintf(stderr, "[onion][data] unknown packet from %u\n", pkt->h_from);
                return -1; // unknown packet
            }
        } break;

        default:
            return -1;
    }

    return 0;
}

int _onion_dispatcher_request(onion_dispatcher *disp, protopack *pkt, const nnet_fd *to_whom){
    if (!disp || !pkt || !to_whom) return -1;

    listener_packet lpkt = {
        .from_who = *to_whom,
        .pack = pkt
    };
    if (0 > prot_queue_push(&disp->request_packets, &lpkt))
        return -1;

    mt_evsock_notify(&disp->request_ev);
    return 0;
}

int _onion_dispatcher_response(onion_dispatcher *disp, protopack *pkt, const nnet_fd *from_whom){
    if (!disp || !pkt || !from_whom) return -1;

    listener_packet lpkt = {
        .from_who = *from_whom,
        .pack = pkt
    };
    if (0 > prot_queue_push(&disp->response_packets, &lpkt))
        return -1;

    mt_evsock_notify(&disp->response_ev);
    return 0;
}

int _onion_dispatcher_exit_packet(onion_dispatcher *disp, protopack *pkt, const nnet_fd *nfd){
    if (!disp || !pkt || !nfd) return -1;

    listener_packet lpkt = {
        .from_who = *nfd,
        .pack = pkt
    };

    if (0 > prot_queue_push(&disp->exit_packets, &lpkt))
        return -1;

    mt_evsock_notify(&disp->exit_ev);
    return 0;
}
