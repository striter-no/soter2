#include <pthread.h>
#include <soter2/interface.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>

int soter2_intr_init(
    soter2_interface *intr
){
    intr->self_sign = sign_gen();

    uint32_t UID = crypto_pubkey_to_uid(intr->self_sign.id_pub);
    
    if (0 > ln_usock_new(&intr->sock)) return -1;

    if (0 > pvd_listener_new(&intr->listener, &intr->sock)) return -1;
    if (0 > pvd_sender_new  (&intr->sender,   &intr->sock)) return -1;
    if (0 > watcher_init(&intr->wtch, &intr->sender, &intr->listener)) return -1;
    if (0 > rudp_dispatcher_new(&intr->rudp_disp, &intr->sender, UID)) return -1;
    if (0 > peers_db_init(&intr->pdb)) return -1;
    if (0 > gossip_system_init(&intr->gsyst, UID)) return -1;

    if (0 > state_sys_init(&intr->ssyst)) return -1;

    intr->ctx = (app_context){
        .g_syst   = &intr->gsyst,
        .p_db     = &intr->pdb,
        .watcher  = &intr->wtch,
        .rudp     = &intr->rudp_disp,
        .listener = &intr->listener,
        .sender   = &intr->sender,
        .ssytem   = &intr->ssyst
    };

    watcher_handler_reg(&intr->wtch, PACK_ACK,    (watcher_handler){soter2_hnd_ACK,    &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_PUNCH,  (watcher_handler){soter2_hnd_PUNCH,  &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_PING,   (watcher_handler){soter2_hnd_PING,   &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_PONG,   (watcher_handler){soter2_hnd_PONG,   &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_GOSSIP, (watcher_handler){soter2_hnd_GOSSIP, &intr->ctx});
    watcher_handler_reg(&intr->wtch, PACK_STATE,  (watcher_handler){soter2_hnd_STATE,  &intr->ctx});

    intr->state_serv   = (naddr_t){0};
    intr->state_req_dt = 0;
    intr->state_last_called = 0;

    intr->packets_timestamp = 0;
    intr->packet_rate = 0.f;
    intr->packets_translated = 0;

    atomic_store(&intr->is_running, false);
    pthread_mutex_init(&intr->rate_mtx, NULL);
    return 0;
}

int soter2_intr_save_sign(soter2_interface *intr, const char *path){
    if (0 > sign_store(&intr->self_sign, path)) 
        return -1;
    return 0;
}

int soter2_intr_load_sign(soter2_interface *intr, const char *path){
    if (0 > sign_load(&intr->self_sign, path)) 
        return -1;
    
    intr->rudp_disp.self_uid = crypto_pubkey_to_uid(intr->self_sign.id_pub);
    intr->gsyst.self_uid = intr->rudp_disp.self_uid;
    return 0;
}

// load if exists, save otherwise
int soter2_intr_upd_sign(soter2_interface *intr, const char *path){
    struct stat s;
    if (0 == stat(path, &s))
        return soter2_intr_load_sign(intr, path);
    else
        return soter2_intr_save_sign(intr, path);
}

void soter2_intr_reset_handlers(soter2_interface *intr, soter2_ivtable vt){
    if(vt.ACK.foo) watcher_handler_reg(&intr->wtch, PACK_ACK, (watcher_handler){vt.ACK.foo, vt.ACK.ctx});
    if(vt.PUNCH.foo) watcher_handler_reg(&intr->wtch, PACK_PUNCH, (watcher_handler){vt.PUNCH.foo, vt.PUNCH.ctx});
    if(vt.PING.foo) watcher_handler_reg(&intr->wtch, PACK_PING, (watcher_handler){vt.PING.foo, vt.PING.ctx});
    if(vt.PONG.foo) watcher_handler_reg(&intr->wtch, PACK_PONG, (watcher_handler){vt.PONG.foo, vt.PONG.ctx});
    if(vt.GOSSIP.foo) watcher_handler_reg(&intr->wtch, PACK_GOSSIP, (watcher_handler){vt.GOSSIP.foo, vt.GOSSIP.ctx});
    if(vt.STATE.foo) watcher_handler_reg(&intr->wtch, PACK_STATE, (watcher_handler){vt.STATE.foo, vt.STATE.ctx});
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
    state_sys_end(&intr->ssyst);

    ln_usock_close(&intr->sock);
    pthread_mutex_destroy(&intr->rate_mtx);
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

int soter2_intr_stateconn(soter2_interface *intr, naddr_t addr, int state_req_dt){
    if (!intr) return -1;
    
    intr->state_serv = addr;
    intr->state_req_dt = state_req_dt;

    return 0;
}

int soter2_intr_statestop(soter2_interface *intr){
    if (!intr) return -1;

    intr->state_req_dt = 0;
    return 0;
}

int soter2_intr_wait_state(soter2_interface *intr, int timeout, state_request *out_req){
    if (!intr || !out_req) return -1;

    int r = state_sys_wait(&intr->ssyst, out_req, timeout);
    return r;
}

int soter2_e2ee_wrap(soter2_interface *intr, rudp_channel *chan, e2ee_channel *wrapped, unsigned char other_pubkey[CRYPTO_PUBKEY_BYTES]){
    if (!intr || !chan || !wrapped) return -1;
    return e2ee_chan_init(wrapped, &intr->rudp_disp, chan, intr->self_sign, other_pubkey);
}

void soter2_iconnect(soter2_interface *intr, naddr_t address, uint32_t UID){
    peers_db_add(&intr->pdb, (peer_info){
        .last_seen = mt_time_get_seconds(),
        .UID = UID,
        .state = PEER_ST_INITED,
        .nfd = ln_netfdq(&address),
        .ctx = NULL
    });

    protopack *punch_msg = proto_msg_quick(intr->rudp_disp.self_uid, UID, 0, PACK_PUNCH);
    
    nnet_fd nfd = ln_netfdq(&address);
    pvd_sender_send(&intr->sender, punch_msg, &nfd);
    free(punch_msg);
}

nat_type soter2_intr_STUN(soter2_interface *intr, naddr_t stun1, naddr_t stun2){
    intr->NAT = nat_get_type(&intr->sock, &stun1, &stun2, 1024 + (rand() % 63000));

    return intr->NAT;
}

int soter2_istatewait(soter2_interface *intr, uint32_t client_uid, peer_state state, peer_info *info){
    return peers_db_wait(&intr->pdb, client_uid, state, info);
}

// -- user-connections

int soter2_iwait_chan(soter2_interface *intr, uint32_t client_uid){
    return rudp_dispatcher_chan_wait(&intr->rudp_disp, client_uid);
}

rudp_channel *soter2_inew_chan(soter2_interface *intr, nnet_fd *nfd, uint32_t client_uid){
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

int soter2_isend_r(soter2_interface *intr, rudp_channel *chan, protopack *p){
    return rudp_direct_send(&intr->rudp_disp, chan, p);
}

int soter2_isend(soter2_interface *intr, rudp_channel *chan, void *data, size_t dsize){
    protopack *p = udp_make_pack(chan->next_seq, intr->rudp_disp.self_uid, chan->client_uid, PACK_DATA, data, dsize);
    int r = rudp_direct_send(&intr->rudp_disp, chan, p);
    free(p);

    return r;
}

float soter2_get_DPS(soter2_interface *intr){
    if (!intr) return 0;

    pthread_mutex_lock(&intr->rate_mtx);
    float a = intr->packet_rate;
    pthread_mutex_unlock(&intr->rate_mtx);
    
    return a;
}

// -- workers

static int ping_iter(peer_info *info, void *ctx);
static int gossip_iter(peer_info *info, void *ctx);
static int state_iter(soter2_interface *intr);

static void *iter_daemon(void *_args){
    soter2_interface *intr = _args;
     
    while (atomic_load(&intr->is_running)){
        int r = mt_evsock_wait(&intr->listener.newpack_es, 10);
        mt_evsock_drain(&intr->listener.newpack_es);
        
        int64_t millis = mt_time_get_millis();
        if (millis - intr->packets_timestamp >= 1000) {
            int64_t delta_ms = millis - intr->packets_timestamp;
            
            pthread_mutex_lock(&intr->rate_mtx);
            if (delta_ms > 0) {
                float delta_sec = delta_ms / 1000.0f;
                
                intr->packet_rate = (float)intr->packets_translated / delta_sec;
            } else {
                intr->packet_rate = 0.0f;
            }
            pthread_mutex_unlock(&intr->rate_mtx);
            
            intr->packets_timestamp = millis;
            intr->packets_translated = 0;
        }

        if (intr->state_req_dt != 0 && (mt_time_get_seconds() - intr->state_last_called >= intr->state_req_dt)){
            state_iter(intr);
            intr->state_last_called = mt_time_get_seconds();
        }

        // if (intr->packet_rate <= SOTER_LOW_PACKET_RATE)
        // peers_db_foreach(&intr->pdb, ping_iter, &intr->ctx);
        // if (mt_time_get_seconds() - intr->gsyst.last_gossiped >= SOTER_GOSSIP_DT){
        //     gossip_cleanup(&intr->gsyst);

        //     peers_db_foreach(&intr->pdb, gossip_iter, &intr->ctx);
        //     intr->gsyst.last_gossiped = mt_time_get_seconds();
        // }

        if (r == 0) continue;
        if (r < 0) {
            perror("poll()");
            continue;
        }

        listener_packet pkt = pvd_next_packet(&intr->listener);
        if (pkt.pack == 0 || pkt.from_who.addr_len == 0) continue;

        if (udp_is_RUDP_req(pkt.pack->packtype)){
            rudp_dispatcher_pass(&intr->rudp_disp, pkt.pack, &pkt.from_who);
            intr->packets_translated++;
        } else {
            watcher_pass(&intr->wtch, pkt.pack, &pkt.from_who);
        }
    }

    return NULL;
}

static int state_iter(soter2_interface *intr){
    // requesting to server

    printf("state_iter called (uid: %u)\n", intr->rudp_disp.self_uid);
    state_request r = state_rcreate(
        ln_to_uint32(&intr->sock.addr), 
        intr->sock.addr.ip.v4.port,
        intr->rudp_disp.self_uid, REQUEST_CONNECTION, 
        intr->self_sign
    );

    protopack *pack = udp_make_pack(0, intr->rudp_disp.self_uid, 0, PACK_STATE, &r, sizeof(r));
    
    nnet_fd n = ln_netfdq(&intr->state_serv);
    pvd_sender_send(&intr->sender, pack, &n);
    free(pack);

    return 0;
}

static int ping_iter(peer_info *info, void *ctx_){ app_context *ctx = ctx_;
    uint32_t stamp = mt_time_get_seconds();
    // printf("for %u last_seen DT: %u\n", info->UID, stamp - info->last_seen);
    if (stamp - info->last_seen >= SOTER_DEAD_DT){
        printf("Dead peer detected: %u\n", info->UID);
        return 1;
    }

    if (stamp - info->last_seen >= SOTER_REPING_DT){
        protopack *ping = proto_msg_quick(ctx->rudp->self_uid, info->UID, 0, PACK_PING);
        pvd_sender_send(ctx->sender, ping, &info->nfd);
        info->last_seen = stamp;
        free(ping);
    }

    return 0;
}

static int gossip_iter(peer_info *info, void *ctx_){ app_context *ctx = ctx_;
    // we dont check dt because of 1 check in iter() daemon
    // do not spam to all peers
    if (rand() % 2 == 0) return 0;

    naddr_t addr = ln_nfd2addr(&info->nfd);
    gossip_entry *entry = gossip_create_entry(info->UID, ln_to_uint32(&addr), addr.ip.v4.port, 0, NULL);
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
    pvd_sender_send(ctx->sender, gossip, &info->nfd);
    free(gossip);
    return 0;
}
