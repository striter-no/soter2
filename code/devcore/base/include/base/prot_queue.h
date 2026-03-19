#include "prot_array.h"

#ifndef BASE_PROT_QUEUE

typedef struct {
    prot_array arr;
    size_t     head;
    size_t     tail;
    size_t     count;
} prot_queue;

int prot_queue_create(size_t element_size, prot_queue *queue);
void prot_queue_lock(prot_queue *queue);
void prot_queue_unlock(prot_queue *queue);
int prot_queue_push(prot_queue *q, const void *element);
int prot_queue_pop(prot_queue *q, void *elem);
size_t prot_queue_len(prot_queue *q);
int prot_queue_peek(prot_queue *q, void *out);
int prot_queue_upush(prot_queue *q, const void *element);
void prot_queue_end(prot_queue *q);

static inline size_t _prot_queue_len_unsafe(prot_queue *q) {
    return q->count;
}

static inline int _prot_queue_pop_unsafe(prot_queue *q, void *elem) {
    if (q->count == 0) return -1;
    if (elem) {
        memcpy(elem, 
               (char*)q->arr.array.elements + (q->head * q->arr.array.element_size),
               q->arr.array.element_size);
    }
    q->head = (q->head + 1) % q->arr.array.len;
    q->count--;
    return 0;
}

int _prot_queue_push_unsafe(prot_queue *q, const void *elem);

#endif
#define BASE_PROT_QUEUE