
#include <avuna/hash.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

uint64_t hashmap_hash(char* key) {
    if (key == NULL) return 0;
    size_t kl = strlen(key);
    size_t i = 0;
    uint64_t hash = 0x8888888888888888;
    for (; i + 8 < kl; i += 8) {
        uint64_t v = *(uint64_t*) &key[i];
        hash = hash ^ v;
    }
    if (kl >= 8) {
        uint64_t v = *(uint64_t*) &key[kl - 8];
        hash = hash ^ v;
    } else {
        for (; i < kl; i++) {
            uint8_t v = (uint8_t) key[i];
            hash = hash ^ (v << (8 * i));
        }
    }
    hash = hash ^ kl;

    return hash;
}

uint64_t hashmap_hash_mod(uint64_t hash, size_t size) {
    // a loop would get marginally better hashes, but be a bit slower
    if (size <= 0xFFFFFFFF) {
        hash = (hash >> 32) ^ (hash & 0xFFFFFFFF);
    }
    if (size <= 0xFFFF) {
        hash = (hash >> 16) ^ (hash & 0xFFFF);
    }
    if (size <= 0xFF) {
        hash = (hash >> 8) ^ (hash & 0xFF);
    }
    return hash % size;
}

void hashset_fixcap(struct hashset* set);

void hashmap_fixcap(struct hashmap* map);

struct hashmap* hashmap_new(size_t init_cap, struct mempool* pool) {
    struct hashmap* map = pcalloc(pool, sizeof(struct hashmap));
    map->bucket_count = init_cap;
    map->buckets = pcalloc(pool, sizeof(struct hashmap_bucket_entry*) * map->bucket_count);
    map->pool = pool;
    return map;
}

struct hashmap* hashmap_thread_new(size_t init_cap, struct mempool* pool) {
    struct hashmap* map = hashmap_new(init_cap, pool);
    map->multithreaded = 1;
    pthread_rwlock_init(&map->rwlock, NULL);
    if (map->pool != NULL) {
        phook(map->pool, (void (*)(void*)) pthread_rwlock_destroy, &map->rwlock);
    }
    return map;
}

struct hashset* hashset_new(size_t init_cap, struct mempool* pool) {
    struct hashset* set = pcalloc(pool, sizeof(struct hashset));
    set->bucket_count = init_cap;
    set->buckets = pcalloc(pool, sizeof(struct hashset_bucket_entry*) * set->bucket_count);
    set->pool = pool;
    return set;
}

struct hashset* hashset_thread_new(size_t init_cap, struct mempool* pool) {
    struct hashset* set = hashset_new(init_cap, pool);
    set->multithreaded = 1;
    pthread_rwlock_init(&set->rwlock, NULL);
    if (set->pool != NULL) {
        phook(set->pool, (void (*)(void*)) pthread_rwlock_destroy, &set->rwlock);
    }
    return set;
}

void hashmap_free(struct hashmap* map) {
    hashmap_free2(map, free);
}

void hashset_free(struct hashset* set) {
    hashset_free2(set, free);
}


void hashmap_free2(struct hashmap* map, void (*mfree)(void*)) {
    if (map->pool != NULL) {
        return;
    }
    if (map->multithreaded) {
        pthread_rwlock_wrlock(&map->rwlock);
    }
    for (size_t i = 0; i < map->bucket_count; i++) {
        for (struct hashmap_bucket_entry* bucket = map->buckets[i]; bucket != NULL;) {
            struct hashmap_bucket_entry* next = bucket->next;
            mfree(bucket);
            bucket = next;
        }
    }
    mfree(map->buckets);
    if (map->multithreaded) {
        map->buckets = NULL;
        pthread_rwlock_unlock(&map->rwlock);
        pthread_rwlock_destroy(&map->rwlock);
    }
    mfree(map);
}

void hashset_free2(struct hashset* set, void (*mfree)(void*)) {
    if (set->pool != NULL) {
        return;
    }
    if (set->multithreaded) {
        pthread_rwlock_wrlock(&set->rwlock);
    }
    for (size_t i = 0; i < set->bucket_count; i++) {
        for (struct hashset_bucket_entry* bucket = set->buckets[i]; bucket != NULL;) {
            struct hashset_bucket_entry* next = bucket->next;
            mfree(bucket);
            bucket = next;
        }
    }
    mfree(set->buckets);
    if (set->multithreaded) {
        set->buckets = NULL;
        pthread_rwlock_unlock(&set->rwlock);
        pthread_rwlock_destroy(&set->rwlock);
    }
    mfree(set);
}

void* hashmap_get(struct hashmap* map, char* key) {
    uint64_t hashum = hashmap_hash(key);
    if (map->multithreaded) {
        pthread_rwlock_rdlock(&map->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(hashum, map->bucket_count);
    for (struct hashmap_bucket_entry* bucket = map->buckets[hash]; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == hashum && (key == bucket->key || (key != NULL && strcmp(bucket->key, key) == 0))) {
            if (map->multithreaded) {
                pthread_rwlock_unlock(&map->rwlock);
            }
            return bucket->data;
        }
    }
    if (map->multithreaded) {
        pthread_rwlock_unlock(&map->rwlock);
    }
    return NULL;
}

void* hashmap_getptr(struct hashmap* map, void* key) {
    return hashmap_getint(map, (uint64_t) key >> 2);
}


void* hashmap_getint(struct hashmap* map, uint64_t key) {
    if (map->multithreaded) {
        pthread_rwlock_rdlock(&map->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(key, map->bucket_count);
    for (struct hashmap_bucket_entry* bucket = map->buckets[hash]; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == key && bucket->key == NULL) {
            if (map->multithreaded) {
                pthread_rwlock_unlock(&map->rwlock);
            }
            return bucket->data;
        }
    }
    if (map->multithreaded) {
        pthread_rwlock_unlock(&map->rwlock);
    }
    return NULL;
}

int hashset_has(struct hashset* set, char* key) {
    uint64_t hashum = hashmap_hash(key);
    if (set->multithreaded) {
        pthread_rwlock_rdlock(&set->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(hashum, set->bucket_count);
    for (struct hashset_bucket_entry* bucket = set->buckets[hash]; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == hashum && strcmp(bucket->key, key) == 0) {
            if (set->multithreaded) {
                pthread_rwlock_unlock(&set->rwlock);
            }
            return 1;
        }
    }
    if (set->multithreaded) {
        pthread_rwlock_unlock(&set->rwlock);
    }
    return 0;
}

int hashset_hasptr(struct hashset* set, void* key) {
    return hashset_hasint(set, (uint64_t) key >> 2);
}

int hashset_hasint(struct hashset* set, uint64_t key) {
    if (set->multithreaded) {
        pthread_rwlock_rdlock(&set->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(key, set->bucket_count);
    for (struct hashset_bucket_entry* bucket = set->buckets[hash]; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == key && bucket->key == NULL) {
            if (set->multithreaded) {
                pthread_rwlock_unlock(&set->rwlock);
            }
            return 1;
        }
    }
    if (set->multithreaded) {
        pthread_rwlock_unlock(&set->rwlock);
    }
    return 0;
}

void hashmap_put(struct hashmap* map, char* key, void* data) {
    uint64_t hashum = hashmap_hash(key);
    if (map->multithreaded) {
        pthread_rwlock_wrlock(&map->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(hashum, map->bucket_count);
    struct hashmap_bucket_entry* bucket = map->buckets[hash];
    if (bucket == NULL) {
        bucket = pmalloc(map->pool, sizeof(struct hashmap_bucket_entry));
        bucket->umod_hash = hashum;
        bucket->next = NULL;
        bucket->key = key;
        bucket->data = data;
        map->buckets[hash] = bucket;
        map->entry_count++;
        goto putret;
    }
    for (; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == hashum && (key == bucket->key || (key != NULL && strcmp(bucket->key, key) == 0))) {
            bucket->data = data;
            break;
        } else if (bucket->next == NULL) {
            struct hashmap_bucket_entry* bucketc = pmalloc(map->pool, sizeof(struct hashmap_bucket_entry));
            bucketc->umod_hash = hashum;
            bucketc->next = NULL;
            bucketc->key = key;
            bucketc->data = data;
            bucket->next = bucketc;
            map->entry_count++;
            break;
        }
    }
    putret:;
    hashmap_fixcap(map);
    if (map->multithreaded) {
        pthread_rwlock_unlock(&map->rwlock);
    }
}

void hashmap_putptr(struct hashmap* map, void* key, void* data) {
    return hashmap_putint(map, (uint64_t) key >> 2, data);
}


void hashmap_putint(struct hashmap* map, uint64_t key, void* data) {
    if (map->multithreaded) {
        pthread_rwlock_wrlock(&map->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(key, map->bucket_count);
    struct hashmap_bucket_entry* bucket = map->buckets[hash];
    if (bucket == NULL) {
        bucket = pmalloc(map->pool, sizeof(struct hashmap_bucket_entry));
        bucket->umod_hash = key;
        bucket->next = NULL;
        bucket->key = NULL;
        bucket->data = data;
        map->buckets[hash] = bucket;
        map->entry_count++;
        goto putret;
    }
    for (; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == key && bucket->key == NULL) {
            bucket->data = data;
            break;
        } else if (bucket->next == NULL) {
            struct hashmap_bucket_entry* bucketc = pmalloc(map->pool, sizeof(struct hashmap_bucket_entry));
            bucketc->umod_hash = key;
            bucketc->next = NULL;
            bucketc->key = NULL;
            bucketc->data = data;
            bucket->next = bucketc;
            map->entry_count++;
            break;
        }
    }
    putret:;
    hashmap_fixcap(map);
    if (map->multithreaded) {
        pthread_rwlock_unlock(&map->rwlock);
    }
}

// precondition: map is locked
void hashmap_fixcap(struct hashmap* map) {
    if ((map->entry_count / map->bucket_count) > 4) {
        size_t new_bucket_count = map->bucket_count * 2;
        map->buckets = prealloc(map->pool, map->buckets,
                                    new_bucket_count * sizeof(struct hashmap_bucket_entry*));
        memset((void*) map->buckets + (map->bucket_count * sizeof(struct hashmap_bucket_entry*)), 0,
               map->bucket_count * sizeof(struct hashmap_bucket_entry*));
        for (size_t i = 0; i < map->bucket_count; i++) {
            struct hashmap_bucket_entry* lbucket = NULL;
            for (struct hashmap_bucket_entry* bucket = map->buckets[i]; bucket != NULL;) {
                size_t ni = hashmap_hash_mod(bucket->umod_hash, new_bucket_count);
                if (ni == i) {
                    lbucket = bucket;
                    bucket = bucket->next;
                    continue;
                } else {
                    struct hashmap_bucket_entry* nbucket = bucket->next;
                    if (lbucket == NULL) {
                        map->buckets[i] = nbucket;
                    } else {
                        lbucket->next = nbucket;
                    }
                    if (map->buckets[ni] == NULL) {
                        map->buckets[ni] = bucket;
                        bucket->next = NULL;
                    } else {
                        bucket->next = map->buckets[ni];
                        map->buckets[ni] = bucket;
                    }
                    bucket = nbucket;
                    // no lbucket change
                }
            }
        }
        map->bucket_count = new_bucket_count;
    }
}

void hashset_add(struct hashset* set, char* key) {
    uint64_t hashum = hashmap_hash(key);
    if (set->multithreaded) {
        pthread_rwlock_wrlock(&set->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(hashum, set->bucket_count);
    struct hashset_bucket_entry* bucket = set->buckets[hash];
    if (bucket == NULL) {
        bucket = pmalloc(set->pool, sizeof(struct hashset_bucket_entry));
        bucket->umod_hash = hashum;
        bucket->next = NULL;
        bucket->key = key;
        set->buckets[hash] = bucket;
        set->entry_count++;
        goto putret;
    }
    for (; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == hashum && strcmp(bucket->key, key) == 0) {
            break;
        } else if (bucket->next == NULL) {
            struct hashset_bucket_entry* bucketc = pmalloc(set->pool, sizeof(struct hashset_bucket_entry));
            bucketc->umod_hash = hashum;
            bucketc->next = NULL;
            bucketc->key = key;
            bucket->next = bucketc;
            set->entry_count++;
            break;
        }
    }
    putret:;
    hashset_fixcap(set);
    if (set->multithreaded) {
        pthread_rwlock_unlock(&set->rwlock);
    }
}

void hashset_addptr(struct hashset* set, void* key) {
    hashset_addint(set, (uint64_t) key >> 2);
}


void hashset_addint(struct hashset* set, uint64_t key) {
    if (set->multithreaded) {
        pthread_rwlock_wrlock(&set->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(key, set->bucket_count);
    struct hashset_bucket_entry* bucket = set->buckets[hash];
    if (bucket == NULL) {
        bucket = pmalloc(set->pool, sizeof(struct hashset_bucket_entry));
        bucket->umod_hash = key;
        bucket->next = NULL;
        bucket->key = NULL;
        set->buckets[hash] = bucket;
        set->entry_count++;
        goto putret;
    }
    for (; bucket != NULL; bucket = bucket->next) {
        if (bucket->umod_hash == key) {
            break;
        } else if (bucket->next == NULL) {
            struct hashset_bucket_entry* bucketc = pmalloc(set->pool, sizeof(struct hashset_bucket_entry));
            bucketc->umod_hash = key;
            bucketc->next = NULL;
            bucketc->key = NULL;
            bucket->next = bucketc;
            set->entry_count++;
            break;
        }
    }
    putret:;
    hashset_fixcap(set);
    if (set->multithreaded) {
        pthread_rwlock_unlock(&set->rwlock);
    }
}

void hashset_rem(struct hashset* set, char* key) {
    uint64_t hashum = hashmap_hash(key);
    if (set->multithreaded) {
        pthread_rwlock_wrlock(&set->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(hashum, set->bucket_count);
    struct hashset_bucket_entry* bucket = set->buckets[hash];
    if (bucket == NULL) {
        if (set->multithreaded) {
            pthread_rwlock_unlock(&set->rwlock);
        }
        return;
    }
    struct hashset_bucket_entry* last_bucket = NULL;
    for (; bucket != NULL; last_bucket = bucket, bucket = bucket->next) {
        if (bucket->umod_hash == hashum && strcmp(bucket->key, key) == 0) {
            if (last_bucket != NULL) {
                last_bucket->next = bucket->next;
                pprefree(set->pool, bucket);
                break;
            } else {
                set->buckets[hash] = bucket->next;
                pprefree(set->pool, bucket);
                break;
            }
        }
    }
    --set->entry_count;
    if (set->multithreaded) {
        pthread_rwlock_unlock(&set->rwlock);
    }
}

void hashset_remptr(struct hashset* set, void* key) {
    return hashset_remint(set, (uint64_t) key >> 2);
}

void hashset_remint(struct hashset* set, uint64_t key) {
    if (set->multithreaded) {
        pthread_rwlock_wrlock(&set->rwlock);
    }
    uint64_t hash = hashmap_hash_mod(key, set->bucket_count);
    struct hashset_bucket_entry* bucket = set->buckets[hash];
    if (bucket == NULL) {
        if (set->multithreaded) {
            pthread_rwlock_unlock(&set->rwlock);
        }
        return;
    }
    struct hashset_bucket_entry* last_bucket = NULL;
    for (; bucket != NULL; last_bucket = bucket, bucket = bucket->next) {
        if (bucket->umod_hash == key) {
            if (last_bucket != NULL) {
                last_bucket->next = bucket->next;
                pprefree(set->pool, bucket);
                break;
            } else {
                set->buckets[hash] = bucket->next;
                pprefree(set->pool, bucket);
                break;
            }
        }
    }
    if (set->multithreaded) {
        pthread_rwlock_unlock(&set->rwlock);
    }
}

// precondition: set is locked
void hashset_fixcap(struct hashset* set) {
    if ((set->entry_count / set->bucket_count) > 4) {
        size_t nbuck_count = set->bucket_count * 2;
        set->buckets = prealloc(set->pool, set->buckets, nbuck_count * sizeof(struct hashset_bucket_entry*));
        memset((void*) set->buckets + (set->bucket_count * sizeof(struct hashset_bucket_entry*)), 0,
               set->bucket_count * sizeof(struct hashset_bucket_entry*));
        for (size_t i = 0; i < set->bucket_count; i++) {
            struct hashset_bucket_entry* lbucket = NULL;
            for (struct hashset_bucket_entry* bucket = set->buckets[i]; bucket != NULL;) {
                size_t ni = hashmap_hash_mod(bucket->umod_hash, nbuck_count);
                if (ni == i) {
                    lbucket = bucket;
                    bucket = bucket->next;
                    continue;
                } else {
                    struct hashset_bucket_entry* nbucket = bucket->next;
                    if (lbucket == NULL) {
                        set->buckets[i] = nbucket;
                    } else {
                        lbucket->next = nbucket;
                    }
                    if (set->buckets[ni] == NULL) {
                        set->buckets[ni] = bucket;
                        bucket->next = NULL;
                    } else {
                        bucket->next = set->buckets[ni];
                        set->buckets[ni] = bucket;
                    }
                    bucket = nbucket;
                    // no lbucket change
                }
            }
        }
        set->bucket_count = nbuck_count;
    }
}

struct hashmap* hashmap_clone(struct hashmap* map, struct mempool* pool) {
    if (map->multithreaded) {
        pthread_rwlock_rdlock(&map->rwlock);
    }
    struct hashmap* newmap = (map->multithreaded ? hashmap_thread_new : hashmap_new)(map->bucket_count, pool);
    for (size_t i = 0; i < map->bucket_count; i++) {
        if (map->buckets[i] == NULL) continue;
        struct hashmap_bucket_entry** newbucket = &newmap->buckets[i];
        for (struct hashmap_bucket_entry* bucket = map->buckets[i]; bucket != NULL; bucket = bucket->next) {
            (*newbucket) = pcalloc(pool, sizeof(struct hashmap_bucket_entry));
            (*newbucket)->data = bucket->data;
            (*newbucket)->key = bucket->key;
            (*newbucket)->umod_hash = bucket->umod_hash;
            newbucket = &((*newbucket)->next);
        }
    }
    if (map->multithreaded) {
        pthread_rwlock_unlock(&map->rwlock);
    }
    return newmap;
}