#include <rele/dispatcher.h>

int rele_dispatcher_new(
    rele_dispatcher *disp, 
    peers_db        *pdb,
    pvd_sender      *sender, 
    uint32_t         relay_uid
){
    if (!disp || !sender) return -1;

    disp->sender    = sender;
    disp->pdb       = pdb;
    disp->relay_uid = relay_uid;

    return 0;
}

int rele_forward(
    rele_dispatcher *disp, 
    protopack       *pkt, 
    uint32_t         relay_to,
    const nnet_fd   *to,
    bool             low_send
){
    if (!disp || !pkt) return -1;

    protopack *relayed = proto_msg_relay(pkt, relay_to, disp->relay_uid);
    if (!low_send) 
        pvd_sender_send(disp->sender, relayed, to);
    else
        _pvd_sender_low_send(disp->sender, relayed, to, false);
    free(relayed);

    return 0;
}

int rele_unpack(
    rele_dispatcher *disp, 
    protopack       *inc_pkt, 
    protopack       **unpacked
){
    if (!disp || !inc_pkt || !unpacked) return -1;

    if (inc_pkt->d_size < sizeof(protopack)) return -1;

    // memcpy(*unpacked, inc_pkt->data, inc_pkt->d_size);
    printf("[rele_unpack] unpacking %u/%u\n", inc_pkt->d_size, ntohl(inc_pkt->d_size));
    *unpacked = protopack_recv((char*)inc_pkt->data, inc_pkt->d_size);
    if (!(*unpacked)) return -1;

    return 0;
}

int rele_broadcast(
    rele_dispatcher *disp,
    protopack       *pkt, 
    uint32_t         relay_to
){
    if (!disp || !pkt) return -1;
    peer_info *snapshot = NULL;
    size_t snapshot_sz = peers_db_snapshot(disp->pdb, &snapshot);

    // printf("[rele][brd] starting to broadcast\n");
    for (size_t i = 0; i < snapshot_sz; i++){
        peer_info info = snapshot[i];
        if (info.UID == disp->relay_uid) continue;
        
        if (info.state    != PEER_ST_ACTIVE   ||
            info.relay_st != PEER_RE_STRAIGHT
        ) continue;

        // printf("[rele][brd] forwarding to %u through %u\n", relay_to, info.UID);
        rele_forward(disp, pkt, relay_to, &info.nfd, true);
    }

    free(snapshot);

    return 0;
}

int rele_proxy_f(
    proxyfied *ans, 
    protopack *pkt, 
    nnet_fd *nfd,
    void *ctx
){
    (void)nfd;
    // printf("[rele] proxy triggered\n");
    ans->drop_pkt = false;

    // if (pkt->is_relayed){
    //     rele_broadcast(ctx, pkt, pkt->h_to);
    //     ans->drop_pkt = true;
    //     return 0;
    // }
    
    // printf("[rele] proxy passing\n");
    ans->proxyfied_pkt = udp_copy_pack(pkt);
    return 0;
}
