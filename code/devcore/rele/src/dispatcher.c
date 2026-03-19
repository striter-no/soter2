#include <rele/dispatcher.h>

int rele_dispatcher_new(
    rele_dispatcher *disp, 
    pvd_sender      *sender, 
    uint32_t         relay_uid
){
    if (!disp || !sender) return -1;

    disp->sender    = sender;
    disp->relay_uid = relay_uid;

    return 0;
}

int rele_forward(
    rele_dispatcher *disp, 
    protopack       *pkt, 
    uint32_t         relay_to,
    const nnet_fd   *to
){
    if (!disp || !pkt) return -1;

    protopack *relayed = proto_msg_relay(pkt, relay_to, disp->relay_uid);
    pvd_sender_send(disp->sender, relayed, to);
    free(relayed);

    return 0;
}

int rele_unpack(
    rele_dispatcher *disp, 
    protopack       *inc_pkt, 
    protopack       **unpacked
){
    if (!disp || !inc_pkt || !unpacked) return -1;
    if (inc_pkt->packtype != PACK_RELAYED) return 1;

    if (inc_pkt->d_size < sizeof(protopack)) return -1;

    *unpacked = malloc(inc_pkt->d_size);
    memcpy(*unpacked, inc_pkt->data, inc_pkt->d_size);

    return 0;
}