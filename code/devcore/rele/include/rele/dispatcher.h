#include <packproto/protomsgs.h>
#include <providers/sender.h>
#include <peers/peerdb.h>
#include <stdint.h>

#ifndef RELE_DISPATCHER_H
#define RELE_DISPATCHER_H

typedef struct {
    pvd_sender *sender;
    peers_db   *pdb;
    uint32_t    relay_uid;
} rele_dispatcher;

int rele_dispatcher_new(
    rele_dispatcher *disp, 
    peers_db        *pdb,
    pvd_sender      *sender, 
    uint32_t         relay_uid
);

int rele_forward(
    rele_dispatcher *disp, 
    protopack       *pkt, 
    uint32_t         relay_to,
    const nnet_fd   *to,
    bool             low_send
);

int rele_unpack(
    rele_dispatcher *disp, 
    protopack       *inc_pkt, 
    protopack       **unpacked
);

// int rele_broadcast(
//     rele_dispatcher *disp,
//     peers_db        *pdb,
//     protopack       *pkt
// );

int rele_proxy_f(proxyfied *ans, protopack *pkt, nnet_fd *nfd, void *ctx);

#endif