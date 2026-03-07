#include <netinet/in.h>
#include <pthread.h>
#include <rudp/dispatcher.h>
#include <rudp/channels.h>
#include <rudp/packets.h>
#include <packproto/protomsgs.h>

int rudp_dispatcher_new(
    rudp_dispatcher *d, 
    pvd_sender *s,
    uint32_t self_uid
){
    if (!d || !s) return -1;

    if (0 > mt_evsock_new(&d->newchan_fd))  return -1;
    if (0 > mt_evsock_new(&d->newpack_fd))  return -1;
    if (0 > mt_evsock_new(&d->outgoing_fd)) return -1;

    d->sender = s;
    d->self_uid = self_uid;

    prot_queue_create(sizeof(rudp_wrpkt), &d->passed_packs);
    prot_queue_create(sizeof(rudp_wrpkt), &d->outgoing_packs);
    prot_table_create(
        sizeof(uint32_t), sizeof(rudp_channel*), DYN_OWN_BOTH, &d->channels
    );

    d->daemon = 0;
    atomic_store(&d->is_active, false);
    return 0;
}

void rudp_dispatcher_end(
    rudp_dispatcher *d
){
    if (!d) return;

    atomic_store(&d->is_active, false);
    pthread_join(d->daemon, NULL);

    for (size_t i = 0; i < d->channels.table.array.len; i++){
        dyn_pair *p = dyn_array_at(&d->channels.table.array, i);
        rudp_channel_end(*((rudp_channel**)p->second));
    }

    for (size_t i = 0; i < d->passed_packs.arr.array.len; i++){
        rudp_wrpkt pack;
        prot_queue_pop(&d->passed_packs, &pack);
        free(pack.pack);
    }

    prot_queue_end(&d->passed_packs);
    prot_queue_end(&d->outgoing_packs);
    prot_table_end(&d->channels);

    mt_evsock_close(&d->newchan_fd);
    mt_evsock_close(&d->newpack_fd);
    mt_evsock_close(&d->outgoing_fd);
}

static void *rudp_dispatcher_worker(void *_args);
int rudp_dispatcher_run(
    rudp_dispatcher *d
){
    if (!d) return -1;

    atomic_store(&d->is_active, true);
    int r = pthread_create(&d->daemon, NULL, rudp_dispatcher_worker, d);
    if (r < 0){
        atomic_store(&d->is_active, false);
        return r;
    }

    return 0;
}

int rudp_dispatcher_pass(
    rudp_dispatcher *d,
    protopack       *pack,
    nnet_fd          from
){
    if (!d || !pack) return -1;

    prot_queue_push(&d->passed_packs, &(rudp_wrpkt){
        .nfd  = from,
        .pack = pack
    });

    mt_evsock_notify(&d->newpack_fd);

    return 0;
}

int rudp_dispatcher_send(
    rudp_dispatcher *d,
    protopack       *pack,
    nnet_fd          to
){
    if (!d || !pack) return -1;

    prot_queue_push(&d->outgoing_packs, &(rudp_wrpkt){
        .nfd  = to,
        .pack = pack
    });

    mt_evsock_notify(&d->outgoing_fd);

    return 0;
}

// -- channels
int rudp_dispatcher_chan_new(
    rudp_dispatcher *d,
    nnet_fd client_nfd,
    uint32_t client_uid
){
    if (!d) return -1;
    prot_table_lock(&d->channels);

    rudp_channel **existing_ = prot_table_get(&d->channels, &client_uid);
    rudp_channel *existing = existing_? *existing_: NULL;
    
    if (existing != NULL){
        existing->client_nfd = client_nfd;
        prot_table_unlock(&d->channels);
        mt_evsock_notify(&d->newchan_fd);
        return 0;
    }

    rudp_channel *c = malloc(sizeof(rudp_channel));
    if (0 > rudp_channel_new(c, client_nfd, client_uid)){
        prot_table_unlock(&d->channels);
        return -1;
    }

    prot_table_set(&d->channels, &client_uid, &c);
    prot_table_unlock(&d->channels);

    mt_evsock_notify(&d->newchan_fd);
    return 0;
}

int rudp_dispatcher_chan_get(
    rudp_dispatcher *d,
    uint32_t         client_uid,
    rudp_channel    **channel
){
    if (!d) return -1;

    // printf("[rudp_dispatcher_chan_get] before\n");
    rudp_channel **chan = ((rudp_channel**)prot_table_get(
        &d->channels, 
        &client_uid
    ));
    // printf("[rudp_dispatcher_chan_get] after\n");

    if (!chan || !(*chan)) return -1;
    *channel = *chan;

    return 0;
}

int rudp_dispatcher_chan_wait(
    rudp_dispatcher *d,
    int timeout
){
    if (!d) return -1;
    return mt_evsock_wait(&d->newchan_fd, timeout);
}

// -- workers

static void rudp_sender_worker(rudp_dispatcher *disp);
static void rudp_reader_worker(rudp_dispatcher *disp);

static void rudp_check_timeouts(rudp_dispatcher *disp);
static void rudp_check_timeouts_chan(rudp_dispatcher *disp, rudp_channel *chan);

static void rudp_reordering(rudp_dispatcher *disp);
static void rudp_reordering_chan(rudp_channel *chan);

static void *rudp_dispatcher_worker(void *_args){
    rudp_dispatcher *disp = _args;
    
    struct pollfd fds[2] = {
        {.fd = disp->newpack_fd.pfd.fd, .events = POLLIN}, 
        {.fd = disp->outgoing_fd.pfd.fd, .events = POLLIN}
    };
    while (atomic_load(&disp->is_active)){
        int r = poll(fds, 2, 200);

        // printf("[rudp worker] check timeouts: %d\n", atomic_load(&disp->is_active));
        rudp_check_timeouts(disp);

        // printf("[rudp worker] reordering\n");
        rudp_reordering(disp);

        // printf("[rudp worker] workers...\n");

        if (r < 0)  {perror("rudp_dispactcher:poll()"); continue;}
        if (r == 0) continue;

        if (fds[0].revents & POLLIN) rudp_reader_worker(disp);
        if (fds[1].revents & POLLIN) rudp_sender_worker(disp);

        // printf("[rudp worker] next iter\n");
    }

    return NULL;
}

static void rudp_reordering(rudp_dispatcher *disp){
    prot_table_lock(&disp->channels);
    for (size_t i = 0; i < disp->channels.table.array.len; i++){
        dyn_pair *p = dyn_array_at(&disp->channels.table.array, i);
        rudp_reordering_chan(*((rudp_channel**)p->second));
    }
    prot_table_unlock(&disp->channels);
}

static void rudp_reordering_chan(rudp_channel *chan){
    prot_queue_lock(&chan->network_queue);
    protopack *pack;

    while (0 == prot_queue_pop(&chan->network_queue, &pack)){        
        if (!pack) break;

        uint32_t expected = (chan->last_recved_seq == UINT32_MAX) ? 0 : chan->last_recved_seq + 1;
        uint32_t diff = pack->seq - expected;

        if (diff > RUDP_REORDER_WINDOW && diff < (0xFFFFFFFF - RUDP_REORDER_WINDOW)) {
            free(pack);
            continue;
        }

        prot_array_lock(&chan->reorder_buffer);
        
        size_t pos = 0;
        bool duplicate = false;
        size_t len = chan->reorder_buffer.array.len;

        for (pos = 0; pos < len; pos++) {
            protopack **existing = dyn_array_at(&chan->reorder_buffer.array, pos);
            if ((*existing)->seq == pack->seq) {
                duplicate = true; 
                break;
            }
            if ((*existing)->seq > pack->seq) {
                break;
            }
        }

        if (!duplicate)
            dyn_array_insert(&chan->reorder_buffer.array, pos, &pack);
        else
            free(pack);
        
        prot_array_unlock(&chan->reorder_buffer);
    }
    prot_queue_unlock(&chan->network_queue);

    prot_array_lock(&chan->reorder_buffer);
    
    while (chan->reorder_buffer.array.len > 0) {
        protopack **p_ptr = dyn_array_at(&chan->reorder_buffer.array, 0);
        protopack *p = *p_ptr;

        uint32_t next_needed = (chan->last_recved_seq == UINT32_MAX) ? 0 : chan->last_recved_seq + 1;

        if (p->seq == next_needed) {
            prot_queue_push(&chan->reoredered_queue, &p);
            chan->last_recved_seq = p->seq;

            dyn_array_remove(&chan->reorder_buffer.array, 0);
            mt_evsock_notify(&chan->reordered_fd);

        } else {
            break; 
        }
    }
    
    prot_array_unlock(&chan->reorder_buffer);
}

static void rudp_check_timeouts(rudp_dispatcher *disp){
    prot_table_lock(&disp->channels);
    for (size_t i = 0; i < disp->channels.table.array.len; i++){
        dyn_pair *p = dyn_array_at(&disp->channels.table.array, i);
        rudp_check_timeouts_chan(disp, *((rudp_channel**)p->second));
    }
    prot_table_unlock(&disp->channels);
}

static void rudp_check_timeouts_chan(rudp_dispatcher *disp, rudp_channel *chan){
    prot_array_lock(&chan->pending_queue);
    
    uint32_t currt = time(NULL);
    for (size_t i = 0; i < chan->pending_queue.array.len;){
        rudp_pending_pkt *pkt = dyn_array_at(
            &chan->pending_queue.array, i
        );

        if (pkt->retransmit_count >= RUDP_RETRANSMISSION_CAP){
            free(pkt->copy_pack);
            dyn_array_remove(&chan->pending_queue.array, i);
            // SLOG_WARNING("[rudpdisp] retransmission cap hit");
            continue;
        }

        if (currt - pkt->timestamp >= RUDP_TIMEOUT){
            pvd_sender_send(disp->sender, pkt->copy_pack, pkt->nfd);

            pkt->timestamp = currt;
            pkt->retransmit_count++;
            i++;
            continue;
        } else if (pkt->retransmit_count == 0) {
            pvd_sender_send(disp->sender, pkt->copy_pack, pkt->nfd);
            chan->next_seq++;
            pkt->retransmit_count++;
        }
        i++;
    }

    prot_array_unlock(&chan->pending_queue);
}

static void rudp_sender_worker(rudp_dispatcher *disp){
    rudp_wrpkt pkt = {0};
    if (0 > prot_queue_pop(&disp->outgoing_packs, &pkt) || !pkt.pack)
        return;

    naddr_t addr = ln_nfd2addr(pkt.nfd);
    // printf("[rudp_sender_worker] addr: %s:%u\n", addr.ip.v4.ip, addr.ip.v4.port);
    protopack *rpack = pkt.pack;

    uint32_t peer_id = rpack->h_to;
    rudp_channel *chan = NULL;
    if (0 > rudp_dispatcher_chan_get(disp, peer_id, &chan)){
        // printf("[rudp_sender_worker] creating new channel\n");
        rudp_dispatcher_chan_new(disp, pkt.nfd, peer_id);
        // printf("[rudp_sender_worker] created channel\n");

        rudp_dispatcher_chan_get(disp, peer_id, &chan);
    }

    rudp_channel_send(chan, rpack, pkt.nfd);
}

static void rudp_reader_worker(rudp_dispatcher *disp){
    rudp_wrpkt pkt = {0};
    if (0 > prot_queue_pop(&disp->passed_packs, &pkt) || !pkt.pack)
        return;

    rudp_channel *chan = NULL;
    if (0 > rudp_dispatcher_chan_get(disp, pkt.pack->h_from, &chan)){
        // printf("[rudp_reader_worker] created new channel\n");
        rudp_dispatcher_chan_new(disp, pkt.nfd, pkt.pack->h_from);
        rudp_dispatcher_chan_get(disp, pkt.pack->h_from, &chan);
    }

    // freeing packet inside handlers
    protopack *rpack = pkt.pack;
    if (rpack->packtype == PACK_DATA){
        uint32_t seq = rpack->seq;
        prot_queue_push(&chan->network_queue, &rpack);
        mt_evsock_notify(&chan->network_fd);
        chan->last_ack_sent = seq;

        // sending auto-ack
        pvd_sender_send(disp->sender, proto_msg_quick(
            disp->self_uid, rpack->h_from, seq, PACK_RACK
        ), pkt.nfd);

    } else if (rpack->packtype == PACK_RACK){
        prot_array_lock(&chan->pending_queue);
        
        bool was_ack = false;
        for (size_t i = 0; i < chan->pending_queue.array.len;){
            rudp_pending_pkt *ppkt = prot_array_at(&chan->pending_queue, i);
            if (ppkt->seq != rpack->seq){ i++; continue;}
            
            chan->last_ack_received = rpack->seq;
            // free(ppkt->copy_pack); <-- it is freed in sending place
            prot_array_remove(&chan->pending_queue, i);
            was_ack = true;
            break;
        }

        if (!was_ack){
            fprintf(stderr, "[warn] ACK was recved, but no suitable SEQ was found: %u", rpack->seq);
        }

        prot_array_unlock(&chan->pending_queue);
        free(rpack);
    } else {
        fprintf(stderr, "to worker passed unknown packet w/ type %d\n", rpack->packtype);
        free(rpack);
    }
}
