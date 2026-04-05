#ifndef INTERFACE_SYSTEMS_H
#define INTERFACE_SYSTEMS_H

#include "_modules.h"
#include "handlers.h"

typedef struct {
    naddr_t addr;
    int     req_dt;
} s2_state_intr;

typedef struct {
    watcher_handler ACK;
    watcher_handler PUNCH;
    watcher_handler PING;
    watcher_handler PONG;
    watcher_handler GOSSIP;
    watcher_handler STATE;
    watcher_handler RAW_UDP;
} s2_ivtable;

typedef struct {
    prot_queue   reqd_conns; // type: light_peer_info
    mt_eventsock reqd_conn_ev;

    prot_queue   reqd_disconns; // type: uint32 (uid)
    mt_eventsock reqd_disc_ev;

    prot_table   active;    // types: uint32: *rudp_connection
    mt_eventsock active_ev;
} s2s_connections;

typedef struct {
    pvd_listener listener;
    pvd_sender   sender;

    ln_socket usock;
    watcher wtch;
} s2_io_system;

typedef struct {
    sign     self_sign;
    uint32_t UID;
} s2_crypto;

typedef struct {
    int64_t   state_last_called;

    int64_t   packets_timestamp;
    uint32_t  packets_translated;
    float     packet_rate;

    pthread_mutex_t rate_mtx;
} s2_analysis;

typedef struct {
    state_system sys;
    prot_table   servers;
} s2_stating;

typedef struct {
    turn_client     turn_cli;
    rudp_dispatcher rudp_disp;
    gossip_system   gossip_sys;
    peers_db        peers_sys;

    s2_io_system    io_sys;
    s2_crypto       crypto_mod;
    s2_analysis     analysis_mod;
    s2_stating      state_mod;
    s2s_connections conn_mod;

    app_context    ctx;
} s2_systems;

int s2_systems_create(s2_systems *s);
int s2_systems_run   (s2_systems *s);
int s2_systems_stop  (s2_systems *s);
void s2_sys_shandlers(s2_systems *s, s2_ivtable vt);

const char *s2_fingerprint(s2_systems *sys);
int s2_sys_save_sign(s2_systems *sys, const char *path);
int s2_sys_load_sign(s2_systems *sys, const char *path);
int s2_sys_upd_sign(s2_systems *sys,  const char *path);

#endif
