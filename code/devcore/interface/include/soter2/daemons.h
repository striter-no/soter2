#ifndef INTERFACE_DEAMONS_H
#define INTERFACE_DEAMONS_H

#include "_modules.h"
#include "systems.h"

typedef struct {
    // listener
    daemon io_daemon;

    // stating
    daemon st_daemon;
} s2_daemons;

int s2_daemons_create(s2_daemons *d, s2_systems *ctx);
int s2_daemons_stop  (s2_daemons *d);

#endif
