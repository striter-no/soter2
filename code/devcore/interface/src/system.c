#include "soter2/daemons.h"
#include <pthread.h>
#include <soter2/systems.h>
#include <stdint.h>
#include <sys/stat.h>

int s2_systems_create(s2_systems *s){
    if (!s) return -1;

    // crypto module
    if (0 > crypto_init()) return -1;
    s->crypto_mod.self_sign = sign_gen();
    s->crypto_mod.UID = crypto_pubkey_to_uid(s->crypto_mod.self_sign.id_pub);

    // io module
    if (0 > ln_usock_new    (&s->io_sys.usock))                                        goto fail;
    if (0 > pvd_listener_new(&s->io_sys.listener, &s->io_sys.usock))                   goto fail;
    if (0 > pvd_sender_new  (&s->io_sys.sender, &s->io_sys.usock))                     goto fail;
    if (0 > watcher_init    (&s->io_sys.wtch, &s->io_sys.sender, &s->io_sys.listener)) goto fail;

    // analysis module
    s->analysis_mod.packet_rate = 0.f;
    s->analysis_mod.packets_timestamp = 0;
    s->analysis_mod.state_last_called = 0;
    s->analysis_mod.packets_translated = 0;
    pthread_mutex_init(&s->analysis_mod.rate_mtx, NULL);

    // state module
    if (0 > prot_table_create(sizeof(uint32_t), sizeof(s2_state_intr), DYN_OWN_BOTH, &s->state_mod.servers)) goto fail;
    if (0 > state_sys_init(&s->state_mod.sys)) goto fail;

    // connection module
    if (0 > mt_evsock_new(&s->conn_mod.reqd_conn_ev)) goto fail;
    if (0 > mt_evsock_new(&s->conn_mod.reqd_disc_ev)) goto fail;
    if (0 > mt_evsock_new(&s->conn_mod.active_ev))    goto fail;

    if (0 > prot_table_create(sizeof(uint32_t), sizeof(rudp_connection*), DYN_OWN_BOTH, &s->conn_mod.active)) goto fail;
    if (0 > prot_queue_create(sizeof(uint32_t), &s->conn_mod.reqd_disconns)) goto fail;
    if (0 > prot_queue_create(sizeof(light_peer_info), &s->conn_mod.reqd_conns)) goto fail;

    // subsystems
    if (0 > peers_db_init      (&s->peers_sys))                                       goto fail;
    if (0 > gossip_system_init (&s->gossip_sys, s->crypto_mod.UID))                   goto fail;
    if (0 > rudp_dispatcher_new(&s->rudp_disp, &s->io_sys.sender, s->crypto_mod.UID)) goto fail;
    if (0 > turn_client_init   (&s->turn_cli, s->crypto_mod.UID, &s->io_sys.sender))  goto fail;

    // registering handlers

    watcher_handler_reg(&s->io_sys.wtch, PACK_ACK,    (watcher_handler){soter2_hnd_ACK,    &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_PUNCH,  (watcher_handler){soter2_hnd_PUNCH,  &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_PING,   (watcher_handler){soter2_hnd_PING,   &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_PONG,   (watcher_handler){soter2_hnd_PONG,   &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_GOSSIP, (watcher_handler){soter2_hnd_GOSSIP, &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_STATE,  (watcher_handler){soter2_hnd_STATE,  &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_TURN,   (watcher_handler){soter2_hnd_TURN,   &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_HELLO,  (watcher_handler){soter2_hnd_HELLO,  &s->ctx});
    watcher_handler_reg(&s->io_sys.wtch, PACK_HELLO2, (watcher_handler){soter2_hnd_HELLO2, &s->ctx});

    // context

    s->ctx = (app_context){
        .g_syst   = &s->gossip_sys,
        .p_db     = &s->peers_sys,
        .watcher  = &s->io_sys.wtch,
        .rudp     = &s->rudp_disp,
        .listener = &s->io_sys.listener,
        .sender   = &s->io_sys.sender,
        .ssytem   = &s->state_mod.sys,
        .now_ms   = mt_time_get_millis_monocoarse(),
        .turn     = &s->turn_cli
    };

    return 0;

fail:
    return -1;
}

int s2_systems_run(s2_systems *s){
    if (!s) return -1;

    if (0 > pvd_listener_start(&s->io_sys.listener)) goto fail;
    if (0 > pvd_sender_start(&s->io_sys.sender))     goto fail;
    if (0 > watcher_start(&s->io_sys.wtch))          goto fail;
    if (0 > rudp_dispatcher_run(&s->rudp_disp))      goto fail;

    return 0;

fail:
    return -1;
}

int s2_systems_stop(s2_systems *s){
    if (!s) return -1;

    // ending subsystems
    rudp_dispatcher_end(&s->rudp_disp);
    gossip_system_end(&s->gossip_sys);
    peers_db_end(&s->peers_sys);
    turn_client_end(&s->turn_cli);

    // ending io_sys
    watcher_end(&s->io_sys.wtch);
    pvd_listener_end(&s->io_sys.listener);
    pvd_sender_end(&s->io_sys.sender);
    ln_usock_close(&s->io_sys.usock);

    // ending analysis module
    pthread_mutex_destroy(&s->analysis_mod.rate_mtx);

    // ending state module
    prot_table_end(&s->state_mod.servers);
    state_sys_end(&s->state_mod.sys);

    // ending connection module
    mt_evsock_close(&s->conn_mod.reqd_conn_ev);
    mt_evsock_close(&s->conn_mod.reqd_disc_ev);
    mt_evsock_close(&s->conn_mod.active_ev);

    prot_queue_end(&s->conn_mod.reqd_conns);
    prot_queue_end(&s->conn_mod.reqd_disconns);
    prot_table_end(&s->conn_mod.active);

    return 0;
}


const char *s2_fingerprint(s2_systems *sys){
    return crypto_fingerprint(sys->crypto_mod.self_sign.id_pub);
}

int s2_sys_save_sign(s2_systems *sys, const char *path){
    if (0 > sign_store(&sys->crypto_mod.self_sign, path))
        return -1;
    return 0;
}

int s2_sys_load_sign(s2_systems *sys, const char *path){
    if (0 > sign_load(&sys->crypto_mod.self_sign, path))
        return -1;

    sys->rudp_disp.self_uid = crypto_pubkey_to_uid(sys->crypto_mod.self_sign.id_pub);
    sys->gossip_sys.self_uid = sys->rudp_disp.self_uid;
    return 0;
}

// load if exists, save otherwise
int s2_sys_upd_sign(s2_systems *sys, const char *path){
    struct stat s;
    if (0 == stat(path, &s))
        return s2_sys_load_sign(sys, path);
    else
        return s2_sys_save_sign(sys, path);
}
