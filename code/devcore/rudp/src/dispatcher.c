#include "rudp/_modules.h"
#include "rudp/packets.h"
#include <packproto/protomsgs.h>
#include <rudp/dispatcher.h>
#include <stdint.h>

// -- creation
int rudp_dispatcher_new(
    rudp_dispatcher *disp, 
    pvd_sender *sender,

    uint32_t self_uid
){
    if (!disp || !sender) return -1;

    disp->self_uid = self_uid;
    disp->sender = sender;

    if (0 > mt_evsock_new(&disp->ev_passed)){
        return -1;
    }

    if (0 > prot_queue_create(sizeof(protopack*), &disp->passed_pkts)){
        mt_evsock_close(&disp->ev_passed);
        return -1;
    }

    if (0 > prot_table_create(
        sizeof(uint32_t), sizeof(rudp_connection*), 
        DYN_OWN_BOTH, &disp->connections
    )){
        prot_queue_end(&disp->passed_pkts);
        mt_evsock_close(&disp->ev_passed);
        return -1;
    }

    disp->daemon = 0;
    atomic_store(&disp->is_running, false);
    return 0;
}

int rudp_dispatcher_end(rudp_dispatcher *disp){
    if (!disp) return -1;

    // -- joining
    atomic_store(&disp->is_running, false);
    pthread_join(disp->daemon, NULL);

    // -- clearing

    protopack *pkt;
    while (0 == prot_queue_pop(&disp->passed_pkts, &pkt)){
        if (!pkt) continue;
        free(pkt);
    }

    for (size_t i = 0; i < disp->connections.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&disp->connections.table.array, i);
        if (!pair) continue;

        if (pair->second){
            rudp_conn_close(*((rudp_connection**)pair->second));
            free(*((rudp_connection**)pair->second));
        }
    }

    // -- ending

    prot_queue_end(&disp->passed_pkts);
    prot_table_end(&disp->connections);

    return 0;
}

// -- connection managing
int rudp_est_connection(
    rudp_dispatcher *disp, 
    rudp_connection **out_conn,
    uint32_t other_UID,
    nnet_fd  *nfd
){
    if (!disp || !out_conn) return -1;

    *out_conn = malloc(sizeof(**out_conn));
    if (!out_conn) return -1;

    if (0 > rudp_conn_new(*out_conn, disp->sender, disp->self_uid, other_UID, *nfd)){
        free(*out_conn);
        return -1;
    }

    printf("[estconn] connection with %u established\n", other_UID);
    prot_table_set(&disp->connections, &other_UID, out_conn);
    return 0;
}

int rudp_get_connection(
    rudp_dispatcher *disp, 
    uint32_t UID,
    rudp_connection **conn
){
    if (!disp || !conn) return -1;

    *conn = *(rudp_connection **)prot_table_get(&disp->connections, &UID);
    if (!(*conn)) return -1;

    return 0;
}

int rudp_close_conncetion(
    rudp_dispatcher *disp, 
    uint32_t UID
){
    if (!disp) return -1;

    rudp_connection **conn = prot_table_get(&disp->connections, &UID);
    if (!conn || !(*conn)) return -1;

    rudp_conn_close(*conn);
    free(*conn);

    return prot_table_remove(&disp->connections, &UID);
}

// -- system

int rudp_dispatcher_pass(
    rudp_dispatcher *disp, 
    protopack *pkt
){
    if (!disp || !pkt) return -1;

    prot_queue_push(&disp->passed_pkts, &pkt);
    mt_evsock_notify(&disp->ev_passed);

    return 0;
}

static void *_rudp_dispatcher_worker(void *_args);
int rudp_dispatcher_run(
    rudp_dispatcher *disp
){
    if (!disp) return -1;
    atomic_store(&disp->is_running, true);

    int r = pthread_create(
        &disp->daemon,
        NULL,
        &_rudp_dispatcher_worker,
        disp
    );

    if (r < 0){
        atomic_store(&disp->is_running, false);
    }

    return r;
}

// -- workers

static void _rudp_dispatcher_net_handler(rudp_dispatcher *disp, rudp_connection *conn, protopack *pkt);
static void *_rudp_dispatcher_worker(void *_args){
    rudp_dispatcher *disp = _args;

    while (atomic_load(&disp->is_running)){
        int r = mt_evsock_wait(&disp->ev_passed, 50);
        if (r < 0){
            perror("poll");
            continue;
        }

        for (size_t i = 0; i < disp->connections.table.array.len;){
            dyn_pair *pair = dyn_array_at(&disp->connections.table.array, i);
            
            // removing closed or dead connections
            if (!pair || !pair->second) {
                dyn_array_remove(&disp->connections.table.array, i);
                continue;
            }

            rudp_connection *conn = *((rudp_connection**)pair->second);
            if (conn->closed){
                dyn_array_remove(&disp->connections.table.array, i);
                continue;
            }

            _rudp_conn_timeouts(conn);
            i++;
        }

        if (r == 0) continue;
        mt_evsock_drain(&disp->ev_passed);
        
        protopack *pkt;
        if (0 > prot_queue_pop(&disp->passed_pkts, &pkt)){
            continue;
        }

        uint32_t c_uid = pkt->h_from;
        rudp_connection *connection = NULL;
        if (0 > rudp_get_connection(disp, c_uid, &connection)){
            fprintf(stderr, "[rudp][disp][worker] error: no connection established with %u\n", c_uid);
            free(pkt);
            continue;
        }

        _rudp_dispatcher_net_handler(disp, connection, udp_copy_pack(pkt));
        free(pkt);
    }
    
    return NULL;
}

static void _rudp_dispatcher_net_handler(rudp_dispatcher *disp, rudp_connection *conn, protopack *pkt){
    if (pkt->packtype == PACK_DATA){
        uint32_t seq = pkt->seq;

        _rudp_conn_pass_net(conn, pkt);
        _rudp_conn_reordering(conn);

        // printf("delta: %li\n", (int64_t)seq - (int64_t)conn->last_sended_ack);
        if ((int64_t)seq - (int64_t)conn->last_sended_ack < 1) {
            conn->last_sended_ack = seq;
            // free(pkt);
            return;
        }
        
        conn->last_sended_ack = seq;
        protopack *msg = proto_msg_quick(
            conn->s_uid, conn->c_uid, seq, PACK_RACK
        );
        conn->last_sended_ack = seq;
        if (msg) {
            pvd_sender_send(conn->sender, msg, &conn->nfd);
            free(msg);
        } else {
            fprintf(stderr, "[rudp][disp][nhandler] error: failed to compose and send RUDP ACK to %u\n", conn->c_uid);
        }

        // free(pkt);
        // not freeing packet, passing to connection
    } else if (pkt->packtype == PACK_RACK){
        prot_array_lock(&conn->pkts_fhost);

        bool was_ack = false;

        for (size_t i = 0; i < conn->pkts_fhost.array.len; ){
            rudp_pending_pkt *ppkt = prot_array_at(&conn->pkts_fhost, i);

            // cumulative ACK
            // removing all pending packets from array if greater ACK recved
            if (ppkt->seq <= pkt->seq){
                if (ppkt->copy_pack) free(ppkt->copy_pack);
                prot_array_remove(&conn->pkts_fhost, i);
                was_ack = true;
                
                if (conn->last_recved_ack == UINT32_MAX || pkt->seq > conn->last_recved_ack) {
                    conn->last_recved_ack = pkt->seq;
                }
                
            } else {
                i++;
            }
        }

        if (!was_ack){
            if (pkt->seq < conn->current_seq) {
                fprintf(stderr, "[debug] duplicate/late ACK: %u\n", pkt->seq);
            } else {
                fprintf(stderr, "[warn] impossible ACK: %u (current seq: %u)\n",
                        pkt->seq, conn->current_seq);
            }
        }

        prot_array_unlock(&conn->pkts_fhost);

        free(pkt);
    } else if (pkt->packtype == PACK_FIN) {

        uint32_t remote_uid = conn->c_uid;
        fprintf(stderr, "[rudp][disp] FIN received from %u, closing connection\n", remote_uid);

        mt_evsock_notify(&conn->ev_reordered);
        rudp_close_conncetion(disp, remote_uid);
        
        free(pkt);
        return;

    } else {
        fprintf(
            stderr, 
            "[rudp][disp][nhandler] error: to handler passed unknown packet with type %s\n", 
            PROTOPACK_TYPES_CHAR[pkt->packtype]
        );

        free(pkt);
    }
}