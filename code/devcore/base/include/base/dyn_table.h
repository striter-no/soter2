#include "dyn_array.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef BASE_TABLE

typedef enum {
    DYN_OWN_NONE  = 0,
    DYN_OWN_KEY   = 1,
    DYN_OWN_VAL   = 2,
    DYN_OWN_BOTH  = 3
} dyn_own_t;

typedef struct {
    void *first, *second;
} dyn_pair;

typedef struct {
    size_t    key_size, val_size;
    dyn_own_t flags;
    dyn_array array;
} dyn_table;

dyn_table dyn_table_create(size_t key_size, size_t val_size, dyn_own_t flags);

int   dyn_table_set(dyn_table *table, const void *key, const void *val);
int   dyn_table_remove(dyn_table *table, const void *key);
void *dyn_table_get(dyn_table *table, const void *key);
void  dyn_table_end(dyn_table *table);
dyn_pair *dyn_table_iterate(dyn_table *table, size_t *inx);

#endif
#define BASE_TABLE
