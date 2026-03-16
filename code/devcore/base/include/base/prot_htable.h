#include "dyn_htable.h"
#include <pthread.h>

#ifndef PROT_HASHTABLE_H

typedef struct {
    dyn_htable      htable;
    pthread_mutex_t mtx;
} prot_htable;

int prot_ht_setup(prot_htable* table,
				size_t key_size,
				size_t value_size,
				size_t capacity);

int prot_ht_copy(prot_htable* first, prot_htable* second);
int prot_ht_move(prot_htable* first, prot_htable* second);
int prot_ht_swap(prot_htable* first, prot_htable* second);

/* Destructor */
int prot_ht_destroy(prot_htable* table);

int prot_ht_insert(prot_htable* table, void* key, void* value);

int prot_ht_contains(prot_htable* table, void* key);
void* prot_ht_lookup(prot_htable* table, void* key);
const void* prot_ht_const_lookup(prot_htable* table, void* key);

int prot_ht_erase(prot_htable* table, void* key);
int prot_ht_clear(prot_htable* table);

int prot_ht_is_empty(prot_htable* table);
bool prot_ht_is_initialized(prot_htable* table);

int prot_ht_reserve(prot_htable* table, size_t minimum_capacity);

#define PROT_HASHTABLE_H
#endif