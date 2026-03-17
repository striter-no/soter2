#include <stdint.h>
#include <time.h>

#ifndef BASE_MULTITHR_TIME_H
#define BASE_MULTITHR_TIME_H

static inline int64_t mt_time_get_seconds(){
    struct timespec tv;
    timespec_get(&tv, TIME_UTC);
    return tv.tv_sec;
}

static inline int64_t mt_time_get_millis(){
    struct timespec tv;
    timespec_get(&tv, TIME_UTC);
    return tv.tv_sec * (int64_t)1'000 + tv.tv_nsec / 1'000'000;
}

static inline int64_t mt_time_get_micros(){
    struct timespec tv;
    timespec_get(&tv, TIME_UTC);
    return tv.tv_sec * (int64_t)1'000'000 + tv.tv_nsec / 1'000;
}

#endif