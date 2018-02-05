#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "htable.h"

static inline void* htable_elem(void* elems, size_t index, size_t esize) {
    return ((uint8_t*)elems) + index * esize;
}

static inline size_t htable_index(uint32_t hash, size_t cap) {
    return hash & (cap - 1);
}

static inline size_t htable_dib(size_t index, size_t expected_index, size_t cap) {
    return index < expected_index ? (cap + index) - expected_index : index - expected_index;
}

static inline bool htable_insert_internal(const void* restrict elem, uint32_t hash,
                                          void* restrict elems, uint32_t* hashes,
                                          size_t esize, size_t cap,
                                          cmpfn_t cmp_fn, bool cmp) {
    size_t index  = htable_index(hash, cap);
    size_t dib    = 0;

    uint8_t tmp_elem[esize];
    uint8_t cur_elem[esize];
    memcpy(cur_elem, elem, esize);

    while (true) {
        uint32_t next_hash = hashes[index];
        if (!(next_hash & OCCUPIED_HASH_MASK))
            break;
        next_hash &= ~OCCUPIED_HASH_MASK;

        size_t next_index = htable_index(next_hash, cap);
        size_t next_dib   = htable_dib(index, next_index, cap);
        void* next_elem   = htable_elem(elems, index, esize);
        if (cmp && cmp_fn(next_elem, cur_elem))
            return false;
        // Robin Hood hashing: swap the current element with the element to insert if
        // its Distance to Initial Bucket is lower than that of the current element.
        if (next_dib < dib) {
            hashes[index] = hash | OCCUPIED_HASH_MASK;
            memcpy(tmp_elem,  cur_elem,  esize);
            memcpy(cur_elem,  next_elem, esize);
            memcpy(next_elem, tmp_elem,  esize);
            hash = next_hash;
            dib  = next_dib;
        }
        dib++;
        index++;
        index = index >= cap ? 0 : index;
    }
    hashes[index] = hash | OCCUPIED_HASH_MASK;
    memcpy(htable_elem(elems, index, esize), cur_elem, esize);
    return true;
}

htable_t* htable_create(size_t esize, size_t cap, cmpfn_t cmp_fn, hashfn_t hash_fn) {
    assert((cap & (cap - 1)) == 0);
    htable_t* table = malloc(sizeof(htable_t));
    table->esize   = esize;
    table->cap     = cap;
    table->nelems  = 0;
    table->elems   = malloc(esize * cap);
    table->hashes  = malloc(sizeof(uint32_t) * cap);
    table->cmp_fn  = cmp_fn;
    table->hash_fn = hash_fn;
    memset(table->hashes, 0, sizeof(uint32_t) * cap);
    return table;
}

void htable_destroy(htable_t* table) {
    free(table->elems);
    free(table->hashes);
    free(table);
}

void htable_clear(htable_t* table) {
    table->nelems = 0;
    memset(table->hashes, 0, sizeof(uint32_t) * table->cap);
}

void htable_rehash(htable_t* table, size_t new_cap) {
    assert((new_cap & (new_cap - 1)) == 0);
    void*     new_elems  = malloc(table->esize * new_cap);
    uint32_t* new_hashes = malloc(sizeof(uint32_t) * new_cap);
    memset(new_hashes, 0, sizeof(uint32_t) * new_cap);

    for (size_t i = 0; i < table->cap; ++i) {
        uint32_t hash = table->hashes[i];
        if (!(hash & OCCUPIED_HASH_MASK))
            continue;
        hash &= ~OCCUPIED_HASH_MASK;

        const void* elem = htable_elem(table->elems, i, table->esize);
        htable_insert_internal(elem, hash,
                               new_elems, new_hashes,
                               table->esize, new_cap,
                               NULL, false);
    }

    free(table->elems);
    free(table->hashes);
    table->elems  = new_elems;
    table->hashes = new_hashes;
    table->cap    = new_cap;
}

bool htable_insert(htable_t* table, const void* elem) {
    if (!htable_insert_internal(elem, table->hash_fn(elem) & ~OCCUPIED_HASH_MASK,
                                table->elems, table->hashes,
                                table->esize, table->cap,
                                table->cmp_fn, true))
        return false;

    table->nelems++;

    // Test if the number of elements reaches 80% of the capacity
    const size_t max_load_factor = 80;
    if (table->nelems * 100 > max_load_factor * table->cap)
        htable_rehash(table, table->cap * 2);
    return true;
}

bool htable_remove(htable_t* table, const void* elem) {
    size_t index = htable_lookup(table, elem);
    if (index == INVALID_INDEX)
        return false;
    htable_remove_by_index(table, index);
    return true;
}

void htable_remove_by_index(htable_t* table, size_t index) {
    assert(table->hashes[index] & OCCUPIED_HASH_MASK);

    // Count number of buckets until an empty bucket or bucket with DIB=0 is found
    size_t prev  = index;
    while (true) {
        index++;
        index = index >= table->cap ? 0 : index;

        uint32_t next_hash = table->hashes[index];
        if (!(next_hash & OCCUPIED_HASH_MASK))
            break;
        next_hash &= ~OCCUPIED_HASH_MASK;

        size_t next_index = htable_index(next_hash, table->cap);
        size_t next_dib   = htable_dib(index, next_index, table->cap);
        if (next_dib == 0)
            break;
    }
    if (index > prev) {
        index--;
        memmove(htable_elem(table->elems, prev,     table->esize),
                htable_elem(table->elems, prev + 1, table->esize),
                table->esize * (index - prev));
        memmove(table->hashes + prev,
                table->hashes + prev + 1,
                sizeof(uint32_t) * (index - prev));
    } else {
        // The index has wrapped around
        memmove(htable_elem(table->elems, prev,     table->esize),
                htable_elem(table->elems, prev + 1, table->esize),
                table->esize * (table->cap - prev));
        memmove(table->hashes + prev,
                table->hashes + prev + 1,
                sizeof(uint32_t) * (table->cap - prev));
        if (index > 0) {
            memcpy(htable_elem(table->elems, table->cap - 1, table->esize),
                   htable_elem(table->elems, 0, table->esize),
                   table->esize);
            table->hashes[table->cap - 1] = table->hashes[0];
            index--;
            memmove(htable_elem(table->elems, 0, table->esize),
                    htable_elem(table->elems, 1, table->esize),
                    table->esize * index);
            memmove(table->hashes,
                    table->hashes + 1,
                    sizeof(uint32_t) * index);
        } else {
            index = table->cap - 1;
        }
    }
    // Clear the last element of the chain
    table->hashes[index] = 0;
    table->nelems--;
}

size_t htable_lookup(htable_t* table, const void* elem) {
    uint32_t hash = table->hash_fn(elem) & ~OCCUPIED_HASH_MASK;
    size_t index  = htable_index(hash, table->cap);
    size_t dib    = 0;

    while (true) {
        uint32_t next_hash  = table->hashes[index];
        if (!(next_hash & OCCUPIED_HASH_MASK))
            return INVALID_INDEX;
        next_hash &= ~OCCUPIED_HASH_MASK;

        size_t next_index = htable_index(next_hash, table->cap);
        size_t next_dib   = htable_dib(index, next_index, table->cap);
        if (next_dib < dib)
            return INVALID_INDEX;

        void* next_elem = htable_elem(table->elems, index, table->esize);
        if (table->cmp_fn(next_elem, elem))
            return index;

        dib++;
        index++;
        index = index >= table->cap ? 0 : index;
    }
}
