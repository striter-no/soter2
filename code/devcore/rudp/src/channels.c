#include <rudp/channels.h>
#include <rudp/packets.h>

int rudp_channel_new(rudp_channel *c, nnet_fd client_nfd, uint32_t client_uid){
    if (!c) return -1;
    
    if (0 > mt_evsock_new(&c->reordered_fd)) return -1;
    if (0 > mt_evsock_new(&c->network_fd))   return -1;
    if (0 > mt_evsock_new(&c->pending_fd))    return -1;

    c->client_nfd = client_nfd;
    c->client_uid = client_uid;

    c->next_seq          = 0;
    c->last_ack_received = 0;
    c->last_ack_sent     = 0;

    c->last_recved_seq   = UINT32_MAX;
    c->pending_queue     = prot_array_create(sizeof(rudp_pending_pkt*));
    c->network_queue     = prot_queue_create(sizeof(protopack*));
    c->reorder_buffer    = prot_array_create(sizeof(protopack*));
    c->reoredered_queue  = prot_queue_create(sizeof(protopack*));

    return 0;
}

int rudp_channel_end(rudp_channel *c){
    if (!c) return -1;

    mt_evsock_close(&c->network_fd);
    mt_evsock_close(&c->reordered_fd);
    mt_evsock_close(&c->pending_fd);

    for (size_t i = 0; i < c->network_queue.arr.array.len; i++){
        protopack *pack;
        prot_queue_pop(&c->network_queue, &pack);

        free(pack);
    }

    for (size_t i = 0; i < c->pending_queue.array.len; i++){
        free(((rudp_pending_pkt*)prot_array_at(&c->pending_queue, i))->copy_pack);
    }

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

int rudp_channel_send(rudp_channel *c, protopack *p, nnet_fd nfd){
    if (!c) return -1;

    rudp_pending_pkt pkt;
    rudp_pkt_make(&pkt, udp_copy_pack(p, true), RUDP_STATE_INITED, 0, nfd);
    free(p);

    prot_array_push(&c->pending_queue, &pkt);
    mt_evsock_notify(&c->pending_fd);
    return 0;
}

int rudp_channel_recv(rudp_channel *c, protopack *p){
    if (!c) return -1;
    return prot_queue_pop(&c->reoredered_queue, &p);
} 
