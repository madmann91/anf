#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "anf.h"
#include "hash.h"

bool node_cmp(const void* ptr1, const void* ptr2) {
    const node_t* node1 = *(const node_t**)ptr1;
    const node_t* node2 = *(const node_t**)ptr2;
    if (node1->tag  != node2->tag  ||
        node1->nops != node2->nops ||
        node1->type != node2->type)
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
    h = hash_ptr(h, node->type);
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

bool type_is_i(const type_t* type) {
    switch (type->tag) {
        case TYPE_I1:  return true;
        case TYPE_I8:  return true;
        case TYPE_I16: return true;
        case TYPE_I32: return true;
        case TYPE_I64: return true;
        default:       return false;
    }
}

bool type_is_u(const type_t* type) {
    switch (type->tag) {
        case TYPE_U8:  return true;
        case TYPE_U16: return true;
        case TYPE_U32: return true;
        case TYPE_U64: return true;
        default:       return false;
    }
}

bool type_is_f(const type_t* type) {
    switch (type->tag) {
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

const type_t* type_i1 (mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I1,  .nops = 0, .ops = NULL }); }
const type_t* type_i8 (mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I8,  .nops = 0, .ops = NULL }); }
const type_t* type_i16(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I16, .nops = 0, .ops = NULL }); }
const type_t* type_i32(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I32, .nops = 0, .ops = NULL }); }
const type_t* type_i64(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I64, .nops = 0, .ops = NULL }); }
const type_t* type_u8 (mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U8,  .nops = 0, .ops = NULL }); }
const type_t* type_u16(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U16, .nops = 0, .ops = NULL }); }
const type_t* type_u32(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U32, .nops = 0, .ops = NULL }); }
const type_t* type_u64(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U64, .nops = 0, .ops = NULL }); }
const type_t* type_f32(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_F32, .nops = 0, .ops = NULL }); }
const type_t* type_f64(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_F64, .nops = 0, .ops = NULL }); }

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
        // Resolve undef values for tuples upon creation
        const node_t* ops[type->nops];
        for (size_t i = 0; i < type->nops; ++i) {
            ops[i] = node_undef(mod, type->ops[i]);
        }
        return node_tuple(mod, type->nops, ops, NULL);
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_UNDEF,
        .nops = 0,
        .uses = NULL,
        .ops  = NULL,
        .type = type,
        .loc  = NULL
    });
}

static inline const node_t* make_literal(mod_t* mod, const type_t* type, box_t value) {
    // Make sure padding bytes are zeroed
    box_t box;
    memset(&box, 0, sizeof(box_t));
    box = value;
    return make_node(mod, (node_t) {
        .tag  = NODE_LITERAL,
        .nops = 0,
        .uses = NULL,
        .box  = box,
        .type = type,
        .loc  = NULL
    });
}

const node_t* node_i1 (mod_t* mod, bool     value) { return make_literal(mod, type_i1(mod),  (box_t) { .i1  = value }); }
const node_t* node_i8 (mod_t* mod, int8_t   value) { return make_literal(mod, type_i8(mod),  (box_t) { .i8  = value }); }
const node_t* node_i16(mod_t* mod, int16_t  value) { return make_literal(mod, type_i16(mod), (box_t) { .i16 = value }); }
const node_t* node_i32(mod_t* mod, int32_t  value) { return make_literal(mod, type_i32(mod), (box_t) { .i32 = value }); }
const node_t* node_i64(mod_t* mod, int64_t  value) { return make_literal(mod, type_i64(mod), (box_t) { .i64 = value }); }
const node_t* node_u8 (mod_t* mod, uint8_t  value) { return make_literal(mod, type_u8(mod),  (box_t) { .u8  = value }); }
const node_t* node_u16(mod_t* mod, uint16_t value) { return make_literal(mod, type_u16(mod), (box_t) { .u16 = value }); }
const node_t* node_u32(mod_t* mod, uint32_t value) { return make_literal(mod, type_u32(mod), (box_t) { .u32 = value }); }
const node_t* node_u64(mod_t* mod, uint64_t value) { return make_literal(mod, type_u64(mod), (box_t) { .u64 = value }); }
const node_t* node_f32(mod_t* mod, float    value) { return make_literal(mod, type_f32(mod), (box_t) { .f32 = value }); }
const node_t* node_f64(mod_t* mod, double   value) { return make_literal(mod, type_f64(mod), (box_t) { .f64 = value }); }

const node_t* node_tuple(mod_t* mod, size_t nops, const node_t** ops, const loc_t* loc) {
    if (nops == 1) return ops[0];
    const type_t* type_ops[nops];
    for (size_t i = 0; i < nops; ++i)
        type_ops[i] = ops[i]->type;
    return make_node(mod, (node_t) {
        .tag  = NODE_TUPLE,
        .nops = nops,
        .uses = NULL,
        .ops  = ops,
        .type = type_tuple(mod, nops, type_ops),
        .loc  = loc
    });
}

const node_t* node_extract(mod_t* mod, const node_t* value, const node_t* index, const loc_t* loc) {
    assert(type_is_u(index->type));
    if (value->type->tag == TYPE_TUPLE) {
        assert(index->tag == NODE_LITERAL);
        assert(index->box.u64 < value->type->nops);
        if (value->tag == NODE_TUPLE)
            return value->ops[index->box.u64];
        const node_t* ops[] = { value, index };
        return make_node(mod, (node_t) {
            .tag  = NODE_EXTRACT,
            .nops = 2,
            .uses = NULL,
            .ops  = ops,
            .type = value->type->ops[index->box.u64],
            .loc  = loc
        });
    }
    assert(false);
    return NULL;
}

const node_t* node_insert(mod_t* mod, const node_t* value, const node_t* index, const node_t* elem, const loc_t* loc) {
    assert(type_is_u(index->type));
    if (value->type->tag == TYPE_TUPLE) {
        assert(index->tag == NODE_LITERAL);
        assert(index->box.u64 < value->type->nops);
        assert(elem->type == value->type->ops[index->box.u64]);
        if (value->tag == NODE_TUPLE) {
            const node_t* ops[value->nops];
            for (size_t i = 0; i < value->nops; ++i)
                ops[i] = value->ops[i];
            ops[index->box.u64] = elem;
            return node_tuple(mod, value->nops, ops, loc);
        }
        const node_t* ops[] = { value, index, elem };
        return make_node(mod, (node_t) {
            .tag  = NODE_INSERT,
            .nops = 3,
            .uses = NULL,
            .ops  = ops,
            .type = value->type,
            .loc  = loc
        });
    }
    assert(false);
    return NULL;
}

const node_t* node_select(mod_t* mod, const node_t* cond, const node_t* if_true, const node_t* if_false, const loc_t* loc) {
    assert(cond->type->tag == TYPE_I1);
    assert(if_true->type == if_false->type);
    if (cond->tag == NODE_LITERAL)
        return cond->box.i1 ? if_true : if_false;
    if (if_true == if_false)
        return if_true;
    const node_t* ops[] = { cond, if_true, if_false };
    return make_node(mod, (node_t) {
        .tag  = NODE_SELECT,
        .nops = 3,
        .uses = NULL,
        .ops  = ops,
        .type = if_true->type,
        .loc  = loc
    });
}

const node_t* node_bitcast(mod_t* mod, const node_t* value, const type_t* type, const loc_t* loc) {
    assert(type_is_prim(type));
    assert(type_is_prim(value->type));
    assert(type_bitwidth(value->type) == type_bitwidth(type));
    while (value->tag == NODE_BITCAST && value->type != type)
        value = value->ops[0];
    if (value->type == type)
        return value;
    if (value->tag == NODE_LITERAL)
        return make_literal(mod, type, value->box);
    if (value->tag == NODE_UNDEF)
        return node_undef(mod, type);
    return make_node(mod, (node_t) {
        .tag  = NODE_BITCAST,
        .nops = 1,
        .uses = NULL,
        .ops  = &value,
        .type = type,
        .loc  = loc
    });
}