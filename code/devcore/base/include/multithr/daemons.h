#ifndef MULTITHR_DAEMONS_H
#define MULTITHR_DAEMONS_H

#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct {
    pthread_t   daemon;
    atomic_bool is_running;

    bool iter;
} mdaemon;

int daemon_run (mdaemon *d, bool as_iterator, bool (*func)(void *args), void *args);
int daemon_stop(mdaemon *d);

#endif
