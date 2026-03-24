#include <pthread.h>
#include "dyn_table.h"

#ifndef PROT_BASE_TABLE

typedef struct {
    pthread_mutex_t mtx;
    dyn_table       table;
} prot_table;

int prot_table_create(size_t key_size, size_t val_size, dyn_own_t flags, prot_table *table);
void prot_table_lock(prot_table *table);
void prot_table_unlock(prot_table *table);
int prot_table_set(prot_table *table, const void *key, const void *val);
void* prot_table_get(prot_table *table, const void *key);
int prot_table_remove(prot_table *table, const void *key);
void prot_table_end(prot_table *table);
dyn_pair *prot_table_iterate(prot_table *table, size_t *inx);

static inline void* _prot_table_get_unsafe(prot_table *table, const void *key){
    return dyn_table_get(&table->table, key);
}

#endif
#define PROT_BASE_TABLE
