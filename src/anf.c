#include <stdlib.h>
#include <string.h>

#include "anf.h"
#include "hash.h"

bool node_cmp(const void* ptr1, const void* ptr2) {
    const node_t* node1 = *(const node_t**)ptr1;
    const node_t* node2 = *(const node_t**)ptr2;
    return node1->tag  == node2->tag  &&
           node1->nops == node2->nops &&
           !memcmp(node_ops(node1), node_ops(node2), sizeof(node_t*) * node1->nops);
}

uint32_t node_hash(const void* ptr) {
    const node_t* node = *(const node_t**)ptr;
    const node_t** ops = node_ops(node);
    uint32_t h = hash_init();
    for (size_t i = 0; i < node->nops; ++i)
        h = hash_ptr(h, ops[i]);
    return h;
}

bool type_cmp(const void* ptr1, const void* ptr2) {
    const type_t* type1 = *(const type_t**)ptr1;
    const type_t* type2 = *(const type_t**)ptr2;
    return type1->tag  == type2->tag  &&
           type1->nops == type2->nops &&
           !memcmp(type_ops(type1), type_ops(type2), sizeof(type_t*) * type1->nops);
}

uint32_t type_hash(const void* ptr) {
    const type_t* type = *(const type_t**)ptr;
    const type_t** ops = type_ops(type);
    uint32_t h = hash_init();
    for (size_t i = 0; i < type->nops; ++i)
        h = hash_ptr(h, ops[i]);
    return h;
}

mod_t* mod_create(void) {
    mod_t* mod = malloc(sizeof(mod_t));
    mod->pool  = mpool_create(1 << 10);
    mod->nodes = htable_create(sizeof(node_t*), 256, node_cmp, node_hash);
    mod->types = htable_create(sizeof(type_t*), 64,  type_cmp, type_hash);
    return mod;
}

void mod_destroy(mod_t* mod) {
    mpool_destroy(mod->pool);
    htable_destroy(mod->nodes);
    htable_destroy(mod->types);
    free(mod);
}
