#include "prot_array.h"

#ifndef BASE_PROT_QUEUE

typedef struct {
    prot_array arr;
} prot_queue;

int prot_queue_create(size_t element_size, prot_queue *queue);
void prot_queue_lock(prot_queue *queue);
void prot_queue_unlock(prot_queue *queue);
int prot_queue_push(prot_queue *q, const void *element);
int prot_queue_upush(prot_queue *q, const void *element);
int prot_queue_rpush(prot_queue *q, const void *element);
int prot_queue_pop(prot_queue *q, void *elem);
size_t prot_queue_len(prot_queue *q);
void* prot_queue_peek(prot_queue *q);
void prot_queue_end(prot_queue *q);

#endif
#define BASE_PROT_QUEUE