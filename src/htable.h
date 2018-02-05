#ifndef HTABLE_H
#define HTABLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define OCCUPIED_HASH_MASK 0x80000000
#define INVALID_INDEX ((size_t)-1)

typedef struct htable_s htable_t;

typedef bool     (*cmpfn_t)(const void*, const void*);
typedef uint32_t (*hashfn_t)(const void*);

struct htable_s {
    size_t esize;
    size_t cap;
    size_t nelems;
    void* elems;
    uint32_t* hashes;
    cmpfn_t cmp_fn;
    hashfn_t hash_fn;
};

htable_t* htable_create(size_t, size_t, cmpfn_t, hashfn_t);
void htable_destroy(htable_t*);
void htable_clear(htable_t*);
void htable_rehash(htable_t*, size_t);
bool htable_insert(htable_t*, const void*);
bool htable_remove(htable_t*, const void*);
void htable_remove_by_index(htable_t*, size_t);
size_t htable_lookup(htable_t*, const void*);

#endif // HTABLE_H
