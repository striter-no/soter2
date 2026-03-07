#include <pthread.h>
#include "dyn_table.h"

#ifndef PROT_BASE_TABLE

typedef struct {
    pthread_mutex_t mtx;
    dyn_table       table;
} prot_table;

prot_table prot_table_create(size_t key_size, size_t val_size, dyn_own_t flags);
void prot_table_lock(prot_table *table);
void prot_table_unlock(prot_table *table);
int prot_table_set(prot_table *table, const void *key, const void *val);
void* prot_table_get(prot_table *table, const void *key);
int prot_table_remove(prot_table *table, const void *key);
void prot_table_end(prot_table *table);
#endif
#define PROT_BASE_TABLE