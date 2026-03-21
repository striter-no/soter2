#include "rudp/_modules.h"
#include "rudp/packets.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <packproto/protomsgs.h>
#include <rudp/connection.h>
#include <stdint.h>

// -- creation

int rudp_conn_new(
    rudp_connection *conn, 
    pvd_sender      *sender,
    uint32_t self_uid, 
    uint32_t conn_uid, 
    nnet_fd  nfd
){ // done
    if (!conn || !sender) return -1;

    if (0 > mt_evsock_new(&conn->ev_host))      return -1;
    if (0 > mt_evsock_new(&conn->ev_net))       return -1;
    if (0 > mt_evsock_new(&conn->ev_reordered)) return -1;
    
    if (0 > prot_queue_create(
        sizeof(protopack*), &conn->pkts_net
    )) return -1;
    
    if (0 > prot_queue_create(
        sizeof(protopack*), &conn->pkts_reordered
    )) return -1;

    if (0 > prot_array_create(
        sizeof(rudp_pending_pkt), &conn->pkts_fhost
    )) return -1;

    if (0 > prot_array_create(
        sizeof(protopack*), &conn->pkts_reorder_buf
    )) return -1;

    conn->c_uid = conn_uid;
    conn->s_uid = self_uid;
    conn->nfd   = nfd;

    conn->last_recved_ack = UINT32_MAX;
    conn->last_recved_seq = UINT32_MAX;
    conn->last_sended_ack = UINT32_MAX;

    conn->last_ack_sent_seq = UINT32_MAX;
    
    conn->ack_threshold = 2;
    conn->ack_timeout_ms = 2000;
    conn->avg_rtt_ms = 250;
    conn->packets_since_ack = 0;
    conn->last_ack_timestamp = mt_time_get_millis_monocoarse();

    conn->current_seq = 0;
    conn->sender = sender;
    conn->closed = false;

    conn->oldest_timestamp = INT64_MAX;
    return 0;
}

void rudp_conn_close(rudp_connection *conn){ // done
    if (!conn) return;

    // clear all remainings
    protopack *pkt = NULL;
    while (0 == prot_queue_pop(&conn->pkts_net, &pkt)){
        if (!pkt) continue;
        free(pkt);
    }

    while (0 == prot_queue_pop(&conn->pkts_reordered, &pkt)){
        if (!pkt) continue;
        free(pkt);
    }

    for (size_t i = 0; i < prot_array_len(&conn->pkts_fhost); i++){
        rudp_pending_pkt *pkt = prot_array_at(&conn->pkts_fhost, i);
        if (!pkt || !(pkt->copy_pack)) continue;
        free(pkt->copy_pack);
    }

    for (size_t i = 0; i < prot_array_len(&conn->pkts_reorder_buf); i++) {
        protopack **pp = prot_array_at(&conn->pkts_reorder_buf, i);
        if (pp && *pp) free(*pp);
    }

    // free containers
    prot_queue_end(&conn->pkts_net);
    prot_queue_end(&conn->pkts_reordered);
    prot_array_end(&conn->pkts_reorder_buf);
    prot_array_end(&conn->pkts_fhost);

    // close evsocks
    mt_evsock_close(&conn->ev_host);
    mt_evsock_close(&conn->ev_net);

    protopack *fin_pkt = proto_msg_quick(
        conn->s_uid, conn->c_uid, conn->current_seq, PACK_FIN
    );
    pvd_sender_send(conn->sender, fin_pkt, &conn->nfd);
    pvd_sender_send(conn->sender, fin_pkt, &conn->nfd);
    free(fin_pkt);

    conn->closed = true;
}

// -- interface

int rudp_conn_send(rudp_connection *conn, protopack *pkt){
    if (!conn || !pkt) return -1;
    if (conn->closed)  return ENOTCONN;

    prot_array_lock(&conn->pkts_fhost);

    // printf("[conn->pkts_fhost]: %zu\n", conn->pkts_fhost.array.len);
    if (conn->pkts_fhost.array.len >= RUDP_SEND_WINDOW) { 
        prot_array_unlock(&conn->pkts_fhost);
        return EAGAIN;
    }

    rudp_pending_pkt pending;
    protopack *pack_copy = udp_copy_pack(pkt);
    pack_copy->seq = conn->current_seq;

    if (0 > rudp_pkt_make(&pending, pack_copy, RUDP_STATE_INITED, conn->current_seq)){ // 
        free(pack_copy);
        prot_array_unlock(&conn->pkts_fhost);
        return -1;
    }

    pvd_sender_send(conn->sender, pack_copy, &conn->nfd);
    pending.state = RUDP_STATE_SENT;
    pending.send_timestamp = mt_time_get_millis_monocoarse(); 

    if (0 > _prot_array_push_unsafe(&conn->pkts_fhost, &pending)){
        prot_array_unlock(&conn->pkts_fhost);
        free(pack_copy);
        return -1;
    }

    conn->current_seq++;
    prot_array_unlock(&conn->pkts_fhost);
    return 0;
}

int rudp_conn_recv(rudp_connection *conn, protopack **pkt){
    if (!conn || !pkt) return -1;
    if (conn->closed) return ENOTCONN;

    return prot_queue_pop(&conn->pkts_reordered, pkt);
}

int rudp_conn_wait(rudp_connection *conn, int timeout){
    if (!conn || conn->closed) return -1;

    int r = mt_evsock_wait(&conn->ev_reordered, timeout);
    if (r > 0) mt_evsock_drain(&conn->ev_reordered);
    return r;
}

size_t rudp_conn_inflight(rudp_connection *conn){
    if (!conn || conn->closed) return SIZE_MAX;

    return prot_array_len(&conn->pkts_fhost);
}

// -- private

int _rudp_conn_pass_net(rudp_connection *conn, protopack *pkt){
    if (!conn || !pkt) return -1;

    if (0 > prot_queue_push(&conn->pkts_net, &pkt)){
        return -1;
    }

    mt_evsock_notify(&conn->ev_net);
    return 0;
}

int _rudp_conn_wait_net(rudp_connection *conn, int timeout){
    if (!conn) return -1;

    int r = mt_evsock_wait(&conn->ev_net, timeout);
    if (r > 0) mt_evsock_drain(&conn->ev_net);

    return r;
}

int _rudp_conn_wait_host(rudp_connection *conn, int timeout){ // done
    if (!conn) return -1;

    int r = mt_evsock_wait(&conn->ev_host, timeout);
    if (r > 0) mt_evsock_drain(&conn->ev_host);

    return r;
}

int _rudp_conn_reordering(rudp_connection *conn){ // done
    if (!conn) return -1;
    // printf("[_rudp_conn_reordering] called\n");

    // locking full queue
    prot_queue_lock(&conn->pkts_net);

    protopack *pkt;
    while (0 == _prot_queue_pop_unsafe(&conn->pkts_net, &pkt)){
        // printf("[_reord] pop called\n");
        if (!pkt) {
            // printf("[_reord] null pkt\n");
            continue;
        }

        uint32_t expected = (conn->last_recved_seq == UINT32_MAX) ? 0 : conn->last_recved_seq + 1;
        int32_t  diff     = (int32_t)(pkt->seq - expected);

        if (diff < 0 || diff > RUDP_REORDER_WINDOW){
            // printf("[reord] diff is too low/big: %d, expected/got: %u/%u \n", diff, expected, pkt->seq);
            free(pkt);
            continue;
        }

        prot_array_lock(&conn->pkts_reorder_buf);
        
        size_t pos = 0;
        bool duplicate = false;
        size_t len = conn->pkts_reorder_buf.array.len;

        for (pos = 0; pos < len; pos++) {
            protopack **existing = _prot_array_at_unsafe(&conn->pkts_reorder_buf, pos);
            if ((*existing)->seq == pkt->seq) {
                duplicate = true; 
                break;
            }

            if ((*existing)->seq > pkt->seq)
                break;
        }

        if (!duplicate){
            dyn_array_insert(&conn->pkts_reorder_buf.array, pos, &pkt);
        } else {
            // printf("[reord] duplicated\n");
            free(pkt);
        }
        
        prot_array_unlock(&conn->pkts_reorder_buf);
    }
    prot_queue_unlock(&conn->pkts_net);

    prot_array_lock(&conn->pkts_reorder_buf);
    while (conn->pkts_reorder_buf.array.len > 0) {
        protopack **p_ptr = dyn_array_at(&conn->pkts_reorder_buf.array, 0);
        protopack *p = *p_ptr;

        uint32_t next_needed = (conn->last_recved_seq == UINT32_MAX) ? 0 : conn->last_recved_seq + 1;
        // printf("[rudp] next_needed: %u, seq: %u, last_recved_seq: %u\n", next_needed, p->seq, conn->last_recved_seq);

        if (p->seq == next_needed) {
            uint32_t seq = p->seq;

            conn->last_recved_seq = seq;
            dyn_array_remove(&conn->pkts_reorder_buf.array, 0);

            prot_queue_push(&conn->pkts_reordered, &p);
            mt_evsock_notify(&conn->ev_reordered);
        } else {
            break;
        }
    }
    
    prot_array_unlock(&conn->pkts_reorder_buf);

    return 0;
}

int _rudp_conn_timeouts(rudp_connection *conn, int64_t now_ms){
    if (!conn) return -1;

    prot_array_lock(&conn->pkts_fhost); 

    size_t len = _prot_array_len_unsafe(&conn->pkts_fhost);
    if (len == 0) {
        prot_array_unlock(&conn->pkts_fhost);
        return 0;
    }

    for (size_t i = 0; i < len; ){
        rudp_pending_pkt *pkt = _prot_array_at_unsafe(&conn->pkts_fhost, i);
        if (!pkt || !pkt->copy_pack) {
            _prot_array_remove_unsafe(&conn->pkts_fhost, i);
            len--;
            continue;
        }

        if (pkt->retransmit_count >= RUDP_RETRANSMISSION_CAP){
            fprintf(stderr, "[timeouts] got retransmission cap for %u seq\n", pkt->seq);
            if (pkt->copy_pack) free(pkt->copy_pack);
    
            rudp_pending_pkt *last_pkt = _prot_array_at_unsafe(&conn->pkts_fhost, len - 1);
            *pkt = *last_pkt;
            
            _prot_array_remove_unsafe(&conn->pkts_fhost, len - 1);
            len--;
            
            conn->closed = true;
            prot_array_unlock(&conn->pkts_fhost);
            return -1;
        }

        int64_t backoff_ms = conn->avg_rtt_ms * (1.5 + (pkt->retransmit_count));
        int64_t dt_ms = now_ms - pkt->timestamp;

        if (dt_ms >= backoff_ms) {
            pvd_sender_send(conn->sender, pkt->copy_pack, &conn->nfd);
            pkt->timestamp = now_ms;
            pkt->retransmit_count++;
        }
        i++;
    }
    prot_array_unlock(&conn->pkts_fhost);
    return 0;
}
