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
    s->packets = prot_queue_create(sizeof(pvd_sender_pack_t));
    atomic_store(&s->is_running, false);

    return 0;
}

void pvd_sender_end(pvd_sender *s){
    if (!s) return;
    atomic_store(&s->is_running, false);
    pthread_join(s->daemon, NULL);
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
        pvd_sender_worker,s
    );

    return r;
}


int pvd_sender_send(pvd_sender *s, protopack *packet, nnet_fd to){
    if (!s || !packet) return -1;

    protopack *pkt = udp_copy_pack(packet, false);
    pvd_sender_pack_t ppkt = {
        .pack = pkt,
        .to   = to
    };
    prot_queue_push(&s->packets, &ppkt);
    free(packet);

    return 0;
}

static void *pvd_sender_worker(void *_args){
    pvd_sender *sender = _args;
    
    while (atomic_load(&sender->is_running)){
        int r = mt_evsock_wait(&sender->newpack_es, 100);
        if (r == 0) continue;
        if (r < 0) {perror("poll()"); break;}

        pvd_sender_pack_t ppkt;
        if (0 != prot_queue_pop(&sender->packets, &ppkt)){
            fprintf(stderr, "prot_queue_pop() somehow failed\n");
            continue;
        }

        if (!ppkt.pack){
            fprintf(stderr, "pvd_sender_pack somehow has NULL rpack\n");
            continue;
        }

        char buf[2048];
        ssize_t s = protopack_send(ppkt.pack, buf);
        if (s < 0){
            fprintf(stderr, "protopack_send() failed");
            goto end;
        } else if (s > 2048){
            fprintf(stderr, "protopack_send() returned bigger data than expected, memory is polluted\n");
            abort();
        }

        ln_usock_send(sender->p_usocket, buf, s, ppkt.to);

end:
        free(ppkt.pack);
    }

    return NULL;
}