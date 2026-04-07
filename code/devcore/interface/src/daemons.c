#include "soter2/systems.h"
#include <soter2/daemons.h>

// broadacasting PACK_STATE (REQUEST_CONNECTION) to all servers
static int state_iter(s2_systems *s2s){
    // requesting to server
    state_request r = state_rcreate(
        s2s->io_sys.usock.addr,
        s2s->rudp_disp.self_uid,
        REQUEST_PEERS,
        s2s->crypto_mod.self_sign
    );

    protopack *pack = udp_make_pack(0, s2s->rudp_disp.self_uid, 0, PACK_STATE, &r, sizeof(r));

    prot_table_lock(&s2s->state_mod.servers);
    for (size_t i = 0; i < s2s->state_mod.servers.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&s2s->state_mod.servers.table.array, i);
        s2_state_intr *st = pair->second;

        if (st->req_dt == 0) {
            continue;
        }

        nnet_fd n = ln_netfdq(&st->addr);
        pvd_sender_send(&s2s->io_sys.sender, pack, &n);
    }
    prot_table_unlock(&s2s->state_mod.servers);
    free(pack);

    return 0;
}

// pinging given peer
static int ping_iter(const peer_info *info, s2_systems *s2s){
    if (s2s->ctx.now_ms - info->last_seen >= 10 * 1000){
        printf("Dead peer detected: %u\n", info->UID);

        prot_queue_push(&s2s->conn_mod.reqd_disconns, &info->UID);
        mt_evsock_notify(&s2s->conn_mod.reqd_conn_ev);
        return 1;
    }

    if (s2s->ctx.now_ms - info->last_seen >= 3 * 1000){
        protopack *ping = proto_msg_quick(s2s->rudp_disp.self_uid, info->UID, 0, PACK_PING);
        pvd_sender_send(&s2s->io_sys.sender, ping, &info->nfd);
        free(ping);
    }

    return 0;
}

// sending gossip to peer with 50% chance
static int gossip_iter(const peer_info *info, app_context *ctx){
    // we dont check dt because of 1 check in iter() daemon
    // do not spam to all peers
    if (rand() % 2 == 0) return 0;

    naddr_t addr = ln_nfd2addr(&info->nfd);
    gossip_metadata md = {.available_state_server = info->server};
    memcpy(md.pubkey, info->pubkey, CRYPTO_PUBKEY_BYTES);

    gossip_entry entry = gossip_create_entry(info->UID, &addr, md, true);
    gossip_new_entry(ctx->g_syst, &entry);

    size_t entries_c = 5;
    gossip_entry *entries = NULL;
    if (1 == gossip_random_entries(ctx->g_syst, &entries, &entries_c)){
        // no entries
        return 0;
    }

    size_t   packet_dsize = 0;
    uint8_t *packet_data = NULL;
    gossip_to_data(ctx->g_syst, &entries, entries_c, &packet_data, &packet_dsize);

    protopack *gossip = udp_make_pack(
        0, ctx->rudp->self_uid, info->UID, PACK_GOSSIP, packet_data, packet_dsize
    );

    free(entries);
    free(packet_data);

    pvd_sender_send(ctx->sender, gossip, &info->nfd);
    free(gossip);
    return 0;
}

// * main: updating statistics (analysis)
// * pinging peers
// * gossiping
// * skip_addt: listening to packets, distributing it
static bool _daemon_io(void *_args){
main:
    s2_systems *s2s = _args;

    s2s->ctx.now_ms = mt_time_get_millis_monocoarse();
    int r = mt_evsock_wait(&s2s->io_sys.listener.newpack_es, 1000);
    mt_evsock_drain(&s2s->io_sys.listener.newpack_es);
    if (r < 0) return false;

    if (s2s->ctx.now_ms - s2s->analysis_mod.packets_timestamp >= 1000) {
        int64_t delta_ms = s2s->ctx.now_ms - s2s->analysis_mod.packets_timestamp;

        pthread_mutex_lock(&s2s->analysis_mod.rate_mtx);
        if (delta_ms > 0) {
            float delta_sec = delta_ms / 1000.0f;

            s2s->analysis_mod.packet_rate = (float)s2s->analysis_mod.packets_translated / delta_sec;
        } else {
            s2s->analysis_mod.packet_rate = 0.0f;
        }
        pthread_mutex_unlock(&s2s->analysis_mod.rate_mtx);

        s2s->analysis_mod.packets_timestamp = s2s->ctx.now_ms;
        s2s->analysis_mod.packets_translated = 0;
    }

    if (s2s->analysis_mod.packet_rate > 1) goto skip_addt;

    if (s2s->ctx.now_ms - s2s->analysis_mod.state_last_called >= 3 * 1000){
        state_iter(s2s);
        s2s->analysis_mod.state_last_called = s2s->ctx.now_ms;
    }

    peer_info *snapshot = NULL;
    size_t snapshot_sz = peers_db_snapshot(&s2s->peers_sys, &snapshot);

    for (size_t i = 0; i < snapshot_sz; i++){
        peer_info info = snapshot[i];
        if (!ping_iter(&info, s2s)) continue;

        peers_db_remove(&s2s->peers_sys, info.UID);
    }

    if (s2s->ctx.now_ms - s2s->gossip_sys.last_gossiped >= 5 * 1000){
        gossip_cleanup(&s2s->gossip_sys);
        for (size_t i = 0; i < snapshot_sz; i++){
            peer_info info = snapshot[i];
            if (!gossip_iter(&info, &s2s->ctx)) continue;

            peers_db_remove(&s2s->peers_sys, info.UID);
        }
        s2s->gossip_sys.last_gossiped = s2s->ctx.now_ms;
    }

    free(snapshot);

skip_addt:

    if (r == 0) return false;
    if (r < 0) {
        perror("poll()");
        return false;
    }

    listener_packet pkt;
    while (0 == pvd_next_packet(&s2s->io_sys.listener, &pkt)){

        // check TURN, if it is from TURNed client, retranslate packet to real client
        if (turn_check_pair(&s2s->turn_cli, pkt.pack->h_to, pkt.pack->h_from)){ // reversed order hash(from,to) -> hash(to,from)
            uint32_t hsh = turn_pair_hash(pkt.pack->h_to, pkt.pack->h_from); // reversed order

            protopack *outpack = NULL;
            turn_client_wrap(&s2s->turn_cli, pkt.pack, &outpack);

            turn_bind_peer bpeer;
            if (0 > turn_client_info(&s2s->turn_cli, hsh, &bpeer)){
                fprintf(stderr, "[daemon][io] turn_client_info failed on hsh: %u\n", hsh);
                goto skip;
            }

            naddr_t addr = bpeer.linfo_origin.addr;
            nnet_fd fd   = ln_netfdq(&addr);
            pvd_sender_send(&s2s->io_sys.sender, outpack, &fd);

            free(pkt.pack);
            goto skip;
        }

        if (udp_is_RUDP_req(pkt.pack->packtype)){
            printf("[s2s] rudp pkt from %u\n", pkt.pack->h_from);
            rudp_dispatcher_pass(&s2s->rudp_disp, pkt.pack);
        } else {
            proto_print(pkt.pack, 1);
            watcher_pass(&s2s->io_sys.wtch, pkt.pack, &pkt.from_who);
        }

skip:
        s2s->analysis_mod.packets_translated++;
        peers_db_utime(&s2s->peers_sys, pkt.pack->h_from);
    }

    return false;
}

// monitoring state responses
static bool _daemon_st(void *_args){
    s2_systems *s2s = _args;

    int r = mt_evsock_wait(&s2s->state_mod.sys.new_state_fd, 50);
    mt_evsock_drain(&s2s->state_mod.sys.new_state_fd);

    if (r > 0){
        state_sys_request sreq;
        while (0 == prot_queue_pop(&s2s->state_mod.sys.new_state_ans, &sreq)){
            if (peers_db_check(&s2s->peers_sys, sreq.req.uid))
                continue;

            light_peer_info linfo = {
                .UID = sreq.req.uid,
                .addr = sreq.req.addr,
                .server = sreq.come_from
            };
            memcpy(linfo.pubkey, sreq.req.pubkey, CRYPTO_PUBKEY_BYTES);

            prot_queue_push(&s2s->conn_mod.reqd_conns, &linfo);
            mt_evsock_notify(&s2s->conn_mod.reqd_conn_ev);

            printf("[daemon] requested linking with %u\n", sreq.req.uid);
        }
    }

    // checking statuses
    peer_info *snapshot = NULL;
    size_t snapshot_sz = peers_db_snapshot(&s2s->peers_sys, &snapshot);

    for (size_t i = 0; i < snapshot_sz; i++){
        peer_info info = snapshot[i];
        if (info.state != PEER_ST_ACTIVE) continue;

        rudp_connection *existing_conn = NULL;
        if (0 == rudp_get_connection(&s2s->rudp_disp, info.UID, &existing_conn)) {
            continue;
        }

        rudp_connection *local_conn = NULL;
        if (0 > rudp_est_connection(&s2s->rudp_disp, &local_conn, info.UID, &info.nfd)){
            fprintf(stderr, "[daemon] failed to est connection with %u\n", info.UID);
            continue;
        }

        if (0 > prot_table_set(
            &s2s->conn_mod.active,
            &info.UID,
            &local_conn
        )){
            fprintf(stderr, "[daemon] failed to set new connection: %u\n", info.UID);
            continue;
        }
        mt_evsock_notify(&s2s->conn_mod.active_ev);
    }

    free(snapshot);
    return false;
}

int s2_daemons_create(s2_daemons *d, s2_systems *ctx){
    if (!d) return -1;

    daemon_run(&d->io_daemon, true, _daemon_io, ctx);
    daemon_run(&d->st_daemon, true, _daemon_st, ctx);

    return 0;
}

int s2_daemons_stop(s2_daemons *d){
    if (!d) return -1;

    daemon_stop(&d->io_daemon);
    daemon_stop(&d->st_daemon);

    return 0;
}
