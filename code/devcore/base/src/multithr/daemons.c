#include <multithr/daemons.h>
#include <stdlib.h>

typedef struct {
    void        *args;
    atomic_bool *is_running;

    bool (*func)(void *args);
    bool run_as_iter;
} daemon_wrapper_args;

static void *_daemon_wrapper(void *_args){
    daemon_wrapper_args *args = _args;

    if (!args->run_as_iter){
        args->func(args->args);
    }

    while (args->run_as_iter && atomic_load(args->is_running)){
        if (!args->func(args->args)) atomic_store(args->is_running, false);
    }

    free(args);
    return NULL;
}

int daemon_run(daemon *d, bool as_iterator, bool (*func)(void *args), void *args){
    if (!d || !func) return -1;

    daemon_wrapper_args *wargs = malloc(sizeof(*wargs));
    wargs->args = args;
    wargs->func = func;
    wargs->run_as_iter = as_iterator;
    wargs->is_running = &d->is_running;
    atomic_store(&d->is_running, true);

    if (0 != pthread_create(&d->daemon, NULL, _daemon_wrapper, wargs)){
        atomic_store(&d->is_running, false);
        free(wargs);
    }

    return 0;
}

int daemon_stop(daemon *d){
    if (!d) return -1;

    atomic_store(&d->is_running, false);
    pthread_join(d->daemon, NULL);

    return 0;
}
