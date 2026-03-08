#include <base/prot_table.h>
#include <multithr/events.h>
#include <lownet/core.h>
#include <stdint.h>

#ifndef PEER_SYSTEM_PEERDB_H

typedef enum {
    PEER_ST_INITED,
    PEER_ST_NATPUNCHING,
    PEER_ST_HANDSHAKING,
    PEER_ST_ACTIVE
} peer_state;

typedef struct {
    peer_state state;
    uint32_t   UID;
    uint32_t   last_seen;
    
    nnet_fd    nfd;
    void      *ctx;
} peer_info;

typedef struct {
    prot_table   data;
    mt_eventsock statchange;
} peers_db;

int peers_db_init(peers_db *db);
int peers_db_end (peers_db *db);

int  peers_db_add   (peers_db *db, peer_info info);
int  peers_db_remove(peers_db *db, uint32_t UID);
int  peers_db_get   (peers_db *db, uint32_t UID, peer_info *info);

bool peers_db_check  (peers_db *db, uint32_t UID);
bool peers_db_scheck (peers_db *db, uint32_t UID, peer_state state);
int  peers_db_schange(peers_db *db, uint32_t UID, peer_state new_state);
int  peers_db_unfd   (peers_db *db, uint32_t UID, nnet_fd nfd);
int  peers_db_utime  (peers_db *db, uint32_t UID);

int  peers_db_fstate(peers_db *db, peer_info **infos, size_t *info_sz, peer_state state);
int  peers_db_faddr (peers_db *db, peer_info **infos, size_t *info_sz, naddr_t    addr);

// UINT32_MAX for waiting any new updates
int  peers_db_wait  (peers_db *db, uint32_t UID, peer_state state, peer_info *got_info);

typedef int (*peer_iter_fn)(peer_info *info, void *ctx);
void peers_db_foreach(peers_db *db, peer_iter_fn func, void *ctx);

#endif
#define PEER_SYSTEM_PEERDB_H