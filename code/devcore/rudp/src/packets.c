#include "rudp/_modules.h"
#include <rudp/packets.h>

int rudp_pkt_make(
    rudp_pending_pkt *out, 
    
    protopack     *pack, 
    rudp_pkt_state state,
    uint32_t       seq
){
    if (!out || !pack) return -1;

    out->retransmit_count = 0;
    out->copy_pack = pack;
    out->state     = state;
    out->seq       = seq;
    out->timestamp = mt_time_get_millis_monocoarse();

    return 0;
}