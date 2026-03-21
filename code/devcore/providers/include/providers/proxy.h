#include <lownet/udp_sock.h>
#include <base/prot_queue.h>
#include <packproto/proto.h>
#include <multithr/events.h>
#include <multithr/time.h>
#include <stdatomic.h>
#include <pthread.h>

#ifndef PROVIDERS_PROXY_H
#define PROVIDERS_PROXY_H

typedef struct {
    bool       drop_pkt;
    protopack *proxyfied_pkt;
} proxyfied;

typedef int (*proxy_function)(proxyfied *ans, protopack *pkt, nnet_fd *nfd, void *ctx);

typedef struct {
    proxy_function prx;
    void *ctx;
} proxy_if;

int proxy_if_create (proxy_if *iface);
int proxy_if_handler(proxy_if *iface, proxy_function func, void *ctx);
int proxy_perform   (proxyfied *out, proxy_if *iface, protopack *pkt, nnet_fd *nfd);

#endif