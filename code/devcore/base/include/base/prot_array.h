#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include "dyn_array.h"

#ifndef BASE_PROT_ARRAY

typedef struct {
    dyn_array       array;
    pthread_mutex_t mtx;
} prot_array;

prot_array prot_array_create(size_t element_size);
void prot_array_lock(prot_array *array);
void prot_array_unlock(prot_array *array);

int prot_array_push(prot_array *array, const void *element);
size_t prot_array_index(prot_array *array, const void *element);

void *prot_array_at(prot_array *array, size_t index);

int prot_array_remove(prot_array *array, size_t index);
int prot_array_count(prot_array *array, const void *element);
void prot_array_setself(prot_array *array);
void prot_array_sort(prot_array *array, dyn_array_cmp_t cmp);
void prot_array_end(prot_array *array);

#endif
#define BASE_PROT_ARRAY