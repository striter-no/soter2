#include "rudp/_modules.h"
#include <soter2/modules.h>
#include <soter2/handlers.h>
#include <stdint.h>

#ifndef SOTER_INTERFACE_H
#define SOTER_INTERFACE_H

#ifndef SOTER_DEAD_DT
#define SOTER_DEAD_DT 6
#endif

#ifndef SOTER_REPING_DT
#define SOTER_REPING_DT 3
#endif

#ifndef SOTER_GOSSIP_DT
#define SOTER_GOSSIP_DT 10
#endif

#ifndef SOTER_LOW_PACKET_RATE
#define SOTER_LOW_PACKET_RATE 0.5
#endif

typedef struct {
    naddr_t addr;
    int     req_dt;
} soter2_state_intr;

typedef struct {
    sign self_sign;
    
    // -- stating
    prot_table state_servs;
    int64_t    state_last_called;

    state_system ssyst;

    // -- networking
    ln_usocket sock;
    nat_type   NAT;

    // -- dispatchers and sub-systems
    rele_dispatcher rele_disp;
    rudp_dispatcher rudp_disp;
    peers_db        pdb;
    gossip_system   gsyst;
    prot_table      _active_conns;
    mt_eventsock    _new_active_conn;

    watcher         wtch;
    pvd_listener    listener;
    pvd_sender      sender;

    // -- context for interface
    app_context     ctx;
    pthread_t       iter_daemon;
    pthread_t       stating_daemon;
    atomic_bool     is_running;

    int64_t         packets_timestamp;
    uint32_t        packets_translated;
    float           packet_rate;

    pthread_mutex_t rate_mtx;
} soter2_interface;

typedef struct {
    watcher_handler ACK;
    watcher_handler PUNCH;
    watcher_handler PING;
    watcher_handler PONG;
    watcher_handler GOSSIP;
    watcher_handler STATE;
    watcher_handler RAW_UDP;
} soter2_ivtable;

int  soter2_intr_init          (soter2_interface *intr);
void soter2_intr_reset_handlers(soter2_interface *intr, soter2_ivtable vt);

int soter2_intr_save_sign(soter2_interface *intr, const char *path);
int soter2_intr_load_sign(soter2_interface *intr, const char *path);
int soter2_intr_upd_sign (soter2_interface *intr, const char *path);

void soter2_intr_end (soter2_interface *intr);
int  soter2_intr_run (soter2_interface *intr);
int  soter2_intr_stateconn (soter2_interface *intr, naddr_t addr, int state_req_dt);
int  soter2_intr_statestop (soter2_interface *intr, int state_server_UID);
int  soter2_intr_wait_state(soter2_interface *intr, int timeout, state_request *out_req);

void soter2_iconnect(
    soter2_interface *intr, 
    naddr_t address, 
    uint32_t UID, 
    const unsigned char pubkey[CRYPTO_PUBKEY_BYTES],
    peer_relay_state relay_st
);

int soter2_istatewait(soter2_interface *intr, uint32_t client_uid, peer_state state, peer_info *info);

int soter2_intr_wait_conn(soter2_interface *intr, rudp_connection **conn, int timeout);
int soter2_intr_wait_connspec(soter2_interface *intr, rudp_connection **conn, uint32_t UID);
int soter2_iget_conn(soter2_interface *intr, rudp_connection **conn, uint32_t client_uid);
int soter2_close_conn(soter2_interface *intr, uint32_t UID);

int soter2_e2ee_wrap(soter2_interface *intr, rudp_connection *conn, e2ee_connection *wrapped, unsigned char other_pubkey[CRYPTO_PUBKEY_BYTES]);
int soter2_e2ee_handshake(e2ee_connection *econn);
int soter2_e2ee_end_handshake(e2ee_connection *econn, int timeout);
const char *soter2_fingerprint(soter2_interface *intr);

// Returns packet owned by caller. Caller must free returned packet.
protopack *soter2_irecv(rudp_connection *conn);

int soter2_isend_r (rudp_connection *conn, protopack *p);
int soter2_isend   (soter2_interface *intr, rudp_connection *conn, void *data, size_t dsize);

float    soter2_get_DPS  (soter2_interface *intr);
void     soter2_punch    (app_context ctx, app_peer_info peer);
nat_type soter2_intr_STUN(soter2_interface *intr, stun_addr *stuns, size_t stuns_n);
bool     soter2_irunning(soter2_interface *intr);

#endif