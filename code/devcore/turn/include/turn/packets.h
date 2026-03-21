#include <stddef.h>
#include <lownet/core.h>
#include <stdint.h>

#ifndef TURN_PACKETS_H
#define TURN_PACKETS_H

typedef enum {
    TURN_OPEN_HOLE,
    TURN_CLOSE_HOLE,
    TURN_SYSTEM_MSG,
    TURN_DATA_MSG
} turn_rtype;

typedef struct {
    turn_rtype type;
    naddr_t    to_whom;

    uint32_t   d_size;
    unsigned char data[];
} turn_request;

turn_request *turn_req_make(
    turn_rtype  type,
    naddr_t     to_whom,
    const void *data,
    uint32_t    d_size
);

turn_request *turn_req_recv(
    const void *data,
    uint32_t    d_size
);

#endif