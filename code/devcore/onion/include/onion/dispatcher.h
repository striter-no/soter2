#include "circut.h"
#include "rudp/_modules.h"
#include <gossip/system.h>

#ifndef ONION_DISPATCHER_H
#define ONION_DISPATCHER_H

typedef struct {
    prot_table circutes;
    prot_table current_connections;

    // requests TO peers
    prot_queue   request_packets;
    mt_eventsock request_ev;

    // responses FROM peers
    prot_queue   response_packets;
    mt_eventsock response_ev;

    // exit means you need to send it to the network or receive, if you are not EXIT node
    prot_queue   exit_packets;
    mt_eventsock exit_ev;

    gossip_system *gsp;
    peers_db      *pdb;
} onion_dispatcher;

// -- lifetime cycle
int onion_dispatcher_init(onion_dispatcher *disp, peers_db *pdb, gossip_system *gsp);
int onion_dispatcher_end (onion_dispatcher *disp);

// -- connection managment

int onion_connection_circgen(onion_dispatcher *disp, uint32_t circ_uid, int nodes_n);

bool onion_dispatcher_check  (onion_dispatcher *disp, uint32_t circ_uid);
int  onion_dispatcher_getcirc(onion_dispatcher *disp, uint32_t circ_uid, rele_circut *circ);
int  onion_dispatcher_getconn(onion_dispatcher *disp, uint32_t circ_uid, onion_connection *conn);

int onion_dispatcher_close(onion_dispatcher *disp, uint32_t circ_uid);
uint32_t onion_get_next_uid(onion_dispatcher *disp, dyn_array *excl);

// -- packet serving

// We are assuming that incoming packet is only an onion packet
int onion_dispatcher_serve_packet(onion_dispatcher *disp, protopack *pkt, const nnet_fd *from_whom);
int _onion_dispatcher_request    (onion_dispatcher *disp, protopack *pkt, const nnet_fd *to_whom);
int _onion_dispatcher_response   (onion_dispatcher *disp, protopack *pkt, const nnet_fd *from_whom);

int _onion_dispatcher_exit_packet(onion_dispatcher *disp, protopack *pkt, const nnet_fd *nfd);

#endif
