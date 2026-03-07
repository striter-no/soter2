#include <gossip/system.h>
#include <stdint.h>

int gossip_system_init(gossip_system *g){
    if (!g) return -1;

    prot_array_create(sizeof(gossip_entry*), &g->gossips);
    return 0;
}

int gossip_system_end(gossip_system *g){
    if (!g) return -1;

    for (size_t i = 0; i < g->gossips.array.len; i++){
        free(*(gossip_entry**)prot_array_at(&g->gossips, i));
    }

    prot_array_end(&g->gossips);
    return 0;
}


int gossip_cleanup(gossip_system *g){
    if (!g) return -1;

    uint32_t curr_timestamp = time(NULL);
    prot_array_lock(&g->gossips);
    for (size_t i = 0; i < g->gossips.array.len;){
        gossip_entry *entr = *(gossip_entry**)prot_array_at(&g->gossips, i);
        if ((curr_timestamp - entr->timestamp) < GOSSIP_DEAD_DT) {
            i++;
            continue;
        }

        free(entr);
        prot_array_remove(&g->gossips, i);
    }
    prot_array_unlock(&g->gossips);

    return 0;
}


int gossip_new_entry(gossip_system *g, gossip_entry *entry){
    if (!g || !entry) return -1;

    prot_array_lock(&g->gossips);

    gossip_entry *existing = NULL;
    size_t        existing_inx = 0;
    for (size_t i = 0; i < g->gossips.array.len; i++) {
        gossip_entry *tmp = *(gossip_entry**)prot_array_at(&g->gossips, i);
        if (tmp->uid == entry->uid) {
            existing = tmp;
            existing_inx = i;
            break;
        }
    }

    if (existing) {
        if (entry->version <= existing->version) {
            prot_array_unlock(&g->gossips);
            return 0;
        }
        
        free(existing);
        prot_array_remove(&g->gossips, existing_inx);
    }

    gossip_entry *copy;
    gossip_entry_copy(&copy, entry);

    prot_array_push(&g->gossips, &copy);
    prot_array_unlock(&g->gossips);

    return 0;
}


int gossip_random_entries(gossip_system *g, gossip_entry **entries, size_t n){
    if (!g) return -1;
    if (n == 0) return -1;
    if (n > g->gossips.array.len) n = g->gossips.array.len;

    size_t indices[n];
    memset(indices, 0, sizeof(size_t) * n);

    prot_array_lock(&g->gossips);
    for (size_t i = 0; i < n; i++){
        bool unique;
        do {
            unique = true;
            size_t rnd = rand() % g->gossips.array.len;
            for (size_t k = 0; k < i; k++) {
                if (rnd == indices[k]) { unique = false; break; }
            }
            if (unique) indices[i] = rnd;
        } while (!unique);
    }
    
    for (size_t i = 0; i < n; i++){
        gossip_entry *entr = *(gossip_entry**)prot_array_at(&g->gossips, indices[i]);
        gossip_entry_copy(&entries[i], entr);
    }
    
    prot_array_unlock(&g->gossips);
    return 0;
}


int gossip_from_packet(gossip_system *g, protopack *pack_in){
    if (!g) return -1;

    uint8_t *data    = pack_in->data;
    size_t   data_sz = pack_in->d_size;

    if (!data || data_sz < sizeof(gossip_entry) + sizeof(uint32_t))
        return -1;

    size_t entries_n = 0;
    memcpy(&entries_n, data, sizeof(uint32_t));
    data += sizeof(uint32_t);

    size_t offset = 0;
    for (size_t i = 0; i < entries_n; i++){
        gossip_entry header;
        memcpy(&header, &data[offset], sizeof(header));
        if (offset + sizeof(gossip_entry) + header.mtd_size > data_sz) 
            return -1;

        gossip_entry *entry = malloc(sizeof(gossip_entry) + header.mtd_size);
        memcpy(entry, &data[offset], sizeof(gossip_entry) + header.mtd_size);
        gossip_new_entry(g, entry);
        free(entry);

        offset += sizeof(gossip_entry) + header.mtd_size;
    }

    return 0;
}


int gossip_to_packet(gossip_system *g, gossip_entry **entries, size_t n, uint8_t **out_data, size_t *out_sz){
    if (!g || !entries || n == 0) return -1;

    uint32_t entries_count = (uint32_t)n;
    size_t outgoing_s = sizeof(uint32_t);
    
    for (size_t i = 0; i < n; i++)
        outgoing_s += sizeof(gossip_entry) + entries[i]->mtd_size;

    *out_data = malloc(outgoing_s);
    if (!*out_data) return -1;
    if (out_sz) *out_sz = outgoing_s;

    memcpy(*out_data, &entries_count, sizeof(uint32_t));

    size_t offset = sizeof(uint32_t);
    for (size_t i = 0; i < n; i++){
        size_t full_sz = sizeof(gossip_entry) + entries[i]->mtd_size;
        memcpy((*out_data) + offset, entries[i], full_sz);
        offset += full_sz;
    }
    return 0;
}

int gossip_entry_copy(gossip_entry **dest, gossip_entry *src){
    *dest = malloc(sizeof(gossip_entry) + src->mtd_size);
    if (!(*dest)) return -1;
    
    memcpy(*dest, src, sizeof(gossip_entry) + src->mtd_size);

    return 0;
}
