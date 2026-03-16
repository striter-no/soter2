#include <asm-generic/errno-base.h>
#include <rudp/channels.h>
#include <rudp/packets.h>

int rudp_channel_new(rudp_channel *c, nnet_fd *client_nfd, uint32_t self_uid, uint32_t client_uid){
    if (!c) return -1;
    
    if (0 > mt_evsock_new(&c->reordered_fd)) return -1;
    if (0 > mt_evsock_new(&c->pending_fd))   return -1;

    c->client_nfd = *client_nfd;
    c->client_uid = client_uid;
    c->self_uid   = self_uid;

    c->next_seq          = 0;
    c->last_ack_received = 0;
    c->last_ack_sent     = 0;

    c->last_recved_seq   = UINT32_MAX;
    prot_array_create(sizeof(rudp_pending_pkt), &c->pending_queue);
    prot_queue_create(sizeof(protopack*), &c->network_queue);
    prot_array_create(sizeof(protopack*), &c->reorder_buffer);
    prot_queue_create(sizeof(protopack*), &c->reoredered_queue);

    return 0;
}

int rudp_channel_end(rudp_channel *c){
    if (!c) return -1;

    mt_evsock_close(&c->reordered_fd);
    mt_evsock_close(&c->pending_fd);

    for (size_t i = 0; i < c->pending_queue.array.len; i++) {
        rudp_pending_pkt *ppkt = prot_array_at(&c->pending_queue, i);
        if (ppkt->copy_pack) free(ppkt->copy_pack);
    }
    prot_array_end(&c->pending_queue);

    protopack *pack;
    while (0 == prot_queue_pop(&c->network_queue, &pack)) {
        if (pack) free(pack);
    }
    prot_queue_end(&c->network_queue);

    while (0 == prot_queue_pop(&c->reoredered_queue, &pack)) {
        if (pack) free(pack);
    }
    prot_queue_end(&c->reoredered_queue);

    prot_array_end(&c->reorder_buffer);

    return 0;
}

int rudp_channel_wait(rudp_channel *c, int timeout){
    if (!c) return -1;
    return mt_evsock_wait(&c->reordered_fd, timeout);
}

int rudp_channel_pending_wait(rudp_channel *c, int timeout){
    if (!c) return -1;
    return mt_evsock_wait(&c->pending_fd, timeout);
}

int rudp_channel_send(pvd_sender *s, rudp_channel *c, protopack *p, nnet_fd *nfd){
    if (!s || !c || !p || !nfd) return -1;

    protopack *copy = udp_copy_pack(p);
    if (!copy) return -1;

    copy->seq = c->next_seq;

    rudp_pending_pkt pkt = {0};
    if (0 > rudp_pkt_make(&pkt, copy, RUDP_STATE_INITED, copy->seq, nfd)) {
        free(copy);
        return -1;
    }

    pkt.retransmit_count = 0;

    prot_array_lock(&c->pending_queue);
    prot_array_push(&c->pending_queue, &pkt);
    c->next_seq++;
    prot_array_unlock(&c->pending_queue);

    if (0 > pvd_sender_send(s, copy, nfd)) {
        // just send it later
        
        // prot_array_lock(&c->pending_queue);
        // for (size_t i = 0; i < c->pending_queue.array.len; ++i) {
        //     rudp_pending_pkt *ppkt = prot_array_at(&c->pending_queue, i);
        //     if (ppkt->seq == pkt.seq) {
        //         if (ppkt->copy_pack) free(ppkt->copy_pack);
        //         prot_array_remove(&c->pending_queue, i);
        //         break;
        //     }
        // }
        // prot_array_unlock(&c->pending_queue);
        free(copy);
        return -1;
    }

    mt_evsock_notify(&c->pending_fd);
    return 0;
}

int rudp_channel_recv(rudp_channel *c, protopack **p){
    if (!c) return -1;
    protopack *p_;
    if (0 > prot_queue_pop(&c->reoredered_queue, &p_)){
        return -1;
    }

    *p = udp_copy_pack(p_);
    free(p_);
    return 0;
} 
