#include <assert.h>
#include <base/prot_queue.h>

int prot_queue_create(size_t element_size, prot_queue *queue) {
    if (!queue) return -1;
    
    int r = prot_array_create(element_size, &queue->arr);
    
    queue->arr.array.elements = calloc(1024, element_size);
    if (!queue->arr.array.elements) {
        prot_array_end(&queue->arr);
        return -1;
    }
    queue->arr.array.head = 1024;
    queue->arr.array.len = 1024;
    
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    return r;
}

void prot_queue_lock(prot_queue *queue){
    prot_array_lock(&queue->arr);
}

void prot_queue_unlock(prot_queue *queue){
    prot_array_unlock(&queue->arr);
}

static void _prot_queue_grow_unsafe(prot_queue *q) {
    size_t old_cap = q->arr.array.len;
    size_t new_cap = old_cap * 2;
    
    void *old_data = q->arr.array.elements;
    void *new_data = malloc(new_cap * q->arr.array.element_size);
    
    for (size_t i = 0; i < q->count; i++) {
        size_t old_idx = (q->head + i) % old_cap;
        memcpy((char*)new_data + (i * q->arr.array.element_size),
               (char*)old_data + (old_idx * q->arr.array.element_size),
               q->arr.array.element_size);
    }
    
    free(old_data);
    q->arr.array.elements = new_data;
    q->arr.array.len = new_cap;
    q->head = 0;
    q->tail = q->count;
    q->arr.array.head = new_cap;
}

int prot_queue_push(prot_queue *q, const void *element) {
    prot_array_lock(&q->arr);
    
    if (q->count == q->arr.array.len) {
        _prot_queue_grow_unsafe(q);
    }
    
    void *dest = (char*)q->arr.array.elements + (q->tail * q->arr.array.element_size);
    memcpy(dest, element, q->arr.array.element_size);
    
    assert(q->arr.array.len > 0);
    q->tail = (q->tail + 1) % q->arr.array.len;
    q->count++;
    
    prot_array_unlock(&q->arr);
    return 0;
}

int prot_queue_upush(prot_queue *q, const void *element){
    if (!q) return -1;
    
    prot_array_lock(&q->arr);
    
    for (size_t i = 0; i < q->count; i++){
        size_t idx = (q->head + i) % q->arr.array.len; 
        void *existing = (char*)q->arr.array.elements + (idx * q->arr.array.element_size);
        if (memcmp(element, existing, q->arr.array.element_size) == 0){
            prot_array_unlock(&q->arr);
            return 0;
        }
    }
    
    _prot_queue_push_unsafe(q, element);
    prot_array_unlock(&q->arr);
    return 0;
}


int prot_queue_pop(prot_queue *q, void *elem) {
    prot_array_lock(&q->arr);
    
    if (q->count == 0) {
        prot_array_unlock(&q->arr);
        return -1;
    }
    
    if (elem) {
        void *src = (char*)q->arr.array.elements + (q->head * q->arr.array.element_size);
        memcpy(elem, src, q->arr.array.element_size);
    }
    
    q->head = (q->head + 1) % q->arr.array.len;
    q->count--;
    
    prot_array_unlock(&q->arr);
    return 0;
}

size_t prot_queue_len(prot_queue *q){
    if (!q) return 0;
    pthread_mutex_lock(&q->arr.mtx);
    size_t i = q->count;
    pthread_mutex_unlock(&q->arr.mtx);
    return i;
}

int prot_queue_peek(prot_queue *q, void *out) {
    if (!q || !out || q->count == 0) return -1;
    pthread_mutex_lock(&q->arr.mtx);
    void *src = (char*)q->arr.array.elements + (q->head * q->arr.array.element_size);
    memcpy(out, src, q->arr.array.element_size);
    pthread_mutex_unlock(&q->arr.mtx);
    return 0;
}

void prot_queue_end(prot_queue *q){
    prot_array_end(&q->arr);
}

int _prot_queue_push_unsafe(prot_queue *q, const void *elem) {
    if (q->count == q->arr.array.len) {
        _prot_queue_grow_unsafe(q);
    }
    memcpy((char*)q->arr.array.elements + (q->tail * q->arr.array.element_size),
           elem, q->arr.array.element_size);
    q->tail = (q->tail + 1) % q->arr.array.len;
    q->count++;
    return 0;
}
