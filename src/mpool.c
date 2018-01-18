#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mpool.h"

static inline void* mpool_ptr(mpool_t* pool) {
    return ((uint8_t*)pool->begin) + pool->size;
}

mpool_t* mpool_create(size_t cap) {
    mpool_t* pool = malloc(sizeof(mpool_t));
    pool->begin = malloc(cap);
    pool->cap   = cap;
    pool->size  = 0;
    pool->next  = NULL;
    return pool;
}

void mpool_destroy(mpool_t* pool) {
    while (pool) {
        mpool_t* next = pool->next;
        free(pool->begin);
        free(pool);
        pool = next;
    }
}

void* mpool_alloc(mpool_t** root, size_t size) {
    mpool_t* pool = *root;
    while (true) {
        if (pool->cap - pool->size >= size) {
            void* ptr = mpool_ptr(pool);
            pool->size += size;
            return ptr;
        }

        mpool_t* next = mpool_create(size > pool->cap ? pool->cap : size);
        next->next = pool;
        *root = next;
        pool  = next;
    }
}
