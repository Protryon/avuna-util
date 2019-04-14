/*
 * prqueue.c
 *
 *  Created on: Nov 19, 2015
 *      Author: root
 */

#include <avuna/prqueue.h>
#include <avuna/string.h>
#include <avuna/pmem.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

struct prqueue* prqueue_new(struct mempool* pool, size_t capacity, uint8_t multithreaded) {
	struct prqueue* prqueue = pmalloc(pool, sizeof(struct prqueue));
	prqueue->pool = pool;
	prqueue->capacity = capacity;
	prqueue->data = pmalloc(prqueue->pool, (capacity == 0 ? 17 : capacity + 1) * sizeof(struct __prqueue_entry));
	prqueue->real_capacity = capacity == 0 ? 16 : 0;
	prqueue->end = 0;
	prqueue->size = 0;
	prqueue->multithreaded = multithreaded;
	if (multithreaded) {
		pthread_mutex_init(&prqueue->data_mutex, NULL);
		phook(prqueue->pool, (void (*)(void*)) pthread_mutex_destroy, &prqueue->data_mutex);
		pthread_cond_init(&prqueue->out_cond, NULL);
		phook(prqueue->pool, (void (*)(void*)) pthread_cond_destroy, &prqueue->out_cond);
		pthread_cond_init(&prqueue->in_cond, NULL);
		phook(prqueue->pool, (void (*)(void*)) pthread_cond_destroy, &prqueue->in_cond);
	}
	return prqueue;
}

int prqueue_add(struct prqueue* prqueue, void* data, float priority) {
	if (prqueue->multithreaded) pthread_mutex_lock(&prqueue->data_mutex);
	if (prqueue->size == prqueue->real_capacity && prqueue->capacity == 0) {
		prqueue->real_capacity += 1024 / sizeof(struct __prqueue_entry);
		prqueue->data = prealloc(prqueue->pool, prqueue->data, prqueue->real_capacity * sizeof(struct __prqueue_entry));
	} else if (prqueue->capacity == 0) {
	} else {
		while (prqueue->size == prqueue->capacity) {
			if (!prqueue->multithreaded) return 1;
			pthread_cond_wait(&prqueue->in_cond, &prqueue->data_mutex);
		}
	}
	struct __prqueue_entry entry;
	entry.data = data;
	entry.priority = priority;
	size_t new_end = ++prqueue->end;
	for (; new_end > 1 && priority < prqueue->data[new_end / 2].priority; new_end /= 2)
		memcpy(prqueue->data + new_end, prqueue->data + (new_end / 2), sizeof(struct __prqueue_entry));
	memcpy(prqueue->data + new_end, &entry, sizeof(struct __prqueue_entry));
	size_t real_capacity = prqueue->capacity > 0 ? prqueue->capacity : prqueue->real_capacity;
	if (prqueue->end == real_capacity && prqueue->capacity == 0) {
		prqueue->real_capacity *= 2;
		prqueue->data = prealloc(prqueue->pool, prqueue->data, prqueue->real_capacity * sizeof(struct __prqueue_entry));
	}
	prqueue->size++;
	if (prqueue->multithreaded) {
		pthread_mutex_unlock(&prqueue->data_mutex);
		pthread_cond_signal(&prqueue->out_cond);
	}
	return 0;
}

void* prqueue_pop(struct prqueue* prqueue) {
	if (prqueue->multithreaded) {
		pthread_mutex_lock(&prqueue->data_mutex);
		while (prqueue->size == 0) {
			pthread_cond_wait(&prqueue->out_cond, &prqueue->data_mutex);
		}
	} else if (prqueue->size == 0) {
		return NULL;
	}
	struct __prqueue_entry entry = prqueue->data[1];
	if (prqueue->size == 1) prqueue->size = 0;
	else memcpy(prqueue->data + 1, prqueue->data + prqueue->size--, sizeof(struct __prqueue_entry));
	struct __prqueue_entry temp_entry;
	memcpy(&temp_entry, prqueue->data + 1, sizeof(struct __prqueue_entry));
	size_t i = 1;
	for (size_t c = 0; 2 * i <= prqueue->size; i = c) {
		c = 2 * i;
		if (c != prqueue->size && prqueue->data[c].priority > prqueue->data[c + 1].priority) {
			c++;
		}
		if (temp_entry.priority > prqueue->data[c].priority) {
			memcpy(prqueue->data + i, prqueue->data + c, sizeof(struct __prqueue_entry));
		}
		else break;
	}
	memcpy(prqueue->data + i, &temp_entry, sizeof(struct __prqueue_entry));
	if (prqueue->multithreaded) {
		pthread_mutex_unlock(&prqueue->data_mutex);
		pthread_cond_signal(&prqueue->in_cond);
	}
	return entry.data;
}

void* prqueue_pop_timeout(struct prqueue* prqueue, struct timespec* abstime) {
	if (prqueue->multithreaded) {
		pthread_mutex_lock(&prqueue->data_mutex);
		while (prqueue->size == 0) {
			int x = pthread_cond_timedwait(&prqueue->out_cond, &prqueue->data_mutex, abstime);
			if (x) {
				pthread_mutex_unlock(&prqueue->data_mutex);
				errno = x;
				return NULL;
			}
		}
	} else if (prqueue->size == 0) {
		return NULL;
	}
	struct __prqueue_entry entry = prqueue->data[1];
	if (prqueue->size == 1) prqueue->size = 0;
	else memcpy(prqueue->data + 1, prqueue->data + prqueue->size--, sizeof(struct __prqueue_entry));
	struct __prqueue_entry temp_entry;
	memcpy(&temp_entry, prqueue->data + 1, sizeof(struct __prqueue_entry));
	size_t i = 1;
	for (size_t c = 0; 2 * i <= prqueue->size; i = c) {
		c = 2 * i;
		if (c != prqueue->size && prqueue->data[c].priority > prqueue->data[c + 1].priority) {
			c++;
		}
		if (temp_entry.priority > prqueue->data[c].priority) {
			memcpy(prqueue->data + i, prqueue->data + c, sizeof(struct __prqueue_entry));
		}
		else break;
	}
	memcpy(prqueue->data + i, &temp_entry, sizeof(struct __prqueue_entry));
	if (prqueue->multithreaded) {
		pthread_mutex_unlock(&prqueue->data_mutex);
		pthread_cond_signal(&prqueue->in_cond);
	}
	return entry.data;
}
