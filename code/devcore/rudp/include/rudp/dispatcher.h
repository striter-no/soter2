#include <providers/sender.h>
#include <providers/listener.h>
#include <base/prot_table.h>
#include "channels.h"

#ifndef RUDP_DISPATCHER_H
#define RUDP_RETRANSMISSION_CAP 10
#define RUDP_TIMEOUT            10
#define RUDP_REORDER_WINDOW     5

typedef struct {
    pvd_sender   *sender;

    prot_table    channels;
    prot_queue    passed_packs;
    prot_queue    outgoing_packs;

    mt_eventsock  outgoing_fd;
    mt_eventsock  newpack_fd;
    mt_eventsock  newchan_fd;

    pthread_t     daemon;
    atomic_bool   is_active;

    uint32_t      self_uid;
} rudp_dispatcher;

int rudp_dispatcher_new(
    rudp_dispatcher *d, pvd_sender *s, uint32_t self_uid
);

void rudp_dispatcher_end(
    rudp_dispatcher *d
);

int rudp_dispatcher_run(
    rudp_dispatcher *d
);

// -- channels
int rudp_dispatcher_chan_new(
    rudp_dispatcher *d,
    nnet_fd client_nfd,
    uint32_t client_uid
);

int rudp_dispatcher_chan_get(
    rudp_dispatcher *d,
    uint32_t client_uid,
    rudp_channel    **channel
);

int rudp_dispatcher_chan_wait(
    rudp_dispatcher *d,
    int timeout
);

#endif
#define RUDP_DISPATCHER_H