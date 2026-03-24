#include <base/dyn_table.h>

dyn_table dyn_table_create(size_t key_size, size_t val_size, dyn_own_t flags) {
    dyn_table table;
    table.key_size = key_size;
    table.val_size = val_size;
    table.flags = flags;
    table.array = dyn_array_create(sizeof(dyn_pair));
    return table;
}

static size_t _dyn_table_find_idx(dyn_table *table, const void *key) {
    for (size_t i = 0; i < table->array.len; i++) {
        dyn_pair *pair = (dyn_pair *)dyn_array_at(&table->array, i);
        if (memcmp(pair->first, key, table->key_size) == 0) return i;
    }
    return SIZE_MAX;
}

int dyn_table_set(dyn_table *table, const void *key, const void *val) {
    if (!table || !key || !val) return -1;

    size_t idx = _dyn_table_find_idx(table, key);

    if (idx != SIZE_MAX) {
        dyn_pair *pair = (dyn_pair *)dyn_array_at(&table->array, idx);
        if (table->flags & DYN_OWN_VAL) {
            memcpy(pair->second, val, table->val_size);
        } else {
            pair->second = (void *)val;
        }
        return 0;
    }

    dyn_pair new_pair;

    if (table->flags & DYN_OWN_KEY) {
        new_pair.first = malloc(table->key_size);
        if (new_pair.first) memcpy(new_pair.first, key, table->key_size);
    } else {
        new_pair.first = (void *)key;
    }

    if (table->flags & DYN_OWN_VAL) {
        new_pair.second = malloc(table->val_size);
        if (new_pair.second) memcpy(new_pair.second, val, table->val_size);
    } else {
        new_pair.second = (void *)val;
    }

    if ((table->flags & DYN_OWN_KEY && !new_pair.first) ||
        (table->flags & DYN_OWN_VAL && !new_pair.second)) {
        if (table->flags & DYN_OWN_KEY) free(new_pair.first);
        if (table->flags & DYN_OWN_VAL) free(new_pair.second);
        return -1;
    }

    return dyn_array_push(&table->array, &new_pair);
}

int dyn_table_remove(dyn_table *table, const void *key) {
    size_t idx = _dyn_table_find_idx(table, key);
    if (idx == SIZE_MAX) return -1;

    dyn_pair *pair = (dyn_pair *)dyn_array_at(&table->array, idx);
    if (table->flags & DYN_OWN_KEY) free(pair->first);
    if (table->flags & DYN_OWN_VAL) free(pair->second);

    return dyn_array_remove(&table->array, idx);
}

void *dyn_table_get(dyn_table *table, const void *key){
    size_t idx = _dyn_table_find_idx(table, key);
    if (idx == SIZE_MAX) return NULL;

    return ((dyn_pair*)dyn_array_at(&table->array, idx))->second;
}

dyn_pair *dyn_table_iterate(dyn_table *table, size_t *_inx){
    if (!_inx || !table) return NULL;
    size_t inx = *_inx;

    if (inx >= table->array.len) return NULL;

    *_inx = inx + 1;
    return (dyn_pair *)dyn_array_at(&table->array, inx);
}

void dyn_table_end(dyn_table *table) {
    if (!table) return;

    if (table->flags != DYN_OWN_NONE) {
        for (size_t i = 0; i < table->array.len; i++) {
            dyn_pair *pair = (dyn_pair *)dyn_array_at(&table->array, i);
            if (table->flags & DYN_OWN_KEY) free(pair->first);
            if (table->flags & DYN_OWN_VAL) free(pair->second);
        }
    }

    dyn_array_end(&table->array);
}
