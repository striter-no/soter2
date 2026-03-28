#include <peers/peerdb.h>
#include <stdio.h>
#include <stdlib.h>

int peers_db_init(peers_db *db){
    if (!db) return -1;

    if (0 > mt_evsock_new(&db->statchange)){
        return -1;
    }

    prot_table_create(
        sizeof(uint32_t), sizeof(peer_info), DYN_OWN_BOTH, &db->data
    );

    return 0;
}

int peers_db_end(peers_db *db){
    if (!db) return -1;

    prot_table_end(&db->data);
    mt_evsock_close(&db->statchange);
    return 0;
}

// -- peers managment methods
int peers_db_add(peers_db *db, peer_info info){
    if (!db) return -1;

    prot_table_set(&db->data, &info.UID, &info);
    return 0;
}

int peers_db_remove(peers_db *db, uint32_t UID){
    if (!db) return -1;

    return prot_table_remove(&db->data, &UID);
}

int peers_db_get(peers_db *db, uint32_t UID, peer_info *info){
    if (!db) return -1;
    prot_table_lock(&db->data);
    peer_info *info_ = _prot_table_get_unsafe(&db->data, &UID);
    if (!info_) {
        prot_table_unlock(&db->data);
        return -1;
    }
    memcpy(info, info_, sizeof(*info));
    prot_table_unlock(&db->data);
    return 0;
}

// -- checking and changing
bool peers_db_check(peers_db *db, uint32_t UID){
    if (!db) return -1;

    return prot_table_get(&db->data, &UID) != NULL;
}

bool peers_db_scheck(peers_db *db, uint32_t UID, peer_state state){
    if (!db) return -1;
    peer_info info;
    if (0 > peers_db_get(db, UID, &info)) return false;

    return info.state == state;
}

int peers_db_schange(peers_db *db, uint32_t UID, peer_state new_state){
    if (!db) return -1;
    prot_table_lock(&db->data);

    peer_info *info = _prot_table_get_unsafe(&db->data, &UID);
    if (!info) {
        prot_table_unlock(&db->data);
        return -1;
    }

    info->state = new_state;
    prot_table_unlock(&db->data);
    mt_evsock_notify(&db->statchange);
    return 0;
}

int peers_db_unfd(peers_db *db, uint32_t UID, const nnet_fd *nfd){
    if (!db) return -1;
    prot_table_lock(&db->data);

    peer_info *info = _prot_table_get_unsafe(&db->data, &UID);
    if (!info) {
        prot_table_unlock(&db->data);
        // fprintf(stderr, "[peersdb][unfd]: failed to get info\n");
        return -1;
    }

    info->nfd = *nfd;
    // naddr_t addr = ln_nfd2addr(nfd);
    // printf("[peersdb][unfd] changed nfd to %s:%u\n", addr.ip.v4.ip, addr.ip.v4.port);

    prot_table_unlock(&db->data);
    return 0;
}

int peers_db_utime(peers_db *db, uint32_t UID){
    if (!db) return -1;
    prot_table_lock(&db->data);

    peer_info *info = _prot_table_get_unsafe(&db->data, &UID);
    if (!info) {
        prot_table_unlock(&db->data);
        // fprintf(stderr, "[peersdb][utime]: failed to get info\n");
        return -1;
    }

    info->last_seen = mt_time_get_millis_monocoarse();

    prot_table_unlock(&db->data);
    return 0;
}

// -- light info, conversions

int peers_db_linfo(const peer_info *input, light_peer_info *out){
    if (!input || !out) return -1;

    memset(out, 0, sizeof(*out));

    out->UID = input->UID;
    out->addr = ln_nfd2addr(&input->nfd);
    out->sserv_addr = input->sserv_addr;
    memcpy(out->pubkey, input->pubkey, CRYPTO_PUBKEY_BYTES);

    return 0;
}

int peers_db_reconstruct(peer_info *out, const light_peer_info *input){
    if (!input || !out) return -1;

    memset(out, 0, sizeof(*out));

    naddr_t addr = input->addr;
    out->UID = input->UID;
    out->nfd = ln_netfdq(&addr);
    out->ctx = NULL;
    out->last_seen = mt_time_get_millis_monocoarse();
    out->state = PEER_ST_INITED;
    out->sserv_addr = input->sserv_addr;
    memcpy(out->pubkey, input->pubkey, CRYPTO_PUBKEY_BYTES);

    return 0;
}


// -- filtering
int peers_db_fstate(peers_db *db, peer_info **infos, size_t *info_sz, peer_state state){
    if (!db || !infos || !info_sz) return -1;

    prot_table_lock(&db->data);

    *info_sz = 0;
    *infos = NULL;

    size_t count = 0;
    for (size_t i = 0; i < db->data.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (!pair) continue;

        peer_info *info = pair->second;
        if (info->state == state) count++;
    }

    if (count == 0) {
        prot_table_unlock(&db->data);
        return 0;
    }

    *infos = malloc(sizeof(peer_info) * count);
    if (!*infos) {
        prot_table_unlock(&db->data);
        return -1;
    }

    size_t idx = 0;
    for (size_t i = 0; i < db->data.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (!pair) continue;

        peer_info *info = pair->second;
        if (info->state == state) {
            (*infos)[idx++] = *info;
        }
    }

    *info_sz = count;
    prot_table_unlock(&db->data);
    return 0;
}

int peers_db_faddr(peers_db *db, peer_info **infos, size_t *info_sz, naddr_t *addr){
    if (!db || !infos || !info_sz) return -1;

    prot_table_lock(&db->data);

    *info_sz = 0;
    *infos = NULL;

    size_t count = 0;
    for (size_t i = 0; i < db->data.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (!pair) continue;

        peer_info *info = pair->second;

        naddr_t n = ln_nfd2addr(&info->nfd);
        if (ln_addrcmp(addr, &n)) count++;
    }

    if (count == 0) {
        prot_table_unlock(&db->data);
        return 0;
    }

    *infos = malloc(sizeof(peer_info) * count);
    if (!*infos) {
        prot_table_unlock(&db->data);
        return -1;
    }

    size_t idx = 0;
    for (size_t i = 0; i < db->data.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (!pair) continue;

        peer_info *info = pair->second;
        naddr_t n = ln_nfd2addr(&info->nfd);
        if (ln_addrcmp(addr, &n)){
            (*infos)[idx++] = *info;
        }
    }

    *info_sz = count;
    prot_table_unlock(&db->data);
    return 0;
}

int peers_db_wait(peers_db *db, uint32_t UID, peer_state state, peer_info *info){
    while (true){
        int r = mt_evsock_wait(&db->statchange, -1);
        if (r == -1) return -1;
        mt_evsock_drain(&db->statchange);

        prot_table_lock(&db->data);
        bool found = false;
        for (size_t i =0; i < db->data.table.array.len; i++) {
            dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
            if (!pair) continue;

            uint32_t _uid; peer_info _info;
            memcpy(&_uid, pair->first, sizeof(_uid));
            memcpy(&_info, pair->second, sizeof(_info));

            if (_uid == UID && _info.state == state){
                memcpy(info, &_info, sizeof(_info));
                found = true;
                break;
            }
        }
        prot_table_unlock(&db->data);

        if (found) return 0;
    }
}

size_t peers_db_snapshot(peers_db *db, peer_info **out){
    size_t o_size = 0;
    prot_table_lock(&db->data);
    *out = malloc(sizeof(peer_info) * db->data.table.array.len);

    for (size_t i = 0; i < db->data.table.array.len; i++) {
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (!pair) continue;

        (*out)[o_size++] = *(peer_info*)pair->second;
    }
    prot_table_unlock(&db->data);

    return o_size;
}
