#include <stdio.h>
#include <string.h>
#include <turn/packets.h>
#include <stdlib.h>

turn_request *turn_req_make(
    turn_rtype  type,
    naddr_t     to_whom,
    const void *data,
    uint32_t    d_size
){
    turn_request *output = malloc(sizeof(*output) + d_size);

    output->type    = type;
    output->to_whom = ln_hton(&to_whom);
    output->d_size  = ntohl(d_size);

    memcpy(output->data, data, d_size);

    return output;
}

turn_request *turn_req_recv(
    const void *data,
    uint32_t    d_size
){
    if (!data) return NULL;
    if (d_size < sizeof(turn_request)) return NULL;

    size_t offset = 0;
    turn_request *out = malloc(d_size);
    memcpy(&out->to_whom, (const char*)data + offset, sizeof(out->to_whom)); offset += sizeof(out->to_whom);
    memcpy(&out->type, (const char*)data + offset, sizeof(out->type)); offset += sizeof(out->type);
    memcpy(&out->d_size, (const char*)data + offset, sizeof(out->d_size)); offset += sizeof(out->d_size);

    out->d_size = ntohl(out->d_size);
    if (out->d_size != d_size - sizeof(turn_request)){
        fprintf(stderr, "[turn] mallformed packet dropped\n");
        free(out);
        return NULL;
    }

    memcpy(out->data, (const char*)data + offset, out->d_size);

    return out;
}