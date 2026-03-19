#include <base/prot_array.h>

int prot_array_create(size_t element_size, prot_array *arr){
    if (!arr) return -1;
    
    arr->array = dyn_array_create(element_size);
    
    pthread_mutexattr_t attrs;
    pthread_mutexattr_init(&attrs);
    // pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init(&arr->mtx, &attrs);
    pthread_mutexattr_destroy(&attrs);

    return 0;
}

void prot_array_lock(prot_array *array){
    if (!array){
        perror("prot lock on NULL array");
        abort();
    }
    pthread_mutex_lock(&array->mtx);
}

void prot_array_unlock(prot_array *array){
    if (!array) {
        perror("prot unlock on NULL array");
        abort();
    }
    pthread_mutex_unlock(&array->mtx);
}

size_t prot_array_len(prot_array *array){
    if (!array) return 0;
    pthread_mutex_lock(&array->mtx);
    size_t r = array->array.len;
    pthread_mutex_unlock(&array->mtx);
    return r;
}

int prot_array_push(prot_array *array, const void *element){
    if (!array) return -1;

    pthread_mutex_lock(&array->mtx);
    int r = dyn_array_push(&array->array, element);
    pthread_mutex_unlock(&array->mtx);
    return r;
}

size_t prot_array_index(prot_array *array, const void *element){
    if (!array) return SIZE_MAX;

    pthread_mutex_lock(&array->mtx);
    size_t r = dyn_array_index(&array->array, element);
    pthread_mutex_unlock(&array->mtx);
    return r;
}

void *prot_array_at(prot_array *array, size_t index){
    if (!array) return NULL;

    pthread_mutex_lock(&array->mtx);
    void *r = dyn_array_at(&array->array, index);
    pthread_mutex_unlock(&array->mtx);
    return r;
}

void prot_array_filter(prot_array *array, int (*ffunc)(size_t inx, void *elem, void *ctx), void *ctx){
    if (!array) return;

    pthread_mutex_lock(&array->mtx);
    
    size_t offset = 0;
    for (size_t i = 0; i < array->array.len; ){
        if (1 == ffunc(offset + i, dyn_array_at(&array->array, i), ctx)){
            dyn_array_remove(&array->array, i);
            offset++;
        } else i++;
    }
    
    pthread_mutex_unlock(&array->mtx);
}

int prot_array_remove(prot_array *array, size_t index){
    if (!array) return -1;

    pthread_mutex_lock(&array->mtx);
    int r = dyn_array_remove(&array->array, index);
    pthread_mutex_unlock(&array->mtx);
    return r;
}

int prot_array_count(prot_array *array, const void *element){
    if (!array) return -1;

    pthread_mutex_lock(&array->mtx);
    int r = dyn_array_count(&array->array, element);
    pthread_mutex_unlock(&array->mtx);
    return r;
}

void prot_array_setself(prot_array *array){
    if (!array) return;

    pthread_mutex_lock(&array->mtx);
    dyn_array_setself(&array->array);
    pthread_mutex_unlock(&array->mtx);
}

void prot_array_sort(prot_array *array, dyn_array_cmp_t cmp){
    if (!array || !cmp) return;
    
    pthread_mutex_lock(&array->mtx);
    dyn_array_sort(&array->array, cmp);
    pthread_mutex_unlock(&array->mtx);
}

void prot_array_end(prot_array *array){
    pthread_mutex_destroy(&array->mtx);
    dyn_array_end(&array->array);
}
