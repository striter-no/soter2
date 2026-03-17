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

int pvd_sender_send(pvd_sender *s, protopack *packet, nnet_fd *to){
    if (!s || !packet || !to) return -1;

    protopack *pkt = udp_copy_pack(packet);
    if (!pkt) return -1;

    pvd_sender_pack_t ppkt = {
        .pack = pkt,
        .to   = *to
    };

    prot_queue_push(&s->packets, &ppkt);

    if (0 > mt_evsock_notify(&s->newpack_es)) {}
    return 0;
}

static void *pvd_sender_worker(void *_args){
    pvd_sender *sender = _args;
    
    while (atomic_load(&sender->is_running)){
        int r = mt_evsock_wait(&sender->newpack_es, 100);
        if (r < 0) {
            perror("poll()");
        }
        if (r == 0) continue;
        
        pvd_sender_pack_t ppkt = {0};
        while (0 == prot_queue_pop(&sender->packets, &ppkt)) {
            if (!ppkt.pack) {
                fprintf(stderr, "pvd_sender_pack has NULL pack\n");
                continue;
            }

            char buf[2048] = {0};

            protopack *retranslated = retranslate_udp(ppkt.pack);
            if (!retranslated) {
                fprintf(stderr, "retranslate_udp() failed\n");
                free(ppkt.pack);
                continue;
            }

            int packtype = retranslated->packtype;
            ssize_t s = protopack_send(retranslated, buf);

            if (s < 0){
                fprintf(stderr, "protopack_send() failed\n");
                free(retranslated);
                free(ppkt.pack);
                continue;
            }

            if (s > (ssize_t)sizeof(buf)){
                fprintf(stderr, "protopack_send() returned bigger data than expected\n");
                free(retranslated);
                free(ppkt.pack);
                abort();
            }

            naddr_t addr = ln_nfd2addr(&ppkt.to);
            // printf("[pvd][sender] pkt: %s sending %zd bytes to %s:%u\n",
            //        PROTOPACK_TYPES_CHAR[packtype], s, addr.ip.v4.ip, addr.ip.v4.port);

            ln_usock_send(sender->p_usocket, buf, s, &ppkt.to);

            free(retranslated);
            free(ppkt.pack);
        }
    }

    return NULL;
}