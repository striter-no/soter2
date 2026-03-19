#define _POSIX_C_SOURCE 200809L
#include <netinet/in.h>
#include <stdint.h>
#include <time.h>

#ifndef BASE_MULTITHR_TIME_H
#define BASE_MULTITHR_TIME_H

static inline int64_t mt_time_get_seconds(void){
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return tv.tv_sec;
}

static inline int64_t mt_time_get_millis(void){
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return tv.tv_sec * (int64_t)1000 + tv.tv_nsec / 1000000;
}

static inline int64_t mt_time_get_micros(void){
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return tv.tv_sec * (int64_t)1000000 + tv.tv_nsec / 1000;
}

static inline int64_t mt_time_get_seconds_monocoarse(void){
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &tv);
    return tv.tv_sec;
}

static inline int64_t mt_time_get_millis_monocoarse(void){
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &tv);
    return tv.tv_sec * (int64_t)1000 + tv.tv_nsec / 1000000;
}

static inline int64_t mt_time_get_micros_monocoarse(void){
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &tv);
    return tv.tv_sec * (int64_t)1000000 + tv.tv_nsec / 1000;
}


static inline uint64_t htonll(uint64_t x) {
    return htonl(1) == 1 ? x : 
           ((uint64_t)htonl((uint32_t)(x >> 32)) | 
           ((uint64_t)htonl((uint32_t)x) << 32));
}

static inline uint64_t ntohll(uint64_t x) {
    return htonll(x);
}

#endif