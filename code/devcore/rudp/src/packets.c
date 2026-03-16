#include <rudp/packets.h>
#include <multithr/time.h>

int rudp_pkt_make(
    rudp_pending_pkt *out, 
    
    protopack     *pack, 
    rudp_pkt_state state,
    uint32_t       seq,
    nnet_fd       *nfd
){
    if (!pack) return -1;

    out->copy_pack = pack;
    out->retransmit_count = 0;
    out->state = state;
    out->seq = seq;
    out->timestamp = mt_time_get_millis();
    out->nfd = *nfd;

    return 0;
}