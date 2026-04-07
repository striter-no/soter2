#include <assert.h>
#include <onion/packets.h>

int onion_pack_construct(
    onion_packet  *out,
    unsigned char  data[MAX_ONION_CELL_SIZE],
    unsigned short d_size,

    uint32_t  circut_uid,
    onion_cmd cmd
){
    if (!out || !data) return -1;
    if (d_size > MAX_ONION_CELL_SIZE) return -1;

    out->cmd = cmd;
    out->d_size = d_size;
    out->circut_uid = circut_uid;
    memcpy(out->cell, data, d_size);

    assert(MAX_ONION_PACK_SIZE >= sizeof(onion_packet));
    return 0;
}

int onion_pack_serial(
    const onion_packet *in,
    unsigned char outgoing[sizeof(onion_packet)]
){
    if (!in || !outgoing) return -1;

    memcpy(outgoing, in, sizeof(onion_packet));

    return 0;
}

int onion_pack_deserial(
    onion_packet *out,
    const unsigned char incoming[sizeof(onion_packet)]
){
    if (!out || !incoming) return -1;

    memcpy(out, incoming, sizeof(onion_packet));

    return 0;
}
