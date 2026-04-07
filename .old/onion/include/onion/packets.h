#ifndef ONION_PACKETS_H
#define ONION_PACKETS_H

#include <e2ee/connection.h>
#include <rudp/packets.h>

#define MAX_ONION_PACK_SIZE (UDP_MTU - PROTOCOL_OVERHEAD - E2EE_OVERHEAD)
#define MAX_ONION_CELL_SIZE 1200

typedef enum {
    ONION_CMD_CREATE,
    ONION_CMD_EXTEND,
    ONION_CMD_CLOSE,
    ONION_CMD_DATA,
    ONION_CMD_CREATED,
    ONION_CMD_EXTENDED
} onion_cmd;

typedef struct {
    unsigned char  cell[MAX_ONION_CELL_SIZE];
    unsigned short d_size;

    uint32_t  circut_uid;
    onion_cmd cmd;
} onion_packet;

int onion_pack_construct(
    onion_packet  *out,
    unsigned char  data[MAX_ONION_CELL_SIZE],
    unsigned short d_size,

    uint32_t  circut_uid,
    onion_cmd cmd
);

int onion_pack_serial(
    const onion_packet *in,
    unsigned char outgoing[sizeof(onion_packet)]
);

int onion_pack_deserial(
    onion_packet *out,
    const unsigned char incoming[sizeof(onion_packet)]
);

#endif // ONION_PACKETS_H
