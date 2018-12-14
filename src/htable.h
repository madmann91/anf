#ifndef HTABLE_H
#define HTABLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define OCCUPIED_HASH_MASK 0x80000000
#define INVALID_INDEX ((size_t)-1)

#define FORALL_OCCUPIED_HASHES(htable, i, ...) \
    for (size_t i = 0, n = (htable)->cap; i < n; ++i) { \
        if (htable->hashes[i] & OCCUPIED_HASH_MASK) { \
            __VA_ARGS__ \
        } \
    }

typedef struct htable_s htable_t;

typedef bool (*cmpfn_t)(const void*, const void*);

struct htable_s {
    size_t esize;
    size_t cap;
    size_t nelems;
    void* elems;
    uint32_t* hashes;
    cmpfn_t cmp_fn;
};

htable_t* htable_create(size_t, size_t, cmpfn_t);
void htable_destroy(htable_t*);
void htable_clear(htable_t*);
void htable_rehash(htable_t*, size_t);
bool htable_insert(htable_t*, const void*, uint32_t);
bool htable_remove(htable_t*, const void*, uint32_t);
void htable_remove_by_index(htable_t*, size_t);
size_t htable_lookup(htable_t*, const void*, uint32_t);
htable_t* htable_copy(const htable_t*);

#endif // HTABLE_H
