#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "anf.h"
#include "hash.h"

bool node_cmp(const void* ptr1, const void* ptr2) {
    const node_t* node1 = *(const node_t**)ptr1;
    const node_t* node2 = *(const node_t**)ptr2;
    if (node1->tag  != node2->tag ||
        node1->nops != node2->nops)
        return false;
    for (size_t i = 0; i < node1->nops; ++i) {
        if (node1->ops[i] != node2->ops[i])
            return false;
    }
    return true;
}

uint32_t node_hash(const void* ptr) {
    const node_t* node = *(const node_t**)ptr;
    uint32_t h = hash_init();
    h = hash_uint32(h, node->tag);
    for (size_t i = 0; i < node->nops; ++i)
        h = hash_ptr(h, node->ops[i]);
    switch (node->tag) {
        default: break;
    }
    return h;
}

bool type_cmp(const void* ptr1, const void* ptr2) {
    const type_t* type1 = *(const type_t**)ptr1;
    const type_t* type2 = *(const type_t**)ptr2;
    if (type1->tag  != type2->tag ||
        type1->name != type2->name ||
        type1->nops != type2->nops)
        return false;
    for (size_t i = 0; i < type1->nops; ++i) {
        if (type1->ops[i] != type2->ops[i])
            return false;
    }
    return true;
}

uint32_t type_hash(const void* ptr) {
    const type_t* type = *(const type_t**)ptr;
    uint32_t h = hash_init();
    h = hash_uint32(h, type->tag);
    h = hash_uint32(h, type->name);
    for (size_t i = 0; i < type->nops; ++i)
        h = hash_ptr(h, type->ops[i]);
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

size_t type_bitwidth(const type_t* type) {
    switch (type->tag) {
        case TYPE_I1:  return 1;
        case TYPE_I8:  return 8;
        case TYPE_I16: return 16;
        case TYPE_I32: return 32;
        case TYPE_I64: return 64;
        case TYPE_U8:  return 8;
        case TYPE_U16: return 16;
        case TYPE_U32: return 32;
        case TYPE_U64: return 64;
        case TYPE_F32: return 32;
        case TYPE_F64: return 64;
        default:
            assert(false);
            return -1;
    }
}

bool type_is_prim(const type_t* type) {
    switch (type->tag) {
        case TYPE_I1:  return true;
        case TYPE_I8:  return true;
        case TYPE_I16: return true;
        case TYPE_I32: return true;
        case TYPE_I64: return true;
        case TYPE_U8:  return true;
        case TYPE_U16: return true;
        case TYPE_U32: return true;
        case TYPE_U64: return true;
        case TYPE_F32: return true;
        case TYPE_F64: return true;
        default:       return false;
    }
}

static inline const type_t* make_type(mod_t* mod, uint32_t tag, uint32_t name, size_t nops, const type_t** ops) {
    type_t type = {
        .tag = tag,
        .nops = nops,
        .name = name,
        .ops  = ops
    };

    size_t index = htable_lookup(mod->types, &type);
    if (index != INVALID_INDEX)
        return ((const type_t**)mod->types->elems)[index];
    type_t* type_ptr = mpool_alloc(&mod->pool, sizeof(type_t));
    *type_ptr = type;
    bool success = htable_insert(mod->types, &type_ptr);
    assert(success);
    return type_ptr;
}

const type_t* type_i(mod_t* mod, size_t bitwidth) {
    switch (bitwidth) {
        case 1:  return make_type(mod, TYPE_I1,  0, 0, NULL);
        case 8:  return make_type(mod, TYPE_I8,  0, 0, NULL);
        case 16: return make_type(mod, TYPE_I16, 0, 0, NULL);
        case 32: return make_type(mod, TYPE_I32, 0, 0, NULL);
        case 64: return make_type(mod, TYPE_I64, 0, 0, NULL);
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_u(mod_t* mod, size_t bitwidth) {
    switch (bitwidth) {
        case 8:  return make_type(mod, TYPE_U8,  0, 0, NULL);
        case 16: return make_type(mod, TYPE_U16, 0, 0, NULL);
        case 32: return make_type(mod, TYPE_U32, 0, 0, NULL);
        case 64: return make_type(mod, TYPE_U64, 0, 0, NULL);
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_f(mod_t* mod, size_t bitwidth) {
    switch (bitwidth) {
        case 32: return make_type(mod, TYPE_F32, 0, 0, NULL);
        case 64: return make_type(mod, TYPE_F64, 0, 0, NULL);
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_tuple(mod_t* mod, size_t nops, const type_t** ops) {
    if (nops == 1) return ops[0];
    return make_type(mod, TYPE_TUPLE, 0, nops, ops);
}

const type_t* type_fn(mod_t* mod, const type_t* from, const type_t* to) {
    const type_t* ops[] = { from, to };
    return make_type(mod, TYPE_FN, 0, 2, ops);
}
