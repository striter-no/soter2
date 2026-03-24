#include "circut.h"
#include "onion/packets.h"

#ifndef ONION_CONNECTION_H
#define ONION_CONNECTION_H

typedef struct {
    uint32_t uid;
    nnet_fd  nfd;
} onion_peer;

typedef struct {
    // used only when onion_connection is middle node
    e2ee_connection prev_conn, next_conn;

    // universal part
    onion_peer requested_by;
    onion_peer next_hop;
    peers_db  *pdb;
} onion_connection;

int onion_connection_new(
    onion_connection *conn,
    rele_circut *circ,
    onion_peer req_by,
    peers_db *pdb
);

int onion_connection_free(onion_connection *conn);

int onion_connection_updcirc(
    onion_connection *conn,
    peer_info *snapshot,
    size_t snapshot_sz,
    int nodes_n,

    void *ctx,
    uint32_t (*next_uid_provider)(dyn_array *excl, void *ctx)
);

int onion_connection_forward(
    onion_connection *conn,
    const onion_packet *pkt,
    onion_packet *forwarded,

    bool         *no_forwarding
);

int onion_connection_backprop(
    onion_connection *conn,
    const onion_packet *pkt,
    onion_packet *backproped,

    bool         *no_backprop
);

int onion_connection_extend(
    onion_connection *conn,
    onion_peer new_peer
);

#endif
