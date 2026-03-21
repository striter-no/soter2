#include <providers/proxy.h>

int proxy_if_create(proxy_if *iface){
    if (!iface) return -1;

    iface->prx = NULL;
    iface->ctx = NULL;

    return 0;
}

int proxy_if_handler(proxy_if *iface, proxy_function func, void *ctx){
    if (!iface) return -1;
    iface->prx = func;
    iface->ctx = ctx;

    return 0;
}

int proxy_perform(proxyfied *out, proxy_if *iface, protopack *pkt, nnet_fd *nfd){
    if (!iface || !out || !pkt || !nfd) return -1;

    if (!iface->prx){
        out->drop_pkt = false;
        out->proxyfied_pkt = pkt;

        return 0;
    }

    return iface->prx(out, pkt, nfd, iface->ctx);
}