#ifndef MOD_H
#define MOD_H

#include "adt.h"

typedef struct mod_s  mod_t;
typedef struct type_s type_t;
typedef struct node_s node_t;

VEC(type_vec, const type_t*)
HSET_DEFAULT(type_set, const type_t*)
HMAP_DEFAULT(type2type, const type_t*, const type_t*)

VEC(node_vec, const node_t*)
HSET_DEFAULT(node_set, const node_t*)
HMAP_DEFAULT(node2node, const node_t*, const node_t*)

#define FORALL_TYPES(mod, type, ...) \
    { \
        const htable_t* htable = mod_types(mod); \
        FORALL_OCCUPIED_HASHES(htable, i, { \
            const type_t* type = ((const type_t**)htable->elems)[i]; \
            __VA_ARGS__ \
        }) \
    }

#define FORALL_NODES(mod, node, ...) \
    { \
        const htable_t* htable = mod_nodes(mod); \
        FORALL_OCCUPIED_HASHES(htable, i, { \
            const node_t* node = ((const node_t**)htable->elems)[i]; \
            __VA_ARGS__ \
        }) \
    }

#define FORALL_FNS(mod, fn, ...) \
    { \
        const node_vec_t* fns = mod_fns(mod); \
        for (size_t i = 0, n = fns->nelems; i < n; ++i) { \
            const node_t* fn = fns->elems[i]; \
            __VA_ARGS__ \
        } \
    }

mod_t* mod_create(void);
void mod_destroy(mod_t*);

void mod_opt(mod_t**);

const htable_t* mod_types(const mod_t*);
const htable_t* mod_nodes(const mod_t*);
const node_vec_t* mod_fns(const mod_t*);

void node_bind(mod_t*, const node_t*, size_t, const node_t*);

const type_t* mod_insert_type(mod_t*, const type_t*);
const node_t* mod_insert_node(mod_t*, const node_t*);

#endif // MOD_H
