#ifndef __HASH_H__
#define __HASH_H__

#include <avuna/pmem.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

struct hashmap_bucket_entry {
    uint64_t umod_hash;
    char* key;
    void* data;
    struct hashmap_bucket_entry* next;
};

struct hashset_bucket_entry {
    uint64_t umod_hash;
    char* key;
    struct hashset_bucket_entry* next;
};

struct hashmap {
    size_t entry_count;
    size_t bucket_count;
    struct hashmap_bucket_entry** buckets;
    struct mempool* pool;
    uint8_t multithreaded;
    pthread_rwlock_t rwlock;
};

// WARNING: ITER_MAP does not synchronize multithreaded hashmaps/sets, it is up to the consumer to do so to prevent accidental dead locks due to exiting control flow.

#define ITER_MAP(map) {for (size_t bucket_i = 0; bucket_i < map->bucket_count; bucket_i++) { for (struct hashmap_bucket_entry* bucket_entry = map->buckets[bucket_i]; bucket_entry != NULL; bucket_entry = bucket_entry->next) { char* str_key = bucket_entry->key; void* ptr_key = (void*)bucket_entry->key; void* value = bucket_entry->data;

#define ITER_MAPR(map, value) {for (size_t bucket_i_value = 0; bucket_i_value < map->bucket_count; bucket_i_value++) { for (struct hashmap_bucket_entry* bucket_entry_value = map->buckets[bucket_i_value]; bucket_entry_value != NULL; bucket_entry_value = bucket_entry_value->next) { char* str_key_value = bucket_entry->key; void* ptr_key_value = (void*)bucket_entry->key; void* value = bucket_entry->data;

#define ITER_MAP_END() }}}

struct hashset {
    size_t entry_count;
    size_t bucket_count;
    struct hashset_bucket_entry** buckets;
    struct mempool* pool;
    uint8_t multithreaded;
    pthread_rwlock_t rwlock;
};

#define ITER_SET(set) {for (size_t bucket_i = 0; bucket_i < set->bucket_count; bucket_i++) { for (struct hashset_bucket_entry* bucket_entry = set->buckets[bucket_i]; bucket_entry != NULL; bucket_entry = bucket_entry->next) { char* str_key = bucket_entry->key; void* ptr_key = (void*)bucket_entry->key;

#define ITER_SET_END() }}}

struct hashmap* hashmap_new(size_t init_cap, struct mempool* pool);

struct hashmap* hashmap_thread_new(size_t init_cap, struct mempool* pool);

struct hashset* hashset_new(size_t init_cap, struct mempool* pool);

struct hashset* hashset_thread_new(size_t init_cap, struct mempool* pool);

void hashmap_free(struct hashmap* map);

void hashset_free(struct hashset* set);

void* hashmap_get(struct hashmap* map, char* key);

void* hashmap_getptr(struct hashmap* map, void* key);

void* hashmap_getint(struct hashmap* map, uint64_t key);

int hashset_has(struct hashset* set, char* key);

int hashset_hasptr(struct hashset* set, void* key);

int hashset_hasint(struct hashset* set, uint64_t key);

void hashmap_put(struct hashmap* map, char* key, void* data);

void hashmap_putptr(struct hashmap* map, void* key, void* data);

void hashmap_putint(struct hashmap* map, uint64_t key, void* data);

void hashset_add(struct hashset* set, char* key);

void hashset_addptr(struct hashset* set, void* key);

void hashset_addint(struct hashset* set, uint64_t key);

void hashset_rem(struct hashset* set, char* key);

void hashset_remptr(struct hashset* set, void* key);

void hashset_remint(struct hashset* set, uint64_t key);

struct hashmap* hashmap_clone(struct hashmap* map, struct mempool* pool);

#endif