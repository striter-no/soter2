#include "dyn_array.h"

#ifndef BASE_QUEUE

typedef struct {
    dyn_array arr;
} dyn_queue;

dyn_queue dyn_queue_create(size_t element_size);
int   dyn_queue_push(dyn_queue *q, const void *element);
int   dyn_queue_pop(dyn_queue *q, void *elem);
void* dyn_queue_peek(dyn_queue *q);
void  dyn_queue_end(dyn_queue *q);

#endif
#define BASE_QUEUE