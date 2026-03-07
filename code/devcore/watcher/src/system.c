#include <stdio.h>
#include <stdlib.h>
#include <watcher/system.h>

int watcher_init(watcher *w, pvd_sender *sender, pvd_listener *listener){
    if (!w || !sender || !listener) return -1;
    
    if (0 > mt_evsock_new(&w->newpack))
        return -1;

    prot_queue_create(sizeof(listener_packet), &w->passed_packets);
    w->sender = sender;
    w->daemon = 0;
    memset(w->handlers, 0, sizeof(w->handlers));

    atomic_store(&w->is_running, false);
    return 0;
}

void watcher_end(watcher *w){
    atomic_store(&w->is_running, false);
    pthread_join(w->daemon, NULL);

    listener_packet pck;
    while (0 == prot_queue_pop(&w->passed_packets, &pck)){
        if (!pck.pack) continue;
        free(pck.pack);
    }
    prot_queue_end(&w->passed_packets);
    mt_evsock_close(&w->newpack);
}

static void *watcher_worker(void *_args);
int watcher_start(watcher *w){
    if (!w) return -1;

    atomic_store(&w->is_running, true);
    int r = pthread_create(
        &w->daemon, NULL,
        watcher_worker, w
    );
    if (r < 0){
        atomic_store(&w->is_running, false);
        return r;
    }

    return 0;
}

int watcher_set_context(watcher *w, protopack_type type, void *ctx){
    if (!w) return -1;
    w->handlers[type].ctx = ctx;
    return 0;
}

int watcher_handler_reg(watcher *w, uint8_t type, watcher_handler handler){
    if (!w) return -1;
    if (type >= PACKET_TYPE_MAX) return -1;

    w->handlers[type] = handler;

    return 0;
}

int watcher_pass(watcher *w, protopack *pack, nnet_fd from_who){
    if (!w || !pack) return -1;
    listener_packet pkt = {
        .pack = pack,
        .from_who = from_who
    };
    if (0 > prot_queue_push(&w->passed_packets, &pkt))
        return -1;
    return mt_evsock_notify(&w->newpack);
}

// -- workers

static void *watcher_worker(void *_args){
    watcher *w = _args;

    while (atomic_load(&w->is_running)){
        int r = mt_evsock_wait(&w->newpack, 100);
        if (r == 0) continue;
        if (r < 0) {
            perror("poll()");
            continue;
        }

        printf("[watcher] got new event\n");

        listener_packet wpkt;
        if (0 > prot_queue_pop(&w->passed_packets, &wpkt)){
            fprintf(stderr, "failed to pop packet at watcher worker\n");
            continue;
        }

        protopack *pkt = wpkt.pack;
        if (!pkt) {
            fprintf(stderr, "passed packet is NULL, something wrong with memory\n");
            abort();
        }

        if (udp_is_RUDP_req(pkt->packtype)){
            fprintf(stderr, "passed packet to watcher is supposed to be passed to RUDP manager, not to watcher. Dropping\n");
            free(pkt);
            continue;
        }

        if (pkt->packtype >= PACKET_TYPE_MAX){
            fprintf(stderr, "passed packet has unknown pkt type: %d\n", pkt->packtype);
            free(pkt);
            continue;
        }

        if (w->handlers[pkt->packtype].foo){
            naddr_t addr = ln_nfd2addr(wpkt.from_who);

            printf("[watcher] calling handler for %d\n", pkt->packtype);
            printf("... [watcher] from_who: %u, %s:%u\n", wpkt.from_who.rfd, addr.ip.v4.ip, addr.ip.v4.port);
            w->handlers[pkt->packtype].foo(pkt, wpkt.from_who, w->sender, w->handlers[pkt->packtype].ctx);
        } else {
            printf("[watcher] no handler set for %d\n", pkt->packtype);
        }

        free(pkt);
    }

    return NULL;
}