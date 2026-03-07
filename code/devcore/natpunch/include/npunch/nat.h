#include "stun.h"

#ifndef NATPUNCH_NAT_H

typedef enum {
    NAT_DYNAMIC,
    NAT_STATIC,
    NAT_SYMMETRIC
} nat_type;

nat_type nat_get_type(
    ln_usocket *client,
    naddr_t first_stun,
    naddr_t second_stun,
    unsigned short port
);

const char *strnattype(nat_type type);

#endif 
#define NATPUNCH_NAT_H