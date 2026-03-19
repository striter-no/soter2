#include <netinet/in.h>
#include <packproto/proto.h>
#include <stdio.h>
#include <string.h>

const char *PROTOPACK_TYPES_CHAR[] = {
    "PACK_DATA",
    "PACK_ACK",
    "PACK_PING",
    "PACK_PONG",
    "PACK_PUNCH",
    "PACK_GOSSIP",
    "PACK_STATE",
    "PACK_RACK"
};

uint32_t crc32(const void *data, size_t n_bytes) {
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
        case PACK_DATA: return true;
        case PACK_FIN:  return true;
        case PACK_RACK: return true;
        
        default: return false;
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
    pack->seq    = sequence_n;
    pack->d_size = payload_size;
    pack->h_from = hash_from;
    pack->h_to   = hash_to;
    pack->packtype = (uint8_t)(type);
    
    if (payload)
        memcpy(pack->data, payload, payload_size);

    // pack->chsum  = htonl(crc32(pack, total_size));
    return pack;
}

protopack *udp_copy_pack(protopack *pk){ // bool apply_ntoh
    protopack *out = malloc(sizeof(protopack) + pk->d_size); // apply_ntoh ? ntohl(pk->d_size): pk->d_size
    out->chsum    = pk->chsum;  /*apply_ntoh ? ntohl(pk->chsum): pk->chsum;*/
    out->seq      = pk->seq;    /*apply_ntoh ? ntohl(pk->seq): pk->seq;*/
    out->d_size   = pk->d_size; /*apply_ntoh ? ntohl(pk->d_size): pk->d_size;*/
    out->h_from   = pk->h_from; /*apply_ntoh ? ntohl(pk->h_from): pk->h_from;*/
    out->h_to     = pk->h_to;   /*apply_ntoh ? ntohl(pk->h_to): pk->h_to;*/
    out->packtype = pk->packtype;
    if (out->d_size != 0) memcpy(out->data, pk->data, out->d_size);

    return out;
}

protopack *retranslate_udp(protopack *pk){
    protopack *out = udp_copy_pack(pk);
    out->chsum    = 0;
    out->seq      = htonl(pk->seq);
    out->d_size   = htonl(pk->d_size);
    out->h_from   = htonl(pk->h_from);
    out->h_to     = htonl(pk->h_to);
    out->packtype = pk->packtype;

    out->chsum  = htonl(crc32(out, pk->d_size + sizeof(protopack)));

    return out;
}

void proto_print(const protopack *pack, int dir){
    switch (pack->packtype){
        case PACK_RACK: {
            printf(
                "[prot][ACK] %u %s %u (%u bytes, %u seq)\n",
                pack->h_to,
                dir == 0 ? "->": "<-",
                pack->h_from,
                pack->d_size,
                pack->seq
            );
        } break;

        case PACK_DATA: {
            printf(
                "[prot][DAT] %u %s %u (%u bytes, %u seq): \"%.*s\"\n",
                pack->h_to,
                dir == 0 ? "->": "<-",
                pack->h_from,
                pack->d_size,
                pack->seq,
                pack->d_size,
                pack->data
            );
        } break;

        case PACK_FIN: {
            printf(
                "[prot][FIN] %u %s %u (%u bytes, %u seq)\n",
                pack->h_to,
                dir == 0 ? "->": "<-",
                pack->h_from,
                pack->d_size,
                pack->seq
            );
        } break;

        default: {
            // printf(
            //     "[protop] pkt: %s (%u bytes, %u seq)\n", 
            //     PROTOPACK_TYPES_CHAR[pack->packtype],
            //     pack->d_size,
            //     pack->seq
            // );
        }
    }
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
    
    if (raw_size >= 2048) {fprintf(stderr, "[proto][recv]: raw_size >= 2048 (%zu)\n", raw_size); return NULL;}
    if (raw_data == NULL) {fprintf(stderr, "[proto][recv]: raw_data is NULL\n"); return NULL;}
    if (raw_size < sizeof(protopack)) {fprintf(stderr, "[proto][recv]: raw_size < sizeof(protopack) (%zu)\n", raw_size); return NULL;}
    
    protopack *recv_pkt = (protopack *)raw_data;

    uint32_t received_chsum = ntohl(recv_pkt->chsum);
    recv_pkt->chsum = 0;
    uint32_t calculated_chsum = crc32(raw_data, raw_size);
    
    if (received_chsum != calculated_chsum) {
        fprintf(stderr, "[proto][recv]: chsum missmatch (%u != %u, %u)\n", received_chsum, calculated_chsum, ntohl(received_chsum));
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