#include <packproto/protomsgs.h>
#include <stdint.h>
#include <stdio.h>

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

protopack *proto_punch_msg(
    uint32_t hash_from,
    uint32_t hash_to,
    unsigned char info[88]
){
    return udp_make_pack(
        0, hash_from, hash_to, PACK_PUNCH,
        info, 88
    );
}

protopack *proto_msg_relay(
    protopack *og_packet,
    uint32_t   from_relay_to,
    uint32_t   relay_uid
){
    if (!og_packet) return NULL;
    protopack *retranslated = retranslate_udp(og_packet);

    char buf[2048] = {0};

    size_t out_sz = protopack_send(retranslated, buf);

    printf("[msgrel] out_sz: %zu, og_size: %u, retr_size: %u\n", out_sz, og_packet->d_size, retranslated->d_size);
    protopack* out = udp_make_pack(
        0,
        relay_uid, from_relay_to,
        PACK_NULL,
        buf, out_sz
    );

    free(retranslated);
    return out;
}
