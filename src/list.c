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
#include <string.h>

struct list* list_new(size_t initial_capacity, struct mempool* pool) {
    struct list* list = pcalloc(pool, sizeof(struct list));
    list->capacity = initial_capacity < 8 ? 8 : initial_capacity;
    list->data = pmalloc(pool, list->capacity * sizeof(void*));
    list->pool = pool;
    return list;
}

struct list* list_thread_new(size_t initial_capacity, struct mempool* pool) {
    struct list* list = pcalloc(pool, sizeof(struct list));
    list->capacity = initial_capacity < 8 ? 8 : initial_capacity;
    list->data = pmalloc(pool, list->capacity * sizeof(void*));
    list->pool = pool;
    list->multithreaded = 1;
    pthread_rwlock_init(&list->rwlock, NULL);
    phook(list->pool, (void (*)(void*)) pthread_rwlock_destroy, &list->rwlock);
    return list;
}

// no locks
void _list_ensure_capacity(struct list* list, size_t size) {
    size_t new_capacity = list->capacity;
    while (list->size < size) {
        new_capacity *= 2;
    }
    if (new_capacity != list->capacity) {
        size_t old_capacity = list->capacity;
        list->capacity = new_capacity;
        list->data = prealloc(list->pool, list->data, list->capacity * sizeof(void*));
        memset(list->data + old_capacity, 0, new_capacity - old_capacity);
    }
}

void list_append(struct list* list, void* data) {
    if (list->multithreaded) {
        pthread_rwlock_rdlock(&list->rwlock);
    }
    if (list->size == list->capacity) {
        list->capacity *= 2;
        list->data = prealloc(list->pool, list->data, list->capacity * sizeof(void*));
    }
    list->data[list->size++] = data;
    list->count++;
    if (list->multithreaded) {
        pthread_rwlock_unlock(&list->rwlock);
    }
}

void list_set(struct list* list, size_t index, void* data) {
    if (list->multithreaded) {
        pthread_rwlock_rdlock(&list->rwlock);
    }
    _list_ensure_capacity(list, index + 1);
    list->data[index] = data;
    if (list->count < index + 1) {
        list->count = index + 1;
    }
    if (list->multithreaded) {
        pthread_rwlock_unlock(&list->rwlock);
    }
}

void list_ensure_capacity(struct list* list, size_t size) {
    if (list->multithreaded) {
        pthread_rwlock_rdlock(&list->rwlock);
    }
    _list_ensure_capacity(list, size);
    if (list->multithreaded) {
        pthread_rwlock_unlock(&list->rwlock);
    }
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

