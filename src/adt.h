#ifndef ADT_H
#define ADT_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "htable.h"
#include "hash.h"

#define VEC_DEFAULT_CAP  64
#define HMAP_DEFAULT_CAP 64
#define HSET_DEFAULT_CAP 64

#define FORALL_HMAP(hmap, key_t, key, value_t, value, ...) \
    for (size_t i = 0; i < (hmap).table->cap; ++i) { \
        if ((hmap).table->hashes[i] & OCCUPIED_HASH_MASK) { \
            struct pair_s { key_t key; value_t value; }; \
            struct pair_s* pair = ((struct pair_s*)(hmap).table->elems) + i; \
            key_t key     = pair->key; \
            value_t value = pair->value; \
            __VA_ARGS__ \
        } \
    }

#define HMAP(hmap, key_t, value_t, cmp, hash) \
    typedef struct { htable_t* table; } hmap##_t; \
    static inline hmap##_t hmap##_create_with_cap(size_t cap) { \
        struct pair_s { key_t key; value_t value; }; \
        return (hmap##_t) { \
            .table = htable_create(sizeof(struct pair_s), cap, cmp) \
        }; \
    } \
    static inline hmap##_t hmap##_create(void) { \
        return hmap##_create_with_cap(HMAP_DEFAULT_CAP); \
    } \
    static inline hmap##_t hmap##_copy(const hmap##_t* map) { \
        return (hmap##_t) { \
            .table = htable_copy(map->table) \
        }; \
    } \
    static inline void hmap##_destroy(hmap##_t* map) { \
        htable_destroy(map->table); \
    } \
    static inline void hmap##_clear(hmap##_t* map) { \
        htable_clear(map->table); \
    } \
    static inline bool hmap##_insert(hmap##_t* map, key_t k, value_t v) { \
        struct { key_t key; value_t value; } elem = { .key = k, .value = v }; \
        return htable_insert(map->table, &elem, hash(&elem)); \
    } \
    static inline bool hmap##_remove(hmap##_t* map, key_t k) { \
        return htable_remove(map->table, &k, hash(&k)); \
    } \
    static inline const value_t* hmap##_lookup(const hmap##_t* map, key_t k) { \
        size_t index = htable_lookup(map->table, &k, hash(&k)); \
        struct pair_s { key_t key; const value_t value; };\
        return index != INVALID_INDEX ? &((struct pair_s*)map->table->elems)[index].value : NULL; \
    }

#define HMAP_DEFAULT(hmap, key_t, value_t) \
    static uint32_t hmap##_hash(const void* ptr) { \
        return hash_bytes(hash_init(), ptr, sizeof(key_t)); \
    } \
    static bool hmap##_cmp(const void* a, const void* b) { \
        return !memcmp(a, b, sizeof(key_t)); \
    } \
    HMAP(hmap, key_t, value_t, hmap##_cmp, hmap##_hash)

#define FORALL_HSET(hset, value_t, value, ...) \
    for (size_t i = 0; i < (hset).table->cap; ++i) { \
        if ((hset).table->hashes[i] & OCCUPIED_HASH_MASK) { \
            value_t value = ((value_t*)(hset).table->elems)[i]; \
            __VA_ARGS__ \
        } \
    }

#define HSET(hset, value_t, cmp, hash) \
    typedef struct { htable_t* table; } hset##_t; \
    static inline hset##_t hset##_create_with_cap(size_t cap) { \
        return (hset##_t) { \
            .table = htable_create(sizeof(value_t), cap, cmp) \
        }; \
    } \
    static inline hset##_t hset##_create(void) { \
        return hset##_create_with_cap(HSET_DEFAULT_CAP); \
    } \
    static inline hset##_t hset##_copy(const hset##_t* set) { \
        return (hset##_t) { \
            .table = htable_copy(set->table) \
        }; \
    } \
    static inline void hset##_destroy(hset##_t* set) { \
        htable_destroy(set->table); \
    } \
    static inline void hset##_clear(hset##_t* set) { \
        htable_clear(set->table); \
    } \
    static inline bool hset##_insert(hset##_t* set, value_t v) { \
        return htable_insert(set->table, &v, hash(&v)); \
    } \
    static inline bool hset##_remove(hset##_t* set, value_t v) { \
        return htable_remove(set->table, &v, hash(&v)); \
    } \
    static inline const value_t* hset##_lookup(const hset##_t* set, value_t v) { \
        size_t index = htable_lookup(set->table, &v, hash(&v)); \
        return index != INVALID_INDEX ? &((const value_t*)set->table->elems)[index] : NULL; \
    }

#define HSET_DEFAULT(hset, value_t) \
    static uint32_t hset##_hash(const void* ptr) { \
        return hash_bytes(hash_init(), ptr, sizeof(value_t)); \
    } \
    static bool hset##_cmp(const void* a, const void* b) { \
        return !memcmp(a, b, sizeof(value_t)); \
    } \
    HSET(hset, value_t, hset##_cmp, hset##_hash)

#define FORALL_VEC(vec, value_t, value, ...) \
    for (size_t i = 0; i < vec.nelems; ++i) { \
        value_t value = vec.elems[i]; \
        __VA_ARGS__ \
    }

#define VEC(vec, value_t) \
    typedef struct { size_t cap; size_t nelems; value_t* elems; } vec##_t; \
    static inline vec##_t vec##_create_with_cap(size_t cap) { \
        return (vec##_t) { \
            .cap = cap, \
            .nelems = 0, \
            .elems = malloc(sizeof(value_t) * cap) \
        }; \
    } \
    static inline vec##_t vec##_create(void) { \
        return vec##_create_with_cap(VEC_DEFAULT_CAP); \
    } \
    static inline void vec##_destroy(vec##_t* vec) { \
        free(vec->elems); \
    } \
    static inline void vec##_push(vec##_t* vec, value_t v) { \
        if (vec->nelems >= vec->cap) { \
            vec->cap *= 2; \
            vec->elems = realloc(vec->elems, sizeof(value_t) * vec->cap); \
        } \
        vec->elems[vec->nelems++] = v; \
    } \
    static inline value_t vec##_pop(vec##_t* vec) { \
        return vec->elems[--vec->nelems]; \
    } \
    static inline void vec##_resize(vec##_t* vec, size_t nelems) { \
        if (nelems > vec->cap) { \
            vec->cap = nelems; \
            vec->elems = realloc(vec->elems, sizeof(value_t) * vec->cap); \
        } \
        vec->nelems = nelems; \
    } \
    static inline void vec##_shrink(vec##_t* vec) { \
        vec->elems = realloc(vec->elems, sizeof(value_t) * vec->nelems); \
        vec->cap = vec->nelems; \
    } \
    static inline value_t* vec##_find(vec##_t* vec, value_t v) { \
        for (size_t i = 0; i < vec->nelems; ++i) { \
            if (!memcmp(&vec->elems[i], &v, sizeof(value_t))) \
                return &vec->elems[i]; \
        } \
        return NULL; \
    }

#endif // ADT_H
