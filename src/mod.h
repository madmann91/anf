#ifndef MOD_H
#define MOD_H

#include "adt.h"
#include "mpool.h"

typedef struct mod_s  mod_t;
typedef struct type_s type_t;
typedef struct node_s node_t;

typedef struct struct_def_s struct_def_t;

struct struct_def_s {
    const char* name;
    const type_t** mbs;
    size_t nmbs;
};

VEC(struct_def_vec, struct_def_t)

VEC(type_vec, const type_t*)
HSET_DEFAULT(type_set, const type_t*)
HMAP_DEFAULT(type2type, const type_t*, const type_t*)

VEC(node_vec, const node_t*)
HSET_DEFAULT(node_set, const node_t*)
HMAP_DEFAULT(node2node, const node_t*, const node_t*)

#define FORALL_TYPES(mod, type, ...) FORALL_HSET(mod->types, const type_t*, type, __VA_ARGS__)
#define FORALL_NODES(mod, node, ...) FORALL_HSET(mod->nodes, const node_t*, node, __VA_ARGS__)
#define FORALL_FNS(mod, fn, ...)     FORALL_VEC(mod->fns, const node_t*, fn, __VA_ARGS__)

bool node_cmp(const void*, const void*);
uint32_t node_hash(const void* ptr);
bool type_cmp(const void*, const void*);
uint32_t type_hash(const void*);

HSET(internal_type_set, const type_t*, type_cmp, type_hash)
HSET(internal_node_set, const node_t*, node_cmp, node_hash)

struct mod_s {
    mpool_t*            pool;
    node_vec_t          fns;
    struct_def_vec_t    structs;
    internal_node_set_t nodes;
    internal_type_set_t types;
};

mod_t* mod_create(void);
void mod_destroy(mod_t*);

void mod_opt(mod_t**);

void node_bind(mod_t*, const node_t*, size_t, const node_t*);

const type_t* mod_insert_type(mod_t*, const type_t*);
const node_t* mod_insert_node(mod_t*, const node_t*);

#endif // MOD_H
