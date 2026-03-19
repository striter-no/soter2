#include "base/dyn_htable.h"
#include <base/prot_htable.h>

int prot_ht_setup(prot_htable* table,
				size_t key_size,
				size_t value_size,
				size_t capacity)
{
    if (!table) return -1;
    pthread_mutexattr_t attrs;
    pthread_mutexattr_init(&attrs);
    // pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init(&table->mtx, &attrs);
    pthread_mutexattr_destroy(&attrs);

    return ht_setup(&table->htable, key_size, value_size, capacity);
}

int prot_ht_copy(prot_htable* first, prot_htable* second){
    if (!first || !second) return -1;
    pthread_mutex_lock(&first->mtx);
    pthread_mutex_lock(&second->mtx);
    int r = ht_copy(&first->htable, &second->htable);
    pthread_mutex_unlock(&second->mtx);
    pthread_mutex_unlock(&first->mtx);
    return r;
}
int prot_ht_move(prot_htable* first, prot_htable* second){
    if (!first || !second) return -1;
    pthread_mutex_lock(&first->mtx);
    pthread_mutex_lock(&second->mtx);
    int r = ht_move(&first->htable, &second->htable);
    pthread_mutex_unlock(&second->mtx);
    pthread_mutex_unlock(&first->mtx);
    return r;
}
int prot_ht_swap(prot_htable* first, prot_htable* second){
    if (!first || !second) return -1;
    pthread_mutex_lock(&first->mtx);
    pthread_mutex_lock(&second->mtx);
    int r = ht_swap(&first->htable, &second->htable);
    pthread_mutex_unlock(&second->mtx);
    pthread_mutex_unlock(&first->mtx);
    return r;
}

/* Destructor */
int prot_ht_destroy(prot_htable* table){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_destroy(&table->htable);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

int prot_ht_insert(prot_htable* table, void* key, void* value){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_insert(&table->htable, key, value);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

int prot_ht_contains(prot_htable* table, void* key){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_contains(&table->htable, key);
    pthread_mutex_unlock(&table->mtx);
    return r;
}
void* prot_ht_lookup(prot_htable* table, void* key){
    if (!table) return NULL;
    pthread_mutex_lock(&table->mtx);
    void* r = ht_lookup(&table->htable, key);
    pthread_mutex_unlock(&table->mtx);
    return r;
}
const void* prot_ht_const_lookup(prot_htable* table, void* key){
    if (!table) return NULL;
    pthread_mutex_lock(&table->mtx);
    const void* r = ht_const_lookup(&table->htable, key);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

int prot_ht_erase(prot_htable* table, void* key){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_erase(&table->htable, key);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

int prot_ht_clear(prot_htable* table){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_clear(&table->htable);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

int prot_ht_is_empty(prot_htable* table){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_is_empty(&table->htable);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

bool prot_ht_is_initialized(prot_htable* table){
    if (!table) return false;
    pthread_mutex_lock(&table->mtx);
    bool r = ht_is_initialized(&table->htable);
    pthread_mutex_unlock(&table->mtx);
    return r;
}

int prot_ht_reserve(prot_htable* table, size_t minimum_capacity){
    if (!table) return -1;
    pthread_mutex_lock(&table->mtx);
    int r = ht_reserve(&table->htable, minimum_capacity);
    pthread_mutex_unlock(&table->mtx);
    return r;
}