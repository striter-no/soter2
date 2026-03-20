#include "stun.h"

#ifndef NATPUNCH_NAT_H

typedef enum {
    NAT_DYNAMIC,
    NAT_STATIC,
    NAT_SYMMETRIC,
    NAT_ERROR
} nat_type;

int nat_get_type(
    ln_usocket *client,
    naddr_t *first_stun,
    naddr_t *second_stun,
    unsigned short port,

    nat_type *type
);

const char *strnattype(nat_type type);

#endif 
#define NATPUNCH_NAT_H