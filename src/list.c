/*
 * queue.c
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#include <avuna/list.h>
#include <avuna/pmem.h>
#include <errno.h>
#include <stdlib.h>

struct list* list_new(size_t initial_capacity, struct mempool* pool) {
    struct list* list = pcalloc(pool, sizeof(struct list));
    list->capacity = initial_capacity < 8 ? 8 : initial_capacity;
    list->data = pmalloc(pool, initial_capacity * sizeof(void*));
    list->pool = pool;
    return list;
}

struct list* list_thread_new(size_t initial_capacity, struct mempool* pool) {
    struct list* list = pcalloc(pool, sizeof(struct list));
    list->capacity = initial_capacity < 8 ? 8 : initial_capacity;
    list->data = pmalloc(pool, initial_capacity * sizeof(void*));
    list->pool = pool;
    list->multithreaded = 1;
    pthread_rwlock_init(&list->rwlock, NULL);
    phook(list->pool, (void (*)(void*)) pthread_rwlock_destroy, &list->rwlock);
    return list;
}

int list_append(struct list* list, void* data) {
    if (list->multithreaded) {
        pthread_rwlock_rdlock(&list->rwlock);
    }
    for (int i = 0; i < list->size; i++) {
        if (list->data[i] == NULL) {
            list->count++;
            list->data[i] = data;
            if (list->multithreaded) {
                pthread_rwlock_unlock(&list->rwlock);
            }
            return 0;
        }
    }
    if (list->size == list->capacity) {
        list->capacity *= 2;
        list->data = prealloc(list->pool, list->data, list->capacity * sizeof(void*));
    } else if (list->capacity > 0 && list->size == list->capacity) {
        errno = EINVAL;
        if (list->multithreaded) {
            pthread_rwlock_unlock(&list->rwlock);
        }
        return -1;
    }
    list->data[list->size++] = data;
    list->count++;
    if (list->multithreaded) {
        pthread_rwlock_unlock(&list->rwlock);
    }
    return 0;
}

int list_find_remove(struct list* list, void* data) {
    if (list->multithreaded) {
        pthread_rwlock_rdlock(&list->rwlock);
    }
    for (int i = 0; i < list->size; i++) {
        if (list->data[i] == data) {
            list->data[i] = NULL;
            list->count--;
            if (list->multithreaded) {
                pthread_rwlock_unlock(&list->rwlock);
            }
            return 0;
        }
    }
    if (list->multithreaded) {
        pthread_rwlock_unlock(&list->rwlock);
    }
    return -1;
}

