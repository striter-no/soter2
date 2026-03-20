#include "rudp/_modules.h"
#include "rudp/packets.h"
#include <packproto/protomsgs.h>
#include <rudp/dispatcher.h>
#include <stdint.h>

#ifndef clamp
#define clamp(a, b, c) ((a < b) ? b: ((a > c) ? c: a))
#endif

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
    const nnet_fd  *nfd
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

    if (UID == UINT32_MAX){
        prot_table_lock(&disp->connections);
        size_t inx = rand() % disp->connections.table.array.len;

        dyn_pair *pair = dyn_array_at(&disp->connections.table.array, inx);
        if (!pair) return -1;

        *conn = *((rudp_connection**)pair->second);
        prot_table_unlock(&disp->connections);

        if (!(*conn)) return -1;
        return 0;
    }

    rudp_connection **ptr = prot_table_get(&disp->connections, &UID);
    if (!ptr || !(*ptr)) return -1;

    *conn = *ptr;
    return 0;
}

int rudp_close_conncetion(
    rudp_dispatcher *disp, 
    uint32_t UID
){
    if (!disp) return -1;

    rudp_connection **conn = prot_table_get(&disp->connections, &UID);
    if (!conn) return -1;
    if (!(*conn)){
        return prot_table_remove(&disp->connections, &UID);
    }

    rudp_conn_close(*conn);

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

static void _rudp_dispatcher_net_handler(rudp_dispatcher *disp, rudp_connection *conn, protopack *pkt, int64_t now_ms);
static void *_rudp_dispatcher_worker(void *_args){
    rudp_dispatcher *disp = _args;
    int64_t last_timeout_check = 0;

    int64_t now_ms = mt_time_get_millis_monocoarse();
    while (atomic_load(&disp->is_running)){
        int r = mt_evsock_wait(&disp->ev_passed, 100);
        mt_evsock_drain(&disp->ev_passed);

        if (r < 0){
            perror("poll");
            continue;
        }

        
        now_ms = mt_time_get_millis_monocoarse();
        
        if (now_ms - last_timeout_check >= 10) {
            // now_ms = mt_time_get_millis_monocoarse();
            prot_table_lock(&disp->connections);
            size_t r_size = 0;
            rudp_connection *conns[disp->connections.table.array.len + 1]; 

            for (size_t i = 0; i < disp->connections.table.array.len;){
                dyn_pair *pair = dyn_array_at(&disp->connections.table.array, i);
                if (!pair || !pair->second) {
                    dyn_array_remove(&disp->connections.table.array, i);
                    continue;
                }
                rudp_connection *conn = *((rudp_connection**)pair->second);
                if (conn->closed){
                    dyn_array_remove(&disp->connections.table.array, i);
                    continue;
                }
                conns[r_size++] = conn;
                i++;
            }
            prot_table_unlock(&disp->connections);

            for (size_t i = 0; i < r_size; i++){
                _rudp_conn_timeouts(conns[i], now_ms);
            }
            
            last_timeout_check = now_ms;
        }

        if (r == 0) continue;
        mt_evsock_drain(&disp->ev_passed);
        
        protopack *pkt;
        while (0 == prot_queue_pop(&disp->passed_pkts, &pkt) && atomic_load(&disp->is_running)){
            uint32_t c_uid = pkt->h_from;
            rudp_connection *connection = NULL;
            if (0 > rudp_get_connection(disp, c_uid, &connection)){
                // fprintf(stderr, "[rudp][disp][worker] error: no connection established with %u\n", c_uid);
                free(pkt);
                // prot_queue_push(&disp->passed_pkts, &pkt);
                continue;
            }

            _rudp_dispatcher_net_handler(disp, connection, udp_copy_pack(pkt), now_ms);
            now_ms = mt_time_get_millis_monocoarse();
            free(pkt);
        }
    }
    
    return NULL;
}

static void _rudp_dispatcher_net_handler(rudp_dispatcher *disp, rudp_connection *conn, protopack *pkt, int64_t now_ms){
    // printf("[neth] got pkt: %u seq (%s)\n", pkt->seq, PROTOPACK_TYPES_CHAR[pkt->packtype]);
    
    if (pkt->packtype == PACK_DATA){
        uint32_t pkt_seq = pkt->seq;
        uint32_t expected = (conn->last_recved_seq == UINT32_MAX) ? 0 : conn->last_recved_seq + 1;

        if (0 > _rudp_conn_pass_net(conn, pkt)) {
            free(pkt);   // only because ownership transfer failed
            return;
        }
        pkt = NULL; // ownership transferred

        bool should_send_ack = false;

        if (pkt_seq != expected) {
            should_send_ack = true;
        } else {
            _rudp_conn_reordering(conn);
        }

        conn->packets_since_ack++;

        // // printf("[neth] before: %u ack_t: %u\n", conn->packets_since_ack, conn->ack_threshold);
        if (conn->packets_since_ack >= conn->ack_threshold
            || now_ms - conn->last_ack_timestamp >= conn->avg_rtt_ms / 2) {
            should_send_ack = true;
        }
        
        if (now_ms - conn->last_ack_timestamp >= conn->ack_timeout_ms) {
            should_send_ack = true;
        }

        if (conn->last_ack_sent_seq == UINT32_MAX) {
            should_send_ack = true;
        }

        if (should_send_ack) {
            if (conn->last_recved_seq != UINT32_MAX) {
                protopack *msg = proto_msg_quick(conn->s_uid, conn->c_uid, conn->last_recved_seq, PACK_RACK);
                
                if (msg) {
                    pvd_sender_send(conn->sender, msg, &conn->nfd);
                    free(msg);
                    
                    uint32_t packets_count = conn->packets_since_ack;
                    conn->last_ack_sent_seq = conn->last_recved_seq;
                    conn->packets_since_ack = 0;
                    conn->last_ack_timestamp = now_ms;

                    if (conn->avg_rtt_ms < 30) {
                        conn->ack_threshold = (conn->ack_threshold < 16) ? conn->ack_threshold + 2 : 16;
                    } else if (conn->avg_rtt_ms > 100) {
                        conn->ack_threshold = (conn->ack_threshold > 2) ? conn->ack_threshold - 1 : 2;
                    }
                    
                    if (packets_count == 0 && conn->ack_threshold > 4) {
                        conn->ack_threshold = 4;
                    }

                    conn->ack_threshold = clamp(conn->ack_threshold, 2, 16);
                } else {
                    conn->packets_since_ack = 0;
                    conn->last_ack_timestamp = now_ms;
                    fprintf(stderr, "[rudp][disp][nhandler] error: failed to compose ACK\n");
                }
            }
        }

    } else if (pkt->packtype == PACK_RACK){
        prot_array_lock(&conn->pkts_fhost);
        
        int64_t rtt_sample = -1;
        
        for (size_t i = 0; i < conn->pkts_fhost.array.len; ){
            rudp_pending_pkt *ppkt = _prot_array_at_unsafe(&conn->pkts_fhost, i);
            
            if (ppkt->seq <= pkt->seq){
                if (ppkt->seq == pkt->seq && ppkt->retransmit_count == 0) {
                    rtt_sample = now_ms - ppkt->timestamp;
                }
                
                if (ppkt->copy_pack) free(ppkt->copy_pack);
                
                size_t last_idx = conn->pkts_fhost.array.len - 1;
                rudp_pending_pkt *last_pkt = _prot_array_at_unsafe(&conn->pkts_fhost, last_idx);
                *ppkt = *last_pkt;

                // printf("[neth] removing from %u seq -> %u (x = %u seq)\n", pkt->seq, ppkt->seq, ppkt_x->seq);
                _prot_array_remove_unsafe(&conn->pkts_fhost, last_idx);
                
            } else {
                i++;
            }
        }
        
        if (rtt_sample > 0 && rtt_sample < 10000) {
            if (rtt_sample < 20) rtt_sample = 20;
            
            if (rtt_sample > conn->avg_rtt_ms * 3 && conn->avg_rtt_ms > 50) {
                conn->avg_rtt_ms = (conn->avg_rtt_ms * 7 + rtt_sample) / 8;
            } else {
                conn->avg_rtt_ms = (conn->avg_rtt_ms * 3 + rtt_sample) / 4;
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
