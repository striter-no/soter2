#include <lownet/udp_sock.h>
#include <base/prot_queue.h>
#include <packproto/proto.h>
#include <multithr/events.h>
#include <multithr/time.h>
#include <stdatomic.h>
#include <pthread.h>

#ifndef PROVIDER_LISTENER

typedef struct {
    protopack *pack;
    nnet_fd    from_who;
} listener_packet;

typedef struct {
    ln_usocket *p_usocket;
    prot_queue  packets;
    atomic_bool is_running;

    mt_eventsock newpack_es;
    pthread_t    daemon;
} pvd_listener;

int pvd_listener_new(pvd_listener *l, ln_usocket *p_usocket);
void pvd_listener_end(pvd_listener *l);

int pvd_listener_start(pvd_listener *l);
listener_packet pvd_next_packet(pvd_listener *l);

#endif
#define PROVIDER_LISTENER