#include <providers/sender.h>
#include <providers/listener.h>

#ifndef WATCHER_SYSTEM

typedef int (*watcher_handler_foo)(protopack *pck, const nnet_fd *nfd, pvd_sender *s, void *ctx);

typedef struct {
    watcher_handler_foo  foo;
    void                *ctx;
} watcher_handler;

typedef struct {
    watcher_handler handlers[PACKET_TYPE_MAX];
    prot_queue      passed_packets;
    
    pvd_sender   *sender;

    mt_eventsock  newpack;
    pthread_t     daemon;
    atomic_bool   is_running;
} watcher;

int  watcher_init (watcher *w, pvd_sender *sender, pvd_listener *listener);
void watcher_end  (watcher *w);
int  watcher_start(watcher *w);

int watcher_set_context(watcher *w, protopack_type type, void *ctx);
int watcher_handler_reg(watcher *w, uint8_t type, watcher_handler handler);
int watcher_pass       (watcher *w, protopack *pack, const nnet_fd *from_who);

#endif
#define WATCHER_SYSTEM