#include <base/prot_queue.h>
#include <base/prot_table.h>
#include <lownet/core.h>
#include <multithr/events.h>
#include <packproto/proto.h>
#include <peers/peerdb.h>
#include <stdint.h>
#include <providers/sender.h>
#include <sys/types.h>
#include "packets.h"

#ifndef TURN_HANDLERS_H
#define TURN_HANDLERS_H

#define TURN_COMMENTARY_MAX_SIZE 500

typedef struct {
    light_peer_info linfo_origin;
    light_peer_info linfo_connected;
} turn_bind_peer;

typedef enum {
    TURN_SST_OK,
    TURN_SST_FAIL,
    TURN_SST_SENT,
    TURN_SST_UNKNOWN
} turn_sys_status;

typedef struct {
    uint32_t TRUID; // for request UIDs
    uint32_t turn_sUID;

    mt_eventsock outgoing_ev;
    prot_queue   outgoing_tpacks;

    prot_table   binded_clients;  // clients binded inside from network
    prot_table   sys_requests;    // type: uint32_t(seq):turn_sys_status
    mt_eventsock sys_ev;          // when new sys_request arrives
    pvd_sender  *sender;
} turn_client;

uint32_t turn_pair_hash(uint32_t uid_from, uint32_t uid_to);
bool     turn_check_pair(turn_client *cli, uint32_t uid_from, uint32_t uid_to);

int turn_client_init(turn_client *cli, uint32_t turn_sUID, pvd_sender  *sender);
int turn_client_end (turn_client *cli);

int turn_client_bind(turn_client *cli, uint32_t hsh, const turn_bind_peer *peer);
int turn_client_info(turn_client *cli, uint32_t hsh, turn_bind_peer *out_peer);
int turn_client_unbind(turn_client *cli, uint32_t hsh);

// wrap real request to TURN request, address is plased in BIND call
int turn_client_wrap(turn_client *cli, protopack *pack, protopack **outgoing);

int turn_client_qsend(turn_client *cli, turn_request *req);

// unwrap TURN request from peer to normal one
int turn_client_unwrap(turn_client *cli, protopack *pack, protopack **outgoing);

// create system msg
turn_request* turn_client_req_sys(
    turn_client *cli,
    const naddr_t *to_whom, uint32_t to_UID,
    uint32_t TRUID,
    turn_sys_status status,
    const char *commentary
);

// unwrap system msg, automaticly notify client
int turn_client_unwrap_sys(turn_client *cli, const turn_request *request, turn_sys_status *status, char **commentary, uint32_t *truid);

// get request straight to address (if not behind NAT)
int turn_client_req_bind(turn_client *cli, turn_bind_peer bind_to, protopack **outgoing, uint32_t *out_TRUID);

// close connection with peer through TURN
int turn_client_req_unbind(turn_client *cli, turn_bind_peer close_with, protopack **outgoing, uint32_t *out_TRUID);

int turn_client_req_wait(turn_client *cli, uint32_t TRUID, int timeout_ms, turn_sys_status status);

#endif
