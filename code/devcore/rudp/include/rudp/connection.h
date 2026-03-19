#include "_modules.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef RUDP_CONNECTION_H
#define RUDP_CONNECTION_H

#ifndef RUDP_TIMEOUT
#define RUDP_TIMEOUT 200
#endif

#ifndef RUDP_RETRANSMISSION_CAP
#define RUDP_RETRANSMISSION_CAP 10
#endif

#ifndef RUDP_REORDER_WINDOW
#define RUDP_REORDER_WINDOW 128
#endif

#ifndef RUDP_SEND_WINDOW
#define RUDP_SEND_WINDOW 128
#endif

typedef struct {
    pvd_sender  *sender;

    uint32_t s_uid; // self UID
    uint32_t c_uid; // connection UID (other UID)
    nnet_fd  nfd;

    mt_eventsock ev_host;
    mt_eventsock ev_net;
    mt_eventsock ev_reordered;

    prot_array   pkts_reorder_buf;
    prot_array   pkts_fhost;
    prot_queue   pkts_net;
    prot_queue   pkts_reordered;

    uint32_t     current_seq;
    uint32_t     last_sended_ack;
    uint32_t     last_recved_ack;
    uint32_t     last_recved_seq;

    uint32_t     last_ack_sent_seq;
    uint32_t     packets_since_ack;
    int64_t      last_ack_timestamp;
    
    uint32_t     ack_threshold;    
    int64_t      ack_timeout_ms;   
    int64_t      avg_rtt_ms;       

    int64_t      oldest_timestamp;
    bool         closed;
} rudp_connection;

// -- creation

int rudp_conn_new(
    rudp_connection *conn, 
    pvd_sender      *sender,
    uint32_t self_uid, 
    uint32_t conn_uid, 
    nnet_fd nfd
);
void rudp_conn_close(rudp_connection *conn);

// -- interface

int rudp_conn_send(rudp_connection *conn, protopack *pkt);

// On success returns packet ownership to caller.
// Caller must free(*pkt).
int rudp_conn_recv(rudp_connection *conn, protopack **pkt);
int rudp_conn_wait(rudp_connection *conn, int timeout);

size_t rudp_conn_inflight(rudp_connection *conn);
// -- private

int _rudp_conn_pass_net  (rudp_connection *conn, protopack *pkt);
int _rudp_conn_wait_net  (rudp_connection *conn, int timeout);
int _rudp_conn_wait_host (rudp_connection *conn, int timeout);
int _rudp_conn_reordering(rudp_connection *conn);
int _rudp_conn_timeouts  (rudp_connection *conn, int64_t now_ms);

#endif