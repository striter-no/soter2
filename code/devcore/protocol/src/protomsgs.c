#include <packproto/protomsgs.h>
#include <stdint.h>

protopack *proto_msg_syst(
    uint32_t hash_from,
    uint32_t hash_to,
    char     addr_port[PROTO_ADDRSIZE],
    protopack_type type
){
    return udp_make_pack(
        0, hash_from, hash_to, type, 
        addr_port, PROTO_ADDRSIZE
    );
}

protopack *proto_msg_quick(
    uint32_t hash_from,
    uint32_t hash_to,
    uint32_t seq,
    protopack_type type
){
    return udp_make_pack(
        seq, hash_from, hash_to, type, 
        NULL, 0
    );
}

protopack *proto_msg_relay(
    protopack *og_packet,
    uint32_t   from_relay_to,
    uint32_t   relay_uid
){
    if (!og_packet) return NULL;

    return udp_make_pack(
        0, 
        relay_uid, from_relay_to, 
        PACK_RELAYED, 
        og_packet, 
        sizeof(protopack) + og_packet->d_size
    );
}