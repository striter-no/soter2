#include <stdint.h>
#include <packproto/proto.h>
#include <lownet/core.h>

#ifndef RUDP_PACKETS_H

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

    int      retransmit_count;
    nnet_fd  nfd;
} rudp_pending_pkt;

int rudp_pkt_make(
    rudp_pending_pkt *out, 
    
    protopack     *pack, 
    rudp_pkt_state state,
    uint32_t       seq,
    nnet_fd       *nfd
);

typedef struct {
    protopack *pack;
    nnet_fd    nfd;
} rudp_wrpkt;

#endif
#define RUDP_PACKETS_H