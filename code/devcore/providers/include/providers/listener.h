#include <lownet/udp_sock.h>
#include <base/prot_queue.h>
#include <packproto/proto.h>
#include <multithr/events.h>
#include <multithr/time.h>
#include <stdatomic.h>
#include <pthread.h>

#include "proxy.h"

#ifndef PROVIDER_LISTENER

typedef struct {
    protopack *pack;
    nnet_fd    from_who;
} listener_packet;

typedef struct {
    ln_socket *p_usocket;
    prot_queue  packets;
    atomic_bool is_running;

    mt_eventsock newpack_es;
    pthread_t    daemon;

    proxy_if     proxy;
} pvd_listener;

int  pvd_listener_new(pvd_listener *l, ln_socket *p_usocket);
void pvd_listener_end(pvd_listener *l);
int  pvd_listener_proxy(pvd_listener *l, proxy_if prx);

int pvd_listener_start(pvd_listener *l);
int pvd_next_packet(pvd_listener *l, listener_packet *pkt);

#endif
#define PROVIDER_LISTENER
