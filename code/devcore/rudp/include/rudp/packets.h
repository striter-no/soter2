#include "_modules.h"

#ifndef RUDP_PACKETS_H
#define RUDP_PACKETS_H

typedef enum {
    RUDP_STATE_INITED,
    RUDP_STATE_SENT,
    RUDP_STATE_ACKED,
    RUDP_STATE_LOST
} rudp_pkt_state;

typedef struct {
    uint32_t        seq;
    int64_t         timestamp;
    protopack      *copy_pack;
    rudp_pkt_state  state;

    int retransmit_count;
} rudp_pending_pkt;

int rudp_pkt_make(
    rudp_pending_pkt *out, 
    
    protopack     *pack, 
    rudp_pkt_state state,
    uint32_t       seq
);

#endif