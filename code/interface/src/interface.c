#include <soter2/interface.h>
#include <stdint.h>

int soter2_intr_init(
    soter2_interface *intr,
    uint32_t UID
){
    if (0 > ln_usock_new(&intr->sock)) return -1;

    if (0 > pvd_listener_new(&intr->listener, &intr->sock)) return -1;
    if (0 > pvd_sender_new  (&intr->sender,   &intr->sock)) return -1;
    if (0 > watcher_init(&intr->wtch, &intr->sender, &intr->listener)) return -1;
    if (0 > rudp_dispatcher_new(&intr->rudp_disp, &intr->sender, UID)) return -1;
    if (0 > peers_db_init(&intr->pdb)) return -1;
    if (0 > gossip_system_init(&intr->gsyst, UID)) return -1;

    intr->ctx = (app_context){
        .g_syst   = &intr->gsyst,
        .p_db     = &intr->pdb,
        .watcher  = &intr->wtch,
        .rudp     = &intr->rudp_disp,
        .listener = &intr->listener,
        .sender   = &intr->sender
    };

    watcher_handler_reg(&intr->wtch, PACK_ACK,    (watcher_handler){soter2_hnd_ACK,    &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_PUNCH,  (watcher_handler){soter2_hnd_PUNCH,  &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_PING,   (watcher_handler){soter2_hnd_PING,   &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_PONG,   (watcher_handler){soter2_hnd_PONG,   &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_GOSSIP, (watcher_handler){soter2_hnd_GOSSIP, &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_STATE,  (watcher_handler){soter2_hnd_STATE,  &intr->ctx});

    atomic_store(&intr->is_running, false);
    return 0;
}

void soter2_intr_end(soter2_interface *intr){
    if (!intr) return;

    atomic_store(&intr->is_running, false);
    pthread_join(intr->iter_daemon, NULL);
    
    watcher_end(&intr->wtch);
    rudp_dispatcher_end(&intr->rudp_disp);
    gossip_system_end(&intr->gsyst);
    peers_db_end(&intr->pdb);
    pvd_sender_end(&intr->sender);
    pvd_listener_end(&intr->listener);

    ln_usock_close(&intr->sock);
}

static void *iter_daemon(void *_args);
int soter2_intr_run(soter2_interface *intr){
    if (!intr) return -1;

    if (0 > pvd_listener_start(&intr->listener)) return -1;
    if (0 > pvd_sender_start(&intr->sender)) return -1;
    if (0 > watcher_start(&intr->wtch)) return -1;
    if (0 > rudp_dispatcher_run(&intr->rudp_disp)) return -1;

    atomic_store(&intr->is_running, true);
    pthread_create(&intr->iter_daemon, NULL, iter_daemon, intr);
    return 0;
}

void soter2_iconnect(soter2_interface *intr, naddr_t address, uint32_t UID){
    peers_db_add(&intr->pdb, (peer_info){
        .last_seen = time(NULL),
        .UID = UID,
        .state = PEER_ST_INITED,
        .nfd = ln_netfdq(address),
        .ctx = NULL
    });

    protopack *punch_msg = proto_msg_quick(intr->rudp_disp.self_uid, UID, 0, PACK_PUNCH);
    pvd_sender_send(&intr->sender, punch_msg, ln_netfdq(address));
    free(punch_msg);
}

nat_type soter2_intr_STUN(soter2_interface *intr, naddr_t stun1, naddr_t stun2){
    intr->NAT = nat_get_type(&intr->sock, stun1, stun2, 1024 + (rand() % 63000));

    return intr->NAT;
}

int soter2_istatewait(soter2_interface *intr, uint32_t client_uid, peer_state state, peer_info *info){
    return peers_db_wait(&intr->pdb, client_uid, state, info);
}

// -- user-connections

int soter2_iwait_chan(soter2_interface *intr, uint32_t client_uid){
    return rudp_dispatcher_chan_wait(&intr->rudp_disp, client_uid);
}

rudp_channel *soter2_inew_chan(soter2_interface *intr, nnet_fd nfd, uint32_t client_uid){
    if (0 > rudp_dispatcher_chan_new(&intr->rudp_disp, nfd, client_uid)) 
        return NULL;

    rudp_channel *channel = NULL;
    rudp_dispatcher_chan_get(&intr->rudp_disp, client_uid, &channel);
    return channel;
}

rudp_channel *soter2_iget_chan(soter2_interface *intr, uint32_t client_uid){
    rudp_channel *channel = NULL;
    rudp_dispatcher_chan_get(&intr->rudp_disp, client_uid, &channel);
    return channel;
}

int soter2_iwait_pack(rudp_channel *chan, int timeout){
    return rudp_channel_wait(chan, timeout);
}

protopack *soter2_irecv (rudp_channel *chan){
    protopack *r = NULL;
    rudp_channel_recv(chan, &r);
    return r;
}

int soter2_isend_r(soter2_interface *intr, nnet_fd nfd, protopack *p){
    return rudp_dispatcher_send(&intr->rudp_disp, p, nfd);
}

int soter2_isend(soter2_interface *intr, rudp_channel *chan, void *data, size_t dsize){
    protopack *p = udp_make_pack(chan->next_seq, intr->rudp_disp.self_uid, chan->client_uid, PACK_DATA, data, dsize);
    int r = rudp_dispatcher_send(&intr->rudp_disp, p, chan->client_nfd);
    free(p);

    return r;
}


// -- workers

static int ping_iter(peer_info *info, void *ctx);
static int gossip_iter(peer_info *info, void *ctx);

static void *iter_daemon(void *_args){
    soter2_interface *intr = _args;
     
    while (atomic_load(&intr->is_running)){
        int r = mt_evsock_wait(&intr->listener.newpack_es, 100);

        peers_db_foreach(&intr->pdb, ping_iter, &intr->ctx);
        if (time(NULL) - intr->gsyst.last_gossiped >= SOTER_GOSSIP_DT){
            gossip_cleanup(&intr->gsyst);
            
            peers_db_foreach(&intr->pdb, gossip_iter, &intr->ctx);
            intr->gsyst.last_gossiped = time(NULL);
        }

        if (r == 0) continue;
        if (r < 0) {
            perror("poll()");
            continue;
        }

        listener_packet pkt = pvd_next_packet(&intr->listener);
        if (pkt.pack == 0 || pkt.from_who.addr_len == 0) continue;

        if (udp_is_RUDP_req(pkt.pack->packtype)){
            rudp_dispatcher_pass(&intr->rudp_disp, pkt.pack, pkt.from_who);
        } else {
            watcher_pass(&intr->wtch, pkt.pack, pkt.from_who);
        }
    }

    return NULL;
}

static int ping_iter(peer_info *info, void *ctx_){ app_context *ctx = ctx_;
    uint32_t stamp = time(NULL);
    // printf("for %u last_seen DT: %u\n", info->UID, stamp - info->last_seen);
    if (stamp - info->last_seen >= SOTER_DEAD_DT){
        printf("Dead peer detected: %u\n", info->UID);
        return 1;
    }

    if (stamp - info->last_seen >= SOTER_REPING_DT){
        protopack *ping = proto_msg_quick(ctx->rudp->self_uid, info->UID, 0, PACK_PING);
        pvd_sender_send(ctx->sender, ping, info->nfd);
        free(ping);
    }

    return 0;
}

static int gossip_iter(peer_info *info, void *ctx_){ app_context *ctx = ctx_;
    // we dont check dt because of 1 check in iter() daemon
    // do not spam to all peers
    if (rand() % 2 == 0) return 0;

    naddr_t addr = ln_nfd2addr(info->nfd);
    gossip_entry *entry = gossip_create_entry(info->UID, ln_to_uint32(addr), addr.ip.v4.port, 0, NULL);
    gossip_new_entry(ctx->g_syst, entry);
    free(entry);

    size_t entries_c = 5;
    gossip_entry **entries = NULL;
    if (1 == gossip_random_entries(ctx->g_syst, &entries, &entries_c)){
        // no entries
        return 0;
    }
    
    size_t   packet_dsize = 0;
    uint8_t *packet_data = NULL;
    gossip_to_data(ctx->g_syst, entries, entries_c, &packet_data, &packet_dsize);

    protopack *gossip = udp_make_pack(
        0, ctx->rudp->self_uid, info->UID, PACK_GOSSIP, packet_data, packet_dsize
    );

    for (size_t i = 0; i < entries_c; i++){
        free(entries[i]);
    }
    free(entries);

    free(packet_data);
    pvd_sender_send(ctx->sender, gossip, info->nfd);
    free(gossip);
    return 0;
}
