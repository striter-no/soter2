#include "proto.h"
#include <stdint.h>

#ifndef PROTO_PACKPROTO_MSGS
#define PROTO_ADDRSIZE (sizeof(uint32_t) + sizeof(uint16_t))

protopack *proto_msg_syst(
    uint32_t hash_from,
    uint32_t hash_to,
    char     addr_port[PROTO_ADDRSIZE],
    protopack_type type
);

protopack *proto_msg_quick(
    uint32_t hash_from,
    uint32_t hash_to,
    uint32_t seq,
    protopack_type type
);

protopack *proto_msg_relay(
    protopack *og_packet,
    uint32_t   from_relay_to,
    uint32_t   relay_uid
);

protopack *proto_punch_msg(
    uint32_t hash_from,
    uint32_t hash_to,
    unsigned char info[140]
);

#endif
#define PROTO_PACKPROTO_MSGS
