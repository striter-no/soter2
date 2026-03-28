#ifndef MULTITHR_DAEMONS_H
#define MULTITHR_DAEMONS_H

#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct {
    pthread_t   daemon;
    atomic_bool is_running;

    bool iter;
} daemon;

int daemon_run (daemon *d, bool as_iterator, bool (*func)(void *args), void *args);
int daemon_stop(daemon *d);

#endif
