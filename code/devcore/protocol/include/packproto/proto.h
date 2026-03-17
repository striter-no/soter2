#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef PROTOCOL_PACKPROTO_H

typedef enum: uint8_t {
    PACK_DATA,      // RUDP
    PACK_ACK,       // no RUDP (natpunching module)
    PACK_PING,      // no RUDP (integrety doesnt matter)
    PACK_PONG,      // no RUDP (integrety doesnt matter)
    PACK_PUNCH,     // no RUDP (natpunching module)
    PACK_GOSSIP,    // no RUDP (integrety doesnt matter)
    PACK_STATE,     // no RUDP
    PACK_RACK,      // RUDP ACK
    PACK_FIN,       // RUDP FIN
    __PROTOPACK_END
} protopack_type;

extern const char *PROTOPACK_TYPES_CHAR[];

#define PACKET_TYPE_MAX ((size_t)__PROTOPACK_END)

bool udp_is_RUDP_req(protopack_type type);

typedef struct __attribute__((packed)) {
    uint32_t seq;
    uint32_t chsum;
    uint32_t d_size;
    uint32_t h_from; // from who
    uint32_t h_to;   // to whom
    uint8_t  packtype;
    uint8_t  data[];
} protopack;

protopack *udp_make_pack(
    uint32_t  sequence_n,
    uint32_t  hash_from,
    uint32_t  hash_to,
    protopack_type type,

    void     *payload,
    size_t    payload_size
);

protopack *udp_copy_pack(protopack *pk); // bool apply_ntoh
protopack *retranslate_udp(protopack *pk);

ssize_t protopack_send(
    const protopack *pack,
    char raw_data[2048]
);

protopack *protopack_recv(
    const char raw_data[2048],
    size_t     raw_size
);

#endif
#define PROTOCOL_PACKPROTO_H