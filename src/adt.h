#ifndef ADT_H
#define ADT_H

#include <stdlib.h>
#include <assert.h>

#include "htable.h"

#define FORALL_HMAP(hmap, key_t, key, value_t, value, body) \
    for (size_t i = 0; i < (hmap).table->cap; ++i) { \
        if ((hmap).table->hashes[i] & OCCUPIED_MASK) { \
            struct pair_s { key_t key; value_t value; }; \
            struct pair_s pair = ((struct pair_s*)(hmap).table->elems)[i]; \
            key_t key     = pair->key; \
            value_t value = pair->value; \
            body \
        } \
    }

#define HMAP(hmap, key_t, value_t) \
    typedef struct { htable_t* table; } hmap##_t; \
    static inline hmap##_t hmap##_create(size_t cap, cmpfn_t cmp, hashfn_t hash) { \
        struct pair_s { key_t key; value_t value; }; \
        return (hmap##_t) { \
            .table = htable_create(sizeof(struct pair_s), cap, cmp, hash) \
        }; \
    } \
    static inline void hmap##_destroy(hmap##_t* map) { \
        htable_destroy(map->table); \
    } \
    static inline bool hmap##_insert(hmap##_t* map, key_t k, value_t v) { \
        struct { key_t key; value_t value; } elem = { .key = k, .value = v }; \
        return htable_insert(map->table, &elem); \
    } \
    static inline bool hmap##_remove(hmap##_t* map, key_t k) { \
        return htable_remove(map->table, &k); \
    } \
    static inline const value_t* hmap##_lookup(hmap##_t* map, key_t k) { \
        size_t index = htable_lookup(map->table, &k); \
        struct pair_s { key_t key; const value_t value; };\
        return index != INVALID_INDEX ? &((struct pair_s*)map->table->elems)[index].value : NULL; \
    }

#define FORALL_HSET(hset, value_t, value, body) \
    for (size_t i = 0; i < (hset).table->cap; ++i) { \
        if ((hset).table->hashes[i] & OCCUPIED_MASK) { \
            value_t value = ((value_t*)(hset).table->elems)[i]; \
            body \
        } \
    }

#define HSET(hset, value_t) \
    typedef struct { htable_t* table; } hset##_t; \
    static inline hset##_t hset##_create(size_t cap, cmpfn_t cmp, hashfn_t hash) { \
        return (hset##_t) { \
            .table = htable_create(sizeof(value_t), cap, cmp, hash) \
        }; \
    } \
    static inline void hset##_destroy(hset##_t* set) { \
        htable_destroy(set->table); \
    } \
    static inline bool hset##_insert(hset##_t* set, value_t v) { \
        return htable_insert(set->table, &v); \
    } \
    static inline bool hset##_remove(hset##_t* set, value_t v) { \
        return htable_remove(set->table, &v); \
    } \
    static inline const value_t* hset##_lookup(hset##_t* set, value_t v) { \
        size_t index = htable_lookup(set->table, &v); \
        return index != INVALID_INDEX ? &((const value_t*)set->table->elems)[index] : NULL; \
    }

#define VEC(vec, value_t) \
    typedef struct { size_t cap; size_t nelems; value_t* elems; } vec##_t; \
    static inline vec##_t vec##_create(size_t cap) { \
        assert(cap > 0); \
        return (vec##_t) { \
            .cap = cap, \
            .nelems = 0, \
            .elems = malloc(sizeof(value_t) * cap) \
        }; \
    } \
    static inline void vec##_destroy(vec##_t* vec) { \
        free(vec->elems); \
    } \
    static inline void vec##_push(vec##_t* vec, const value_t* v) { \
        if (vec->nelems >= vec->cap) { \
            vec->cap *= 2; \
            vec->elems = realloc(vec->elems, sizeof(value_t*) * vec->cap); \
        } \
        vec->elems[vec->nelems++] = *v; \
    } \
    static inline void vec##_shrink(vec##_t* vec) { \
        vec->elems = realloc(vec->elems, sizeof(value_t) * vec->nelems); \
        vec->cap = vec->nelems; \
    }

#endif // ADT_H
