#ifndef INTERFACE_DEAMONS_H
#define INTERFACE_DEAMONS_H

#include "_modules.h"

typedef struct {
    void *args;
} daemon_args;

typedef struct {
    pthread_t   daemon;
    atomic_bool is_running;
} daemon;

#endif
