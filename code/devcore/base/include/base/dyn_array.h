#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef BASE_ARRAY

typedef struct {
    void   *elements;
    size_t  len;
    size_t  head;
    size_t  element_size;
} dyn_array;

dyn_array dyn_array_create(size_t element_size);
int       dyn_array_from_c(dyn_array *out, size_t element_size, size_t len, const void *elements);

size_t dyn_array_index(dyn_array *array, const void *element);

int   dyn_array_resize(dyn_array *array, size_t s);
int   dyn_array_push(dyn_array *array, const void *element);
int   dyn_array_remove(dyn_array *array, size_t index);
int   dyn_array_count(dyn_array *array, const void *element);
void *dyn_array_at(dyn_array *array, size_t index);
void  dyn_array_setself(dyn_array *array);
int   dyn_array_insert(dyn_array *array, size_t index, const void *element);


typedef int (*dyn_array_cmp_t)(const void *a, const void *b);

void dyn_array_sort(dyn_array *array, dyn_array_cmp_t cmp);
void dyn_array_end(dyn_array *array);

#endif
#define BASE_ARRAY
