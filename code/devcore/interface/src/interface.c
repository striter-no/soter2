#include "rudp/_modules.h"
#include <soter2/interface.h>
#include <multithr/time.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int soter2_intr_init(
    soter2_interface *intr
){
    if (0 > crypto_init()) return -1;

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
    if (0 > rele_dispatcher_new(&intr->rele_disp, &intr->sender, UID)) return -1;

    if (0 > mt_evsock_new(&intr->_new_active_conn)) return -1;

    if (0 > prot_queue_create(sizeof(state_request), &intr->state_peers)) return -1;
    if (0 > prot_table_create(
        sizeof(int), sizeof(soter2_state_intr), 
        DYN_OWN_BOTH, &intr->state_servs
    )) return -1;

    if (0 > prot_table_create(
        sizeof(uint32_t), sizeof(rudp_connection*), 
        DYN_OWN_BOTH, &intr->_active_conns
    )) return -1;

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

    intr->packets_timestamp = 0;
    intr->packet_rate = 0.f;
    intr->packets_translated = 0;
    intr->state_last_called = 0;

    intr->stating_daemon = 0;
    intr->iter_daemon = 0;

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
    if(vt.ACK.foo)    watcher_handler_reg(&intr->wtch, PACK_ACK,    (watcher_handler){vt.ACK.foo,    vt.ACK.ctx    });
    if(vt.PUNCH.foo)  watcher_handler_reg(&intr->wtch, PACK_PUNCH,  (watcher_handler){vt.PUNCH.foo,  vt.PUNCH.ctx  });
    if(vt.PING.foo)   watcher_handler_reg(&intr->wtch, PACK_PING,   (watcher_handler){vt.PING.foo,   vt.PING.ctx   });
    if(vt.PONG.foo)   watcher_handler_reg(&intr->wtch, PACK_PONG,   (watcher_handler){vt.PONG.foo,   vt.PONG.ctx   });
    if(vt.GOSSIP.foo) watcher_handler_reg(&intr->wtch, PACK_GOSSIP, (watcher_handler){vt.GOSSIP.foo, vt.GOSSIP.ctx });
    if(vt.STATE.foo)  watcher_handler_reg(&intr->wtch, PACK_STATE,  (watcher_handler){vt.STATE.foo,  vt.STATE.ctx  });
}

void soter2_intr_end(soter2_interface *intr){
    if (!intr) return;

    atomic_store(&intr->is_running, false);
    pthread_join(intr->iter_daemon, NULL);
    pthread_join(intr->stating_daemon, NULL);
    
    watcher_end(&intr->wtch);
    rudp_dispatcher_end(&intr->rudp_disp);
    gossip_system_end(&intr->gsyst);
    peers_db_end(&intr->pdb);
    pvd_sender_end(&intr->sender);
    pvd_listener_end(&intr->listener);
    state_sys_end(&intr->ssyst);
    prot_table_end(&intr->state_servs);

    ln_usock_close(&intr->sock);
    pthread_mutex_destroy(&intr->rate_mtx);

    prot_table_lock(&intr->_active_conns);
    for (size_t i = 0; i < intr->_active_conns.table.array.len; i++) {
        dyn_pair *pair = (dyn_pair *)dyn_array_at(&intr->_active_conns.table.array, i);    
        rudp_connection **conn = pair->second;
        if (conn && (*conn)) free(*conn);
    }
    prot_table_unlock(&intr->_active_conns);
    prot_table_end(&intr->_active_conns);

    mt_evsock_close(&intr->_new_active_conn);
}

static void *iter_daemon(void *_args);
static void* _state_worker(void *_args);
int soter2_intr_run(soter2_interface *intr){
    if (!intr) return -1;
    if (0 > pvd_listener_start(&intr->listener)) 
        return -1;
    if (0 > pvd_sender_start(&intr->sender)) 
        return -1;
    if (0 > watcher_start(&intr->wtch)) 
        return -1;
    if (0 > rudp_dispatcher_run(&intr->rudp_disp)) 
        return -1;

    atomic_store(&intr->is_running, true);
    pthread_create(&intr->iter_daemon, NULL, iter_daemon, intr);
    pthread_create(&intr->stating_daemon, NULL, _state_worker, intr);
    return 0;
}

int soter2_intr_stateconn(soter2_interface *intr, naddr_t addr, int state_req_dt){
    if (!intr) return -1;
    soter2_state_intr st;
    st.req_dt = state_req_dt;
    st.addr = addr;

    int UID = abs(rand());
    prot_table_set(&intr->state_servs, &UID, &st);

    return UID;
}

int soter2_intr_statestop(soter2_interface *intr, int state_serv_UID){
    if (!intr) return -1;

    soter2_state_intr *st = prot_table_get(&intr->state_servs, &state_serv_UID);
    if (st == NULL) return -1;

    st->req_dt = 0;
    return 0;
}

int soter2_intr_wait_state(soter2_interface *intr, int timeout, state_request *out_req){
    if (!intr || !out_req) return -1;

    int r = state_sys_wait(&intr->ssyst, out_req, timeout);
    return r;
}

int soter2_e2ee_wrap(soter2_interface *intr, rudp_connection *conn, e2ee_connection *wrapped, unsigned char other_pubkey[CRYPTO_PUBKEY_BYTES]){
    if (!intr || !conn || !wrapped) return -1;
    return e2ee_conn_init(wrapped, conn, intr->self_sign, other_pubkey);
}

int soter2_e2ee_end_handshake(e2ee_connection *econn, int timeout){
    if (0 >= e2ee_conn_handshake_wait(econn, timeout)){
        fprintf(stderr, "[main][e2ee] hs wait failed\n");
        return 1;
    }

    if (0 > e2ee_conn_handshake_resp(econn)){
        fprintf(stderr, "[main][e2ee] hs response failed\n");
        return -1;
    }

    return 0;
}

int soter2_e2ee_handshake(e2ee_connection *econn){
    return e2ee_conn_handshake_init(econn);
}

void soter2_iconnect(
    soter2_interface *intr, 
    naddr_t address, 
    uint32_t UID, 
    const unsigned char pubkey[CRYPTO_PUBKEY_BYTES]
){
    peer_info inf = (peer_info){
        .last_seen = mt_time_get_millis_monocoarse(),
        .UID = UID,
        .state = PEER_ST_INITED,
        .nfd = ln_netfdq(&address),
        .ctx = NULL
    };
    memcpy(inf.pubkey, pubkey, CRYPTO_PUBKEY_BYTES);
    
    peers_db_add(&intr->pdb, inf);

    protopack *punch_msg = proto_msg_quick(intr->rudp_disp.self_uid, UID, 0, PACK_PUNCH);
    
    nnet_fd nfd = ln_netfdq(&address);
    pvd_sender_send(&intr->sender, punch_msg, &nfd);
    free(punch_msg);
}

typedef struct {
    stun_addr *addresses;
    size_t     n_to_resolve;

    prot_array *out_addrs;
} resolution_args;

static void *_parallel_stun_resolution(void *_args){
    resolution_args *args = _args;

    for (size_t i = 0; i < args->n_to_resolve; i++){
        stun_addr curr = *(args->addresses + i);

        naddr_t out;
        if (0 > ln_uni(curr.uniaddr, curr.port, &out))
            continue;

        prot_array_push(args->out_addrs, &out);
    }

    free(_args);
    return NULL;
}

nat_type soter2_intr_STUN(soter2_interface *intr, stun_addr *stuns, size_t stuns_n){
    int paral_res_n = STUN_PARALLEL_RESOLUTION_LIMIT > stuns_n ? stuns_n: STUN_PARALLEL_RESOLUTION_LIMIT;

    prot_array q;
    prot_array_create(sizeof(naddr_t), &q);

    pthread_t threads[paral_res_n];
    for (int i = 0; i < paral_res_n; i++){
        resolution_args *args = malloc(sizeof(*args));
        args->addresses = stuns + i * (paral_res_n / stuns_n);
        args->n_to_resolve = paral_res_n / stuns_n;
        args->out_addrs = &q;

        int r = pthread_create(&threads[i], NULL, _parallel_stun_resolution, args);
        if (0 > r) {
            free(args);
            return NAT_ERROR;
        }
    }

    for (int i = 0; i < paral_res_n; i++)
        pthread_join(threads[i], NULL);

    nat_type type = NAT_ERROR;

    if (prot_array_len(&q) < 2)
        goto end;

    size_t first_i = 0, second_i = 1, len = prot_array_len(&q);
    do {
        uint16_t port = 1024 + (rand() % 63000);

        naddr_t *stun1 = prot_array_at(&q, first_i);
        naddr_t *stun2 = prot_array_at(&q, second_i);

        int r = nat_get_type(&intr->sock, stun1, stun2, port, &type);
        if (r == 0) break;
        if (r == -1) goto end;

        // first stun failed
        if (r == -2) {
            first_i++;
            if (first_i == second_i) 
                first_i++;

            if (first_i >= len)
                goto end;
        }

        // second stun failed
        if (r == -3) {
            second_i++;
            if (second_i == first_i)
                second_i++;

            if (second_i >= len)
                goto end;
        }
    } while(true);

end:
    intr->NAT = type;

    prot_array_end(&q);
    return intr->NAT;
}

int soter2_istatewait(soter2_interface *intr, uint32_t c_uid, peer_state state, peer_info *info){
    return peers_db_wait(&intr->pdb, c_uid, state, info);
}

bool soter2_irunning(soter2_interface *intr){
    if (!intr) return false;
    return atomic_load(&intr->is_running);
}

// -- user-connections

int soter2_iget_conn(soter2_interface *intr, rudp_connection **conn, uint32_t c_uid){
    if (!intr || !conn) return -1;
    
    return rudp_get_connection(&intr->rudp_disp, c_uid, conn);
}

int soter2_intr_wait_conn(soter2_interface *intr, rudp_connection **conn, int timeout){
    if (!intr || !conn) return -1;

    int r = mt_evsock_wait(&intr->_new_active_conn, timeout);
    if (0 >= r) return r;

    rudp_dispatcher *disp = &intr->rudp_disp;
    prot_table_lock(&disp->connections);

    dyn_pair *pair = dyn_array_at(
        &disp->connections.table.array, 
        disp->connections.table.array.len - 1
    );
    if (!pair) return -1;

    *conn = *((rudp_connection**)pair->second);
    prot_table_unlock(&disp->connections);

    if (!(*conn)) return -1;
    
    return 1;
}

protopack *soter2_irecv (rudp_connection *conn){
    protopack *r = NULL;
    rudp_conn_recv(conn, &r);
    return r;
}

int soter2_isend_r(rudp_connection *conn, protopack *p){
    return rudp_conn_send(conn, p);
}

int soter2_isend(rudp_connection *conn, void *data, size_t dsize){
    if (!conn || conn->closed) return -1;
    
    protopack *p = udp_make_pack(
        0, conn->s_uid, conn->c_uid, PACK_DATA, data, dsize
    );

    int r = rudp_conn_send(conn, p);
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

// always listen to state server
// and automaticly make new connections
static void* _state_worker(void *_args){
    soter2_interface *intr = _args;

    while (atomic_load(&intr->is_running)){
        int r = mt_evsock_wait(&intr->ssyst.new_state_fd, 50);
        mt_evsock_drain(&intr->ssyst.new_state_fd);
        
        if (r > 0){
            state_request req;
            while (0 == prot_queue_pop(&intr->ssyst.new_state_ans, &req)){
                if (peers_db_check(&intr->pdb, req.uid))
                    continue;
                
                soter2_iconnect(intr, req.addr, req.uid, req.pubkey);
                printf("[daemon] connecting to %u\n", req.uid);
            }
        }

        // checking statuses
        peer_info *snapshot = NULL;
        size_t snapshot_sz = peers_db_snapshot(&intr->pdb, &snapshot);

        for (size_t i = 0; i < snapshot_sz; i++){
            peer_info info = snapshot[i];
            if (info.state != PEER_ST_ACTIVE) continue;

            rudp_connection *existing_conn = NULL;
            if (0 == rudp_get_connection(&intr->rudp_disp, info.UID, &existing_conn)) {
                continue; 
            }

            rudp_connection *local_conn = NULL;
            if (0 > rudp_est_connection(&intr->rudp_disp, &local_conn, info.UID, &info.nfd)){
                fprintf(stderr, "[daemon] failed to est connection with %u\n", info.UID);
                continue;
            }

            if (0 > prot_table_set(
                &intr->_active_conns,
                &info.UID,
                &local_conn
            )){
                fprintf(stderr, "[daemon] failed to set new connection: %u\n", info.UID);
                continue;
            }
            mt_evsock_notify(
                &intr->_new_active_conn
            );
        }

        free(snapshot);
    }

    return 0;
}

static int ping_iter  (const peer_info *info, app_context *ctx);
static int gossip_iter(const peer_info *info, app_context *ctx);
static int relay_iter (const peer_info *info, protopack *unpacked, rele_dispatcher *disp);
static int state_iter (soter2_interface *intr);

static void *iter_daemon(void *_args){
    soter2_interface *intr = _args;

    while (atomic_load(&intr->is_running)){
        intr->ctx.now_ms = mt_time_get_millis_monocoarse();
        int r = mt_evsock_wait(&intr->listener.newpack_es, 1000);
        mt_evsock_drain(&intr->listener.newpack_es);

        if (intr->ctx.now_ms - intr->packets_timestamp >= 1000) {
            int64_t delta_ms = intr->ctx.now_ms - intr->packets_timestamp;

            pthread_mutex_lock(&intr->rate_mtx);
            if (delta_ms > 0) {
                float delta_sec = delta_ms / 1000.0f;
                
                intr->packet_rate = (float)intr->packets_translated / delta_sec;
            } else {
                intr->packet_rate = 0.0f;
            }
            pthread_mutex_unlock(&intr->rate_mtx);
            
            intr->packets_timestamp = intr->ctx.now_ms;
            intr->packets_translated = 0;
        }

        if (intr->packet_rate > SOTER_LOW_PACKET_RATE) goto skip_addt;
        
        if (intr->ctx.now_ms - intr->state_last_called >= 3 * 1000){
            state_iter(intr);
            intr->state_last_called = intr->ctx.now_ms;
        }

        peer_info *snapshot = NULL;
        size_t snapshot_sz = peers_db_snapshot(&intr->pdb, &snapshot);

        for (size_t i = 0; i < snapshot_sz; i++){
            peer_info info = snapshot[i];
            if (!ping_iter(&info, &intr->ctx)) continue;

            peers_db_remove(&intr->pdb, info.UID);
        }

        if (intr->ctx.now_ms - intr->gsyst.last_gossiped >= SOTER_GOSSIP_DT * 1000){
            gossip_cleanup(&intr->gsyst);
            for (size_t i = 0; i < snapshot_sz; i++){
                peer_info info = snapshot[i];
                if (!gossip_iter(&info, &intr->ctx)) continue;

                peers_db_remove(&intr->pdb, info.UID);
            }
            intr->gsyst.last_gossiped = intr->ctx.now_ms;
        }

        free(snapshot);

skip_addt:

        if (r == 0) continue;
        if (r < 0) {
            perror("poll()");
            continue;
        }

        listener_packet pkt;
        while (0 == pvd_next_packet(&intr->listener, &pkt)){
            
            protopack_type ptype     = pkt.pack->packtype;
            uint32_t       relay_to  = pkt.pack->h_to;
            nnet_fd        relay_nfd = pkt.from_who; 

            protopack *unpacked = NULL;
            if (1 == rele_unpack(&intr->rele_disp, pkt.pack, &unpacked)){
                unpacked = pkt.pack;
            } else free(pkt.pack);

            if (ptype == PACK_RELAYED){
                
                // relayed to us
                if (relay_to == intr->rudp_disp.self_uid)
                    goto pack_processing;

                // otherwise relay forward

                // firstly check, if I know this UID
                peer_info addresant;
                if (0 == peers_db_get(&intr->pdb, relay_to, &addresant)){
                    rele_forward(&intr->rele_disp, unpacked, relay_to, &relay_nfd);
                    free(unpacked);
                    continue;
                }

                // if not, iterate by all peers database
                peer_info *snapshot = NULL;
                size_t snapshot_sz = peers_db_snapshot(&intr->pdb, &snapshot);
                for (size_t i = 0; i < snapshot_sz; i++){
                    peer_info info = snapshot[i];
                    if (!relay_iter(&info, unpacked, &intr->rele_disp)) continue;

                    peers_db_remove(&intr->pdb, info.UID);
                }

                free(snapshot);
                free(unpacked);
                continue;
            }
            
pack_processing:
            if (udp_is_RUDP_req(unpacked->packtype)){
                rudp_dispatcher_pass(&intr->rudp_disp, unpacked);
                intr->packets_translated++;
            } else {
                watcher_pass(&intr->wtch, unpacked, &pkt.from_who);
            }
        }
    }

    return NULL;
}

static int state_iter(soter2_interface *intr){
    // requesting to server
    state_request r = state_rcreate(
        intr->sock.addr, 
        intr->rudp_disp.self_uid, 
        REQUEST_CONNECTION, 
        intr->self_sign
    );

    protopack *pack = udp_make_pack(0, intr->rudp_disp.self_uid, 0, PACK_STATE, &r, sizeof(r));
    
    prot_table_lock(&intr->state_servs);
    for (size_t i = 0; i < intr->state_servs.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&intr->state_servs.table.array, i);
        soter2_state_intr *st = pair->second;

        if (st->req_dt == 0) {
            continue;
        }

        nnet_fd n = ln_netfdq(&st->addr);
        pvd_sender_send(&intr->sender, pack, &n);
    }
    prot_table_unlock(&intr->state_servs);
    free(pack);

    return 0;
}

static int ping_iter(const peer_info *info, app_context *ctx){
    if (ctx->now_ms - info->last_seen >= SOTER_DEAD_DT * 1000){
        printf("Dead peer detected: %u\n", info->UID);
        return 1;
    }

    if (ctx->now_ms - info->last_seen >= SOTER_REPING_DT * 1000){
        protopack *ping = proto_msg_quick(ctx->rudp->self_uid, info->UID, 0, PACK_PING);
        pvd_sender_send(ctx->sender, ping, &info->nfd);
        free(ping);
    }

    return 0;
}

static int gossip_iter(const peer_info *info, app_context *ctx){
    // we dont check dt because of 1 check in iter() daemon
    // do not spam to all peers
    if (rand() % 2 == 0) return 0;

    naddr_t addr = ln_nfd2addr(&info->nfd);
    gossip_entry *entry = gossip_create_entry(info->UID, &addr, 0, NULL, true);
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

static int relay_iter(
    const peer_info *info, 
    protopack       *unpacked, 
    rele_dispatcher *disp
){
    return rele_forward(disp, unpacked, info->UID, &info->nfd);
}