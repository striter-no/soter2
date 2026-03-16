#include <lownet/udp_sock.h>
#include <base/prot_queue.h>
#include <packproto/proto.h>
#include <multithr/events.h>
#include <multithr/time.h>
#include <stdatomic.h>
#include <pthread.h>

#ifndef PROVIDER_SENDER

typedef struct pvd_sender_pack pvd_sender_pack_t;

typedef struct {
    ln_usocket *p_usocket;
    prot_queue  packets;

    mt_eventsock newpack_es;
    pthread_t    daemon;
    atomic_bool  is_running;
} pvd_sender;

int  pvd_sender_new(pvd_sender *s, ln_usocket *p_usocket);
void pvd_sender_end(pvd_sender *s);
int  pvd_sender_start(pvd_sender *s);

int pvd_sender_send(pvd_sender *s, protopack *packet, nnet_fd *to);

#endif
#define PROVIDER_SENDER