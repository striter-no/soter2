#include <packproto/proto.h>
#include <string.h>

static uint32_t crc32(const void *data, size_t n_bytes) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *byte_ptr = (const uint8_t *)data;

    for (size_t i = 0; i < n_bytes; i++) {
        crc ^= byte_ptr[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

bool udp_is_RUDP_req(protopack_type type){
    switch (type) {
        case PACK_DATA:      return true;
        case PACK_HELLO:     return true;
        case PACK_REJECT:    return true;
        case PACK_ACCEPT:    return true;
        case PACK_RACK:      return true;
        default:                 return false;
    }
}

protopack *udp_make_pack(
    uint32_t  sequence_n,
    uint32_t  hash_from,
    uint32_t  hash_to,
    protopack_type type,

    void     *payload,
    size_t    payload_size
){
    size_t total_size = sizeof(protopack) + payload_size;
    protopack *pack = malloc(total_size);
    if (!pack) return NULL;
    memset(pack, 0, total_size);

    pack->chsum  = 0;
    pack->seq    = htonl(sequence_n);
    pack->d_size = htonl(payload_size);
    pack->h_from = htonl(hash_from);
    pack->h_to   = htonl(hash_to);
    pack->packtype = (uint8_t)(type);
    
    if (payload)
        memcpy(pack->data, payload, payload_size);

    pack->chsum  = htonl(crc32(pack, total_size));

    return pack;
}

protopack *udp_copy_pack(protopack *pk, bool apply_ntoh){
    protopack *out = malloc(sizeof(protopack) + (apply_ntoh ? ntohl(pk->d_size): pk->d_size));
    out->chsum    = apply_ntoh ? ntohl(pk->chsum): pk->chsum;
    out->seq      = apply_ntoh ? ntohl(pk->seq): pk->seq;
    out->d_size   = apply_ntoh ? ntohl(pk->d_size): pk->d_size;
    out->h_from   = apply_ntoh ? ntohl(pk->h_from): pk->h_from;
    out->h_to     = apply_ntoh ? ntohl(pk->h_to): pk->h_to;
    out->packtype = pk->packtype;
    if (out->d_size != 0) memcpy(out->data, pk->data, out->d_size);

    return out;
}

protopack *retranslate_udp(protopack *pk, int to_net){
    protopack *out = udp_copy_pack(pk, to_net == 0);
    if (to_net){
        out->chsum    = htonl(pk->chsum);
        out->seq      = htonl(pk->seq);
        out->d_size   = htonl(pk->d_size);
        out->h_from   = htonl(pk->h_from);
        out->h_to     = htonl(pk->h_to);
        out->packtype = pk->packtype;
    }
    return out;
}

ssize_t protopack_send(
    const  protopack *pack,
    char   raw_data[2048]
){
    if (!pack) return -1;

    size_t total_size = sizeof(protopack) + ntohl(pack->d_size);
    memcpy(raw_data, pack, total_size);

    return total_size;
}

protopack *protopack_recv(
    const char raw_data[2048],
    size_t     raw_size
){
    if (raw_size >= 2048) return NULL;
    if (raw_data == NULL) return NULL;
    if (raw_size < sizeof(protopack)) return NULL;

    protopack *recv_pkt = (protopack *)raw_data;

    uint32_t received_chsum = ntohl(recv_pkt->chsum);
    recv_pkt->chsum = 0;
    uint32_t calculated_chsum = crc32(raw_data, raw_size);
    
    if (received_chsum != calculated_chsum) {
        return NULL;
    }

    protopack *final_pkt = malloc(raw_size);
    if (final_pkt) {
        memcpy(final_pkt, raw_data, raw_size);
        final_pkt->chsum = htonl(received_chsum); 
    }

    final_pkt->seq    = ntohl(final_pkt->seq);
    final_pkt->d_size = ntohl(final_pkt->d_size);
    final_pkt->h_from = ntohl(final_pkt->h_from);
    final_pkt->h_to   = ntohl(final_pkt->h_to);
    return final_pkt;
}