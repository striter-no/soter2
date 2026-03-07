#include <peers/peerdb.h>
#include <stdlib.h>

int peers_db_init(peers_db *db){
    if (!db) return -1;
    
    db->data = prot_table_create(
        sizeof(uint32_t), sizeof(peer_info), DYN_OWN_BOTH
    );

    return 0;
}

int peers_db_end(peers_db *db){
    if (!db) return -1;
    
    prot_table_end(&db->data);
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
    peer_info *info_ = prot_table_get(&db->data, &UID);
    if (!info_) return -1;

    memcpy(info, info_, sizeof(*info));
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
    
    peer_info *info = prot_table_get(&db->data, &UID);
    if (!info) {
        prot_table_unlock(&db->data);
        return -1;
    }

    info->state = new_state;
    
    prot_table_unlock(&db->data);
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

int peers_db_faddr(peers_db *db, peer_info **infos, size_t *info_sz, naddr_t addr){
    if (!db || !infos || !info_sz) return -1;
    
    prot_table_lock(&db->data);
    
    *info_sz = 0;
    *infos = NULL;

    size_t count = 0;
    for (size_t i = 0; i < db->data.table.array.len; i++){
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (!pair) continue;

        peer_info *info = pair->second;
        
        if (ln_addrcmp(addr, ln_nfd2addr(info->nfd))) count++;
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
        if (ln_addrcmp(addr, ln_nfd2addr(info->nfd))){
            (*infos)[idx++] = *info;
        }
    }

    *info_sz = count;
    prot_table_unlock(&db->data);
    return 0;
}

void peers_db_foreach(peers_db *db, peer_iter_fn func, void *ctx) {
    prot_table_lock(&db->data);
    for (size_t i = 0; i < db->data.table.array.len; i++) {
        dyn_pair *pair = dyn_array_at(&db->data.table.array, i);
        if (pair) func((peer_info*)pair->second, ctx);
    }
    prot_table_unlock(&db->data);
}