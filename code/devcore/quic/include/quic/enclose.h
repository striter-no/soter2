/*
 * quic:enclose
 * module for wrapping UDP and TCP connections in QUIC protocol
 */

#ifndef ENCLOSE_HEADER_H
#define ENCLOSE_HEADER_H

#include <quic/quic.h>
#include <lownet/core.h>
#include <base/prot_table.h>
#include <multithr/events.h>

typedef enum {
    ENCLOSE_PROTO_UDP,
    ENCLOSE_PROTO_TCP
} enclose_proto_t;

typedef enum {
    ENCLOSE_STATE_INIT,
    ENCLOSE_STATE_CONNECTING,
    ENCLOSE_STATE_ESTABLISHED,
    ENCLOSE_STATE_CLOSING
} enclose_state_t;

typedef struct {
    uint64_t   stream_id;
    enclose_state_t state;

    prot_queue tx_queue; // from socket to QUIC
    prot_queue rx_queue; // from QUIC to socket

    void *ctx;
} enclose_tunnel;

typedef struct {
    prot_table   tunnels;  // key: hash(addr+port+proto), value: enclose_tunnel*
    mt_eventsock data_ready_ev;
    uint64_t     next_tunnel_id;

    bool is_server;
    int (*f_TUNNEL_read) (void *out_buf,      size_t read_sz);
    int (*f_TUNNEL_write)(const void *in_buf, size_t write_sz);
} enclose_core;

int enclose_init(enclose_core *core, bool is_server);
int enclose_clear(enclose_core *core);

int enclose_tunnel_open(enclose_core *core, naddr_t *addr,
                        enclose_proto_t proto, uint64_t *tunnel_id);
int enclose_tunnel_close(enclose_core *core, uint64_t tunnel_id);

// write to tx_queue (SOCKET->PROTO)
int enclose_write(enclose_core *core, uint64_t tunnel_id,
                  const uint8_t *data, size_t len);

// read from rx_queue (PROTO->SOCKET)
int enclose_read(enclose_core *core, uint64_t tunnel_id,
                 uint8_t *buf, size_t *len, int timeout);

#endif
