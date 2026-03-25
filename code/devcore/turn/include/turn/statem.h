#include <base/prot_queue.h>
#include <base/prot_table.h>
#include <lownet/core.h>
#include <multithr/events.h>
#include <packproto/proto.h>
#include <peers/peerdb.h>

#ifndef TURN_HANDLERS_H
#define TURN_HANDLERS_H

typedef struct {
    uint32_t        UID_conn_to;
    light_peer_info linfo;
} turn_bind_peer;

typedef struct {
    uint32_t turn_sUID;

    mt_eventsock outgoing_ev;
    prot_queue   outgoing_tpacks;

    prot_table   binded_clients;  // clients binded inside from network
} turn_client;

int turn_client_init(turn_client *cli, uint32_t turn_sUID);
int turn_client_end (turn_client *cli);

int turn_client_bind(turn_client *cli, uint32_t client_UID, turn_bind_peer *peer);
int turn_client_unbind(turn_client *cli, uint32_t client_UID);

// wrap real request to TURN request, address is plased in BIND call
int turn_client_wrap(turn_client *cli, protopack *pack, protopack **outgoing);

// unwrap TURN request from peer to normal one
int turn_client_unwrap(turn_client *cli, protopack *pack, protopack **outgoing);

// get request straight to address (if not behind NAT)
int turn_client_req_bind(turn_client *cli, naddr_t bind_to, protopack **outgoing);

// close connection with peer through TURN
int turn_client_req_unbind(turn_client *cli, naddr_t close_with, protopack **outgoing);

#endif
