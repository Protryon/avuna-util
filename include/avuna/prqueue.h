//
// Created by p on 4/14/19.
//

#ifndef AVUNA_UTIL_PRQUEUE_H
#define AVUNA_UTIL_PRQUEUE_H

#include <avuna/pmem.h>
#include <pthread.h>
#include <time.h>

struct __prqueue_entry {
    float priority;
    void* data;
};

struct prqueue {
    struct mempool* pool;
    size_t size;
    size_t capacity;
    size_t end;
    size_t real_capacity;
    struct __prqueue_entry* data;
    uint8_t multithreaded;
    pthread_mutex_t data_mutex;
    pthread_cond_t in_cond;
    pthread_cond_t out_cond;
};

struct prqueue* prqueue_new(struct mempool* pool, size_t capacity, uint8_t multithreaded);

int prqueue_add(struct prqueue* prqueue, void* data, float priority);

void* prqueue_pop(struct prqueue* prqueue);

void* prqueue_pop_timeout(struct prqueue* prqueue, struct timespec* abstime);

#endif //AVUNA_UTIL_PRQUEUE_H
