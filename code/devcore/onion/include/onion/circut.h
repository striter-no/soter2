#include <lownet/core.h>
#include <peers/peerdb.h>
#include <e2ee/connection.h>

#include "packets.h"
#include "rudp/_modules.h"

#ifndef RELE_CIRCUT_H
#define RELE_CIRCUT_H

typedef struct {
    peer_info info;
    e2ee_connection *e2ee_conn;
} circut_peer;

// onion routing:
// 1. Get all peers IP:PORT:PUBKEY from gossip/manual
// 2. Select a random circut path
// 3. Send the dat through the circut, encoding each hop
typedef struct {
    uint32_t     circut_uid;
    size_t       reles_count;
    circut_peer *peers;

    prot_queue   inc_packets;
    mt_eventsock inc_ev;
} rele_circut;

// -- circut lifecycle
int rele_circut_new  (rele_circut *circut, uint32_t circut_uid);
int rele_circut_clear(rele_circut *circut);

// -- circut client
int rele_circut_extend(rele_circut *circut, peer_info *info);

int rele_circut_send_r(rele_circut *circut, size_t curr_peer, void *data, size_t d_size, onion_cmd cmd);
int rele_circut_send  (rele_circut *circut, size_t curr_peer, onion_packet *pkt);

int rele_circut_wait(rele_circut *circut, size_t curr_peer, int timeout);
int rele_circut_wait_any(rele_circut *circut, int timeout);
int rele_circut_recv(rele_circut *circut);

// -- private

int _rele_circut_pass(rele_circut *circut, size_t curr_peer, onion_packet *pkt);

#endif
