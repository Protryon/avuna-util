//
// Created by p on 2/10/19.
//

#include <avuna/pmem.h>
#include <avuna/hash.h>
#include <avuna/smem.h>
#include <stdarg.h>

void* (*pmem_malloc)(size_t size) = smalloc;
void* (*pmem_calloc)(size_t size) = scalloc;
void* (*pmem_realloc)(void* ptr, size_t size) = srealloc;
void (*pmem_free)(void* ptr) = free;


struct hook_entry {
    void (*hook)(void* arg);

    void* arg;
};

struct mempool* mempool_new() {
    struct mempool* pool = pmem_malloc(sizeof(struct mempool));
    pool->allocations = hashset_new(16, NULL);
    pool->hooks = list_new(16, pool);
    return pool;
}

void pfree(struct mempool* pool) {
    if (pool == NULL) {
        return;
    }
    for (size_t i = 0; i < pool->hooks->count; ++i) {
        struct hook_entry* entry = pool->hooks->data[i];
        entry->hook(entry->arg);
    }
    ITER_SET(pool->allocations) {
        pmem_free(ptr_key);
        ITER_SET_END();
    }
    hashset_free2(pool->allocations, pmem_free);
    pmem_free(pool);
}

void* pmalloc(struct mempool* pool, size_t size) {
    void* item = pmem_malloc(size);
    if (pool != NULL) hashset_addptr(pool->allocations, item);
    return item;
}

void* pcalloc(struct mempool* pool, size_t size) {
    void* item = pmem_calloc(size);
    if (pool != NULL) hashset_addptr(pool->allocations, item);
    return item;
}

void* prealloc(struct mempool* pool, void* ptr, size_t size) {
    void* item = pmem_realloc(ptr, size);
    if (pool != NULL && item != ptr) {
        hashset_remptr(pool->allocations, ptr);
        hashset_addptr(pool->allocations, item);
    }
    return item;
}

void* pxfer(struct mempool* from, struct mempool* to, void* ptr) {
    if (from != NULL && ptr != NULL && hashset_hasptr(from->allocations, ptr)) {
        punclaim(from, ptr);
        pclaim(to, ptr);
    } else if (from == NULL) {
        pclaim(to, ptr);
    }
    return ptr;
}

void* pclaim(struct mempool* pool, void* ptr) {
    if (pool != NULL && ptr != NULL) hashset_addptr(pool->allocations, ptr);
    return ptr;
}

void* punclaim(struct mempool* pool, void* ptr) {
    if (pool != NULL && ptr != NULL) hashset_remptr(pool->allocations, ptr);
    return ptr;
}

void pprefree(struct mempool* pool, void* ptr) {
    if (pool != NULL && ptr != NULL && hashset_hasptr(pool->allocations, ptr)) {
        hashset_remptr(pool->allocations, ptr);
    }
    pmem_free(ptr);
}

void pprefree_strict(struct mempool* pool, void* ptr) {
    if (pool != NULL && ptr != NULL && hashset_hasptr(pool->allocations, ptr)) {
        hashset_remptr(pool->allocations, ptr);
        pmem_free(ptr);
    }
}


void phook(struct mempool* pool, void (*hook)(void* arg), void* arg) {
    struct hook_entry* entry = pmalloc(pool, sizeof(struct hook_entry));
    entry->hook = hook;
    entry->arg = arg;
    list_append(pool->hooks, entry);
}

struct _mempool_pair { // always allocated in child
    struct mempool* parent;
    struct mempool* child;
};

void _punhook_parent(struct _mempool_pair* pair) {
    for (size_t i = 0; i < pair->parent->hooks->count; ++i) {
        struct hook_entry* entry = (struct hook_entry*) pair->parent->hooks->data[i];
        if (entry->hook == pfree && entry->arg == pair->child) {
            entry->arg = NULL; // disables pfree call
        }
    }
}

void _prehook_child(struct mempool* child, struct mempool* new_parent) {
    for (size_t i = 0; i < child->hooks->count; ++i) {
        struct hook_entry* entry = (struct hook_entry*) child->hooks->data[i];
        if (entry->hook == _punhook_parent) {
            struct _mempool_pair* pair = entry->arg;
            pair->parent = new_parent;
            return;
        }
    }
}

void pchild(struct mempool* parent, struct mempool* child) {
    phook(parent, pfree, child);
    struct _mempool_pair* pair = pmalloc(child, sizeof(struct _mempool_pair));
    pair->child = child;
    pair->parent = parent;
    phook(child, _punhook_parent, pair);
}

void pxfer_parent(struct mempool* current_parent, struct mempool* new_parent, struct mempool* child) {
    struct _mempool_pair unhook;
    unhook.parent = current_parent;
    unhook.child = child;
    _punhook_parent(&unhook);
    _prehook_child(child, new_parent);
    phook(new_parent, pfree, child);
}

struct mempool* psub(struct mempool* parent) {
    struct mempool* pool = mempool_new();
    pchild(parent, pool);
    return pool;
}

void* pdup(struct mempool* pool, void* raw, size_t size) {
    void* new = pmalloc(pool, size);
    memcpy(new, raw, size);
    return new;
}

char* pstr(struct mempool* pool, char* raw) {
    return pdup(pool, raw, strlen(raw + 1));
}

char* pprintf(struct mempool* pool, const char* template, ...) {
    size_t buf_size = 1024;
    char* output = smalloc(buf_size);
    va_list args;
    va_start(args, template);
    while (vsnprintf(output, buf_size, template, args) < 0) {
        buf_size *= 2;
        output = srealloc(output, buf_size);
    }
    va_end(args);
    pclaim(pool, output);
    return output;
}