#include "_modules.h"
#include "connection.h"

#ifndef RUDP_DISPATCHER_H
#define RUDP_DISPATCHER_H

typedef struct {
    pvd_sender  *sender;
    prot_queue   passed_pkts;
    mt_eventsock ev_passed;

    prot_table  connections;
    uint32_t    self_uid;

    pthread_t   daemon;
    atomic_bool is_running;
} rudp_dispatcher;

// -- creation
int rudp_dispatcher_new(
    rudp_dispatcher *disp, 
    pvd_sender *sender,
    uint32_t self_uid
);

int rudp_dispatcher_end(rudp_dispatcher *disp);

// -- interface

int rudp_est_connection(
    rudp_dispatcher *disp, 
    rudp_connection **out_conn,
    uint32_t other_UID,
    nnet_fd  *nfd
);

int rudp_get_connection(
    rudp_dispatcher *disp, 
    uint32_t UID,
    rudp_connection **conn
);

int rudp_close_conncetion(
    rudp_dispatcher *disp, 
    uint32_t UID
);

// -- system

int rudp_dispatcher_pass(
    rudp_dispatcher *disp, 
    protopack *pkt
);

int rudp_dispatcher_run(
    rudp_dispatcher *disp
);

#endif