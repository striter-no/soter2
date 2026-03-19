#include <providers/sender.h>

struct pvd_sender_pack {
    protopack *pack;
    nnet_fd    to;
};

int pvd_sender_new(pvd_sender *s, ln_usocket *p_usocket){
    if (!s || !p_usocket) return -1;
    
    if (0 > mt_evsock_new(&s->newpack_es)){
        return -1;
    }
    
    s->p_usocket = p_usocket;
    s->daemon  = 0;
    prot_queue_create(sizeof(pvd_sender_pack_t), &s->packets);
    atomic_store(&s->is_running, false);

    return 0;
}

void pvd_sender_end(pvd_sender *s){
    if (!s) return;

    atomic_store(&s->is_running, false);

    if (s->daemon) {
        pthread_join(s->daemon, NULL);
    }

    mt_evsock_close(&s->newpack_es);
    s->p_usocket = NULL;
    
    pvd_sender_pack_t pkt;
    while (prot_queue_pop(&s->packets, &pkt) == 0){
        if (pkt.pack) free(pkt.pack);
    }
    prot_queue_end(&s->packets);
}

static void *pvd_sender_worker(void *_args);

int pvd_sender_start(pvd_sender *s){
    if (!s) return -1;

    atomic_store(&s->is_running, true);
    int r = pthread_create(
        &s->daemon, NULL,
        pvd_sender_worker, s
    );

    if (r != 0) {
        atomic_store(&s->is_running, false);
    }

    return r;
}

int pvd_sender_send(pvd_sender *s, protopack *packet, const nnet_fd *to){
    if (!s || !packet || !to) return -1;

    protopack *pkt = udp_copy_pack(packet);
    if (!pkt) return -1;

    pvd_sender_pack_t ppkt = {
        .pack = pkt,
        .to   = *to
    };

    prot_queue_lock(&s->packets);
    size_t qlen = s->packets.arr.array.len;
    _prot_queue_push_unsafe(&s->packets, &ppkt);
    prot_queue_unlock(&s->packets);

    // if (qlen == 0) {
        mt_evsock_notify(&s->newpack_es);
    // }
    return 0;
}

static void *pvd_sender_worker(void *_args){
    pvd_sender *sender = _args;
    
    while (atomic_load(&sender->is_running)){
        int r = mt_evsock_wait(&sender->newpack_es, 1000);
        if (r < 0) {
            perror("poll()");
        }
        if (r == 0) continue;
        
        pvd_sender_pack_t ppkt = {0};
        // prot_queue_lock(&sender->packets);
        while (prot_queue_pop(&sender->packets, &ppkt) == 0) {
            if (!ppkt.pack) {
                fprintf(stderr, "pvd_sender_pack has NULL pack\n");
                continue;
            }

            char buf[2048] = {0};
            protopack *pkt = ppkt.pack;
            // proto_print(pkt, 0);

            size_t d_size = pkt->d_size;
            size_t total_size = sizeof(protopack) + d_size;

            if (total_size > sizeof(buf)) {
                fprintf(stderr, "packet too large\n");
                free(pkt);
                // abort();
                continue;
            }

            pkt->seq      = htonl(pkt->seq);
            pkt->d_size   = htonl(pkt->d_size);
            pkt->h_from   = htonl(pkt->h_from);
            pkt->h_to     = htonl(pkt->h_to);
            pkt->chsum    = 0;

            memcpy(buf, pkt, total_size);

            uint32_t csum = crc32(buf, total_size);
            ((protopack*)buf)->chsum = htonl(csum);

            ssize_t s = total_size;
            ln_usock_send(sender->p_usocket, buf, s, &ppkt.to);

            free(pkt);
        }

        // prot_queue_unlock(&sender->packets);
    }

    return NULL;
}