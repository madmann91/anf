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
    if (node1->tag == NODE_LITERAL)
        return !memcmp(&node1->box, &node2->box, sizeof(box_t));
    return true;
}

uint32_t node_hash(const void* ptr) {
    const node_t* node = *(const node_t**)ptr;
    uint32_t h = hash_init();
    h = hash_uint32(h, node->tag);
    for (size_t i = 0; i < node->nops; ++i)
        h = hash_ptr(h, node->ops[i]);
    if (node->tag == NODE_LITERAL) {
        for (size_t i = 0; i < sizeof(box_t); ++i)
            h = hash_uint8(h, ((uint8_t*)&node->box)[i]);
    }
    return h;
}

bool type_cmp(const void* ptr1, const void* ptr2) {
    const type_t* type1 = *(const type_t**)ptr1;
    const type_t* type2 = *(const type_t**)ptr2;
    if (type1->tag  != type2->tag ||
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

static inline const type_t* make_type(mod_t* mod, const type_t type) {
    const type_t* lookup_ptr = &type;
    size_t index = htable_lookup(mod->types, &lookup_ptr);
    if (index != INVALID_INDEX)
        return ((const type_t**)mod->types->elems)[index];

    type_t* type_ptr = mpool_alloc(&mod->pool, sizeof(type_t));
    *type_ptr = type;
    if (type.nops > 0) {
        const type_t** type_ops = mpool_alloc(&mod->pool, sizeof(type_t*) * type.nops);
        for (size_t i = 0; i < type.nops; ++i) type_ops[i] = type.ops[i];
        type_ptr->ops = type_ops;
    }

    bool success = htable_insert(mod->types, &type_ptr);
    assert(success), (void)success;
    return type_ptr;
}

const type_t* type_i(mod_t* mod, size_t bitwidth) {
    switch (bitwidth) {
        case 1:  return make_type(mod, (type_t) { .tag = TYPE_I1,  .nops = 0, .ops = NULL });
        case 8:  return make_type(mod, (type_t) { .tag = TYPE_I8,  .nops = 0, .ops = NULL });
        case 16: return make_type(mod, (type_t) { .tag = TYPE_I16, .nops = 0, .ops = NULL });
        case 32: return make_type(mod, (type_t) { .tag = TYPE_I32, .nops = 0, .ops = NULL });
        case 64: return make_type(mod, (type_t) { .tag = TYPE_I64, .nops = 0, .ops = NULL });
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_u(mod_t* mod, size_t bitwidth) {
    switch (bitwidth) {
        case 8:  return make_type(mod, (type_t) { .tag = TYPE_U8,  .nops = 0, .ops = NULL });
        case 16: return make_type(mod, (type_t) { .tag = TYPE_U16, .nops = 0, .ops = NULL });
        case 32: return make_type(mod, (type_t) { .tag = TYPE_U32, .nops = 0, .ops = NULL });
        case 64: return make_type(mod, (type_t) { .tag = TYPE_U64, .nops = 0, .ops = NULL });
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_f(mod_t* mod, size_t bitwidth) {
    switch (bitwidth) {
        case 32: return make_type(mod, (type_t) { .tag = TYPE_F32, .nops = 0, .ops = NULL });
        case 64: return make_type(mod, (type_t) { .tag = TYPE_F64, .nops = 0, .ops = NULL });
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_tuple(mod_t* mod, size_t nops, const type_t** ops) {
    if (nops == 1) return ops[0];
    return make_type(mod, (type_t) { .tag = TYPE_TUPLE, .nops = nops, .ops = ops });
}

const type_t* type_fn(mod_t* mod, const type_t* from, const type_t* to) {
    const type_t* ops[] = { from, to };
    return make_type(mod, (type_t) { .tag = TYPE_FN, .nops = 2, .ops = ops });
}

static inline const node_t* make_node(mod_t* mod, const node_t node) {
    const node_t* lookup_ptr = &node;
    size_t index = htable_lookup(mod->nodes, &lookup_ptr);
    if (index != INVALID_INDEX)
        return ((const node_t**)mod->nodes->elems)[index];

    node_t* node_ptr = mpool_alloc(&mod->pool, sizeof(node_t));
    *node_ptr = node;
    if (node.nops > 0) {
        const node_t** node_ops = mpool_alloc(&mod->pool, sizeof(node_t*) * node.nops);
        for (size_t i = 0; i < node.nops; ++i) node_ops[i] = node.ops[i];
        node_ptr->ops = node_ops;
    }

    bool success = htable_insert(mod->nodes, &node_ptr);
    assert(success), (void)success;
    return node_ptr;
}

const node_t* node_undef(mod_t* mod, const type_t* type) {
    if (type->tag == TYPE_TUPLE) {
        // Resolve undef values for tuples on creation
        const node_t* ops[type->nops];
        for (size_t i = 0; i < type->nops; ++i) {
            ops[i] = node_undef(mod, type->ops[i]);
        }
        return node_tuple(mod, type->nops, ops, NULL);
    }
    return make_node(mod, (node_t) {
        .tag = NODE_UNDEF,
        .nops = 0,
        .uses = NULL,
        .ops  = NULL,
        .type = type,
        .loc = NULL
    });
}

const node_t* node_literal(mod_t* mod, const type_t* type, box_t value) {
    // Make sure padding bytes are zeroed
    box_t box;
    memset(&box, 0, sizeof(box_t));
    box = value;
    return make_node(mod, (node_t) {
        .tag = NODE_LITERAL,
        .nops = 0,
        .uses = NULL,
        .box = box,
        .type = type,
        .loc = NULL
    });
}

const node_t* node_tuple(mod_t* mod, size_t nops, const node_t** ops, const loc_t* loc) {
    if (nops == 1) return ops[0];
    const type_t* type_ops[nops];
    for (size_t i = 0; i < nops; ++i)
        type_ops[i] = ops[i]->type;
    return make_node(mod, (node_t) {
        .tag = NODE_TUPLE,
        .nops = nops,
        .uses = NULL,
        .ops  = ops,
        .type = type_tuple(mod, nops, type_ops),
        .loc  = loc
    });
}