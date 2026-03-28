#include <gossip/system.h>
#include <multithr/time.h>
#include <stdint.h>

int gossip_system_init(gossip_system *g, uint32_t self_uid){
    if (!g) return -1;

    prot_array_create(sizeof(gossip_entry), &g->gossips);
    g->last_gossiped = 0;
    g->self_uid = self_uid;
    return 0;
}

int gossip_system_end(gossip_system *g){
    if (!g) return -1;

    prot_array_end(&g->gossips);
    return 0;
}


int gossip_cleanup(gossip_system *g){
    if (!g) return -1;

    int64_t curr_timestamp = mt_time_get_seconds();
    prot_array_lock(&g->gossips);
    for (size_t i = 0; i < g->gossips.array.len;){
        gossip_entry entr = *(gossip_entry*)_prot_array_at_unsafe(&g->gossips, i);
        if ((curr_timestamp - entr.timestamp) < GOSSIP_DEAD_DT) {
            i++;
            continue;
        }

        _prot_array_remove_unsafe(&g->gossips, i);
    }
    prot_array_unlock(&g->gossips);

    return 0;
}


int gossip_new_entry(gossip_system *g, const gossip_entry *entry){
    if (!g || !entry) return -1;
    if (entry->uid == g->self_uid) return 0;

    prot_array_lock(&g->gossips);

    gossip_entry existing;
    size_t       existing_inx = SIZE_MAX;
    for (size_t i = 0; i < g->gossips.array.len; i++) {
        gossip_entry tmp = *(gossip_entry*)_prot_array_at_unsafe(&g->gossips, i);
        if (tmp.uid == entry->uid) {
            existing = tmp;
            existing_inx = i;
            break;
        }
    }

    if (existing_inx != SIZE_MAX) {
        if (entry->version <= existing.version) {
            prot_array_unlock(&g->gossips);
            return 0;
        }

        _prot_array_remove_unsafe(&g->gossips, existing_inx);
    }

    gossip_entry copy;
    gossip_entry_copy(&copy, entry);

    _prot_array_push_unsafe(&g->gossips, &copy);
    prot_array_unlock(&g->gossips);

    return 0;
}


int gossip_random_entries(gossip_system *g, gossip_entry **entries, size_t *n_ptr){
    if (!g || !n_ptr) return -1;
    size_t n = *n_ptr;

    if (n == 0) return -1;

    prot_array_lock(&g->gossips);

    size_t len = g->gossips.array.len;
    if (len == 0) {
        prot_array_unlock(&g->gossips);
        return 1;
    }

    if (n > len) n = len;

    size_t *indices = malloc(len * sizeof(size_t));
    for (size_t i = 0; i < len; i++) indices[i] = i;

    for (size_t i = 0; i < n; i++) {
        size_t j = i + (rand() % (len - i));

        size_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    *entries = malloc(sizeof(gossip_entry) * n);
    for (size_t i = 0; i < n; i++) {
        gossip_entry *entr = _prot_array_at_unsafe(&g->gossips, indices[i]);
        gossip_entry_copy(&(*entries)[i], entr);
    }

    free(indices);
    *n_ptr = n;
    prot_array_unlock(&g->gossips);
    return 0;
}


int gossip_from_packet(gossip_system *g, protopack *pack_in){
    if (!g) return -1;

    uint8_t *data    = pack_in->data;
    size_t   data_sz = pack_in->d_size;

    if (!data || data_sz < sizeof(gossip_entry) + sizeof(uint32_t))
        return -1;

    uint32_t entries_n = 0;
    memcpy(&entries_n, data, sizeof(uint32_t));
    entries_n = ntohl(entries_n);

    data += sizeof(uint32_t);

    size_t offset = 0;
    for (size_t i = 0; i < entries_n; i++){
        gossip_entry header;
        memcpy(&header, &data[offset], sizeof(header));
        if (offset + sizeof(gossip_entry) > data_sz)
            return -1;

        gossip_new_entry(g, &header);
        offset += sizeof(gossip_entry);
    }

    return 0;
}


int gossip_to_data(gossip_system *g, gossip_entry **entries, size_t n, uint8_t **out_data, size_t *out_sz){
    if (!g || !entries || n == 0) return -1;

    uint32_t entries_count = (uint32_t)n;
    size_t outgoing_s = sizeof(uint32_t);

    for (size_t i = 0; i < n; i++)
        outgoing_s += sizeof(gossip_entry);

    *out_data = malloc(outgoing_s);
    if (!*out_data) return -1;
    if (out_sz) *out_sz = outgoing_s;

    memcpy(*out_data, &(uint32_t){htonl(entries_count)}, sizeof(uint32_t));

    size_t offset = sizeof(uint32_t);
    for (size_t i = 0; i < n; i++){
        memcpy((*out_data) + offset, entries[i], sizeof(gossip_entry));
        offset += sizeof(gossip_entry);
    }
    return 0;
}

int gossip_entry_copy(gossip_entry *dest, const gossip_entry *src){
    memcpy(dest, src, sizeof(gossip_entry));
    return 0;
}

gossip_entry gossip_create_entry(
    uint32_t uid,       // UID
    naddr_t *addr,

    gossip_metadata md,
    bool convert_hton
){
    gossip_entry out;
    out.version = 0;
    out.timestamp = mt_time_get_seconds();

    out.uid = uid;
    out.addr = convert_hton ? ln_hton(addr): *addr;
    out.md = md;
    return out;
}
