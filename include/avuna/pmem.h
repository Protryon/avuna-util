//
// Created by p on 2/10/19.
//

#ifndef AVUNA_HTTPD_PMEM_H
#define AVUNA_HTTPD_PMEM_H


// this memory pool is a lie, this is just allocation tracking for lazy deallocation -- not preallocation

// each `mempool` struct can be used to track arbitrary allocations, then free those allocations all at once at some point.
// it also uses hooks via `phook` in order to track callbacks for various other resources, such as file descriptors
// the child mechanism exposed through `pchild` allows a directed dependency tree of pools to be formed, such that the `pfree` of a parent `pfree`s all children, but `pfree`ing the child first will not cause a double free in the parent.

#include <avuna/hash.h>
#include <avuna/list.h>
#include <avuna/smem.h>


void* (*pmem_malloc)(size_t size);
void* (*pmem_calloc)(size_t size);
void* (*pmem_realloc)(void* ptr, size_t size);
void (*pmem_free)(void* ptr);

// single thread access only!
struct mempool {
    struct hashset* allocations;
    struct list* hooks;
};

// new empty mempool
struct mempool* mempool_new();

// run all exit hooks, then free all entries in the pool
void pfree(struct mempool* pool);

// `malloc` and track in the pool
void* pmalloc(struct mempool* pool, size_t size);

// `calloc` and track in the pool
void* pcalloc(struct mempool* pool, size_t size);

// `realloc`, remove the previous entry if changed, and track in the pool
void* prealloc(struct mempool* pool, void* ptr, size_t size);

// moves `ptr` from being a direct descendant of `from` to `to`.
void* pxfer(struct mempool* from, struct mempool* to, void* ptr);

// claim `ptr` within this pool as if it was allocated here
void* pclaim(struct mempool* pool, void* ptr);

// release ownership of `ptr`, `free` will not be called from `pfree`. this doesn't handle hooks.
void* punclaim(struct mempool* pool, void* ptr);

// this `free`s `ptr` before the rest of the pool, and safely removes it. this does nothing if `ptr` is not controlled directly by this pool.
// this is dangerous if any hooks use `ptr`, as they will NOT be removed
void pprefree(struct mempool* pool, void* ptr);

// same as `pprefree`, but will always call `free` even if `ptr` is not controlled by `pool`.
void pprefree_strict(struct mempool* pool, void* ptr);

// add a hook to `pool` to be called during `pfree` but before any data is `free`d.
void phook(struct mempool* pool, void (*hook)(void* arg), void* arg);

// make `child` a direct descendant of `parent` such that calling `pfree` on `parent` will call `pfree` on `child`.
// it is safe to `pfree` child after this, hooks will be removed and managed appropriately
void pchild(struct mempool* parent, struct mempool* child);

// transfer `child` from being a direct descendant of `current_parent` to `new_parent`.
void pxfer_parent(struct mempool* current_parent, struct mempool* new_parent, struct mempool* child);

// equivalent of mempool_new(), pchild
struct mempool* psub(struct mempool* parent);

#endif //AVUNA_HTTPD_PMEM_H
