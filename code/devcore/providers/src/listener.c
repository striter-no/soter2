#include <providers/listener.h>

int pvd_listener_new(pvd_listener *l, ln_socket *p_usocket){
    if (!l || !p_usocket) return -1;

    l->p_usocket = p_usocket;
    prot_queue_create(sizeof(listener_packet), &l->packets);
    if (0 > mt_evsock_new(&l->newpack_es)){
        return -1;
    }

    l->daemon = 0;
    atomic_store(&l->is_running, false);

    l->proxy = (proxy_if){0};
    return 0;
}

int pvd_listener_proxy(pvd_listener *l, proxy_if prx){
    if (!l) return -1;

    l->proxy = prx;
    return 0;
}

void pvd_listener_end(pvd_listener *l){
    if (!l) return;

    atomic_store(&l->is_running, false);
    pthread_join(l->daemon, NULL);

    mt_evsock_close(&l->newpack_es);
    l->p_usocket = NULL;

    listener_packet pkt;
    while (prot_queue_pop(&l->packets, &pkt) == 0){
        if (pkt.pack) free(pkt.pack);
    }
    prot_queue_end(&l->packets);
}

static void *pvd_listener_worker(void *_args);
int pvd_listener_start(pvd_listener *l){
    if (!l) return -1;
    atomic_store(&l->is_running, true);

    int r = pthread_create(&l->daemon, NULL, pvd_listener_worker, l);
    if (r < 0) return r;
    return 0;
}

int pvd_listener_pass(pvd_listener *l, const listener_packet *pkt){
    prot_queue_push(&l->packets, pkt);
    mt_evsock_notify(&l->newpack_es);

    return 0;
}

int pvd_next_packet(pvd_listener *l, listener_packet *pkt){
    if (!l) return -1;

    if (0 > prot_queue_pop(&l->packets, pkt)) {
        return -1;
    }

    return 0;
}

// worker
static void *pvd_listener_worker(void *_args){
    pvd_listener *listener = _args;

    nnet_fd from = {0};
    char    buf[2048] = {0};
    while (atomic_load(&listener->is_running)){
        int r = ln_wait_netfd(&listener->p_usocket->fd, POLLIN, 1000);
        if (r == 0) {continue;}
        if (r < 0)  {perror("poll()"); continue;}

        ssize_t recved = ln_usock_recv(listener->p_usocket, buf, 2048, &from);

        if (recved == 0) continue;
        if (recved < 0){
            perror("recvfrom()");
            continue;
        }

        protopack *pkt = protopack_recv(buf, recved);
        if (!pkt) {
            fprintf(stderr, "protopack_recv() failed\n");
            continue;
        }

        if (listener->proxy.prx){
            proxyfied prx;
            proxy_perform(&prx, &listener->proxy, pkt, &from);

            if (prx.drop_pkt){
                free(pkt);
                continue;
            }

            free(pkt);
            pkt = prx.proxyfied_pkt;
        }

        listener_packet lpkt = {pkt, from};
        // proto_print(pkt, 1);
        // printf("[pvd][listener] got new packet from %s:%u (%zu bytes): PKTTYPE: %d\n", addr.ip.v4.ip, addr.ip.v4.port, recved, pkt->packtype);
        prot_queue_push(&listener->packets, &lpkt);
        mt_evsock_notify(&listener->newpack_es);
    }

    return NULL;
}
