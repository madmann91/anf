#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
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
    mod->pool  = mpool_create(4096);
    mod->nodes = node_set_create(256);
    mod->types = type_set_create(64);
    mod->fns   = node_vec_create(64);

    mod->commutative_fp  = false;
    mod->distributive_fp = false;

    return mod;
}

void mod_destroy(mod_t* mod) {
    mpool_destroy(mod->pool);
    node_set_destroy(&mod->nodes);
    type_set_destroy(&mod->types);
    node_vec_destroy(&mod->fns);
    free(mod);
}

bool mod_is_commutative(const mod_t* mod, uint32_t tag, const type_t* type) {
    switch (tag) {
        case NODE_ADD: return mod->commutative_fp || !type_is_f(type);
        case NODE_MUL: return mod->commutative_fp || !type_is_f(type);
        case NODE_AND: return true;
        case NODE_OR:  return true;
        case NODE_XOR: return true;
        default: return false;
    }
}

bool mod_is_distributive(const mod_t* mod, uint32_t tag1, uint32_t tag2, const type_t* type) {
    switch (tag1) {
        case NODE_MUL: return (tag2 == NODE_ADD || tag2 == NODE_SUB) &&
                              (mod->distributive_fp || !type_is_f(type));
        case NODE_AND: return tag2 == NODE_OR;
        case NODE_OR:  return tag2 == NODE_AND;
        default: return false;
    }
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
    const type_t** lookup = type_set_lookup(&mod->types, &type);
    if (lookup)
        return *lookup;

    type_t* type_ptr = mpool_alloc(&mod->pool, sizeof(type_t));
    *type_ptr = type;
    if (type.nops > 0) {
        const type_t** type_ops = mpool_alloc(&mod->pool, sizeof(type_t*) * type.nops);
        for (size_t i = 0; i < type.nops; ++i) type_ops[i] = type.ops[i];
        type_ptr->ops = type_ops;
    }

    bool success = type_set_insert(&mod->types, type_ptr);
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

const type_t* type_array(mod_t* mod, const type_t* elem_type) {
    return make_type(mod, (type_t) { .tag = TYPE_ARRAY, .nops = 1, .ops = &elem_type });
}

const type_t* type_fn(mod_t* mod, const type_t* from, const type_t* to) {
    const type_t* ops[] = { from, to };
    return make_type(mod, (type_t) { .tag = TYPE_FN, .nops = 2, .ops = ops });
}

static inline void register_use(mod_t* mod, size_t index, const node_t* used, const node_t* user) {
    use_t* use = mpool_alloc(&mod->pool, sizeof(use_t));
    use->index = index;
    use->user  = user;
    use->next  = used->uses;
    ((node_t*)used)->uses = use;
}

static inline void unregister_use(size_t index, const node_t* used, const node_t* user) {
    use_t* use = used->uses;
    use_t** prev = &((node_t*)used)->uses;
    while (use) {
        if (use->index == index && use->user  == user)
            break;
        prev = &use->next;
        use  = use->next;
    }
    assert(use);
    *prev = use->next;
}

static inline const node_t* make_node(mod_t* mod, const node_t node) {
    const node_t** lookup = node_set_lookup(&mod->nodes, &node);
    if (lookup)
        return *lookup;

    node_t* node_ptr = mpool_alloc(&mod->pool, sizeof(node_t));
    *node_ptr = node;
    if (node.nops > 0) {
        const node_t** node_ops = mpool_alloc(&mod->pool, sizeof(node_t*) * node.nops);
        for (size_t i = 0; i < node.nops; ++i) {
            register_use(mod, i, node.ops[i], node_ptr);
            node_ops[i] = node.ops[i];
        }
        node_ptr->ops = node_ops;
    }

    bool success = node_set_insert(&mod->nodes, node_ptr);
    assert(success), (void)success;
    return node_ptr;
}

const node_t* node_undef(mod_t* mod, const type_t* type) {
    if (type->tag == TYPE_TUPLE) {
        // Resolve undef values for tuples upon creation
        const node_t* ops[type->nops];
        for (size_t i = 0; i < type->nops; ++i)
            ops[i] = node_undef(mod, type->ops[i]);
        return node_tuple(mod, type->nops, ops, NULL);
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_UNDEF,
        .nops = 0,
        .ops  = NULL,
        .type = type,
        .dbg  = NULL
    });
}

static inline const node_t* make_literal(mod_t* mod, const type_t* type, box_t value) {
    return make_node(mod, (node_t) {
        .tag  = NODE_LITERAL,
        .nops = 0,
        .box  = value,
        .type = type,
        .dbg  = NULL
    });
}

bool node_is_zero(const node_t* node) {
    if (node->tag != NODE_LITERAL)
        return false;
    switch (node->type->tag) {
        case TYPE_I1:  return node->box.i1  == false;
        case TYPE_I8:  return node->box.i8  == 0;
        case TYPE_I16: return node->box.i16 == 0;
        case TYPE_I32: return node->box.i32 == 0;
        case TYPE_I64: return node->box.i64 == 0;
        case TYPE_U8:  return node->box.u8  == 0;
        case TYPE_U16: return node->box.u16 == 0;
        case TYPE_U32: return node->box.u32 == 0;
        case TYPE_U64: return node->box.u64 == 0;
        case TYPE_F32: return node->box.f32 == 0.0f;
        case TYPE_F64: return node->box.f64 == 0.0;
        default:       return false;
    }
}

bool node_is_one(const node_t* node) {
    if (node->tag != NODE_LITERAL)
        return false;
    switch (node->type->tag) {
        case TYPE_I1:  return node->box.i1  == true;
        case TYPE_I8:  return node->box.i8  == 1;
        case TYPE_I16: return node->box.i16 == 1;
        case TYPE_I32: return node->box.i32 == 1;
        case TYPE_I64: return node->box.i64 == 1;
        case TYPE_U8:  return node->box.u8  == 1;
        case TYPE_U16: return node->box.u16 == 1;
        case TYPE_U32: return node->box.u32 == 1;
        case TYPE_U64: return node->box.u64 == 1;
        case TYPE_F32: return node->box.f32 == 1.0f;
        case TYPE_F64: return node->box.f64 == 1.0;
        default:       return false;
    }
}

bool node_is_all_ones(const node_t* node) {
    if (node->tag != NODE_LITERAL)
        return false;
    switch (node->type->tag) {
        case TYPE_I1:  return node->box.i1  == true;
        case TYPE_I8:  return node->box.i8  == -1;
        case TYPE_I16: return node->box.i16 == -1;
        case TYPE_I32: return node->box.i32 == -1;
        case TYPE_I64: return node->box.i64 == -1;
        case TYPE_U8:  return node->box.u8  == 0xFFu;
        case TYPE_U16: return node->box.u16 == 0xFFFFu;
        case TYPE_U32: return node->box.u32 == 0xFFFFFFFFu;
        case TYPE_U64: return node->box.u64 == 0xFFFFFFFFFFFFFFFFull;
        default:       return false;
    }
}

const node_t* node_zero(mod_t* mod, const type_t* type) {
    assert(type_is_prim(type));
    switch (type->tag) {
        case TYPE_I1:  return node_i1 (mod, false);
        case TYPE_I8:  return node_i8 (mod, 0);
        case TYPE_I16: return node_i16(mod, 0);
        case TYPE_I32: return node_i32(mod, 0);
        case TYPE_I64: return node_i64(mod, 0);
        case TYPE_U8:  return node_u8 (mod, 0);
        case TYPE_U16: return node_u16(mod, 0);
        case TYPE_U32: return node_u32(mod, 0);
        case TYPE_U64: return node_u64(mod, 0);
        case TYPE_F32: return node_f32(mod, 0.0f);
        case TYPE_F64: return node_f64(mod, 0.0);
        default:       return NULL;
    }
}

const node_t* node_one(mod_t* mod, const type_t* type) {
    assert(type_is_prim(type));
    switch (type->tag) {
        case TYPE_I1:  return node_i1 (mod, true);
        case TYPE_I8:  return node_i8 (mod, 1);
        case TYPE_I16: return node_i16(mod, 1);
        case TYPE_I32: return node_i32(mod, 1);
        case TYPE_I64: return node_i64(mod, 1);
        case TYPE_U8:  return node_u8 (mod, 1);
        case TYPE_U16: return node_u16(mod, 1);
        case TYPE_U32: return node_u32(mod, 1);
        case TYPE_U64: return node_u64(mod, 1);
        case TYPE_F32: return node_f32(mod, 1.0f);
        case TYPE_F64: return node_f64(mod, 1.0);
        default:       return NULL;
    }
}

const node_t* node_all_ones(mod_t* mod, const type_t* type) {
    assert(type_is_prim(type));
    assert(!type_is_f(type));
    switch (type->tag) {
        case TYPE_I1:  return node_i1 (mod, true);
        case TYPE_I8:  return node_i8 (mod, -1);
        case TYPE_I16: return node_i16(mod, -1);
        case TYPE_I32: return node_i32(mod, -1);
        case TYPE_I64: return node_i64(mod, -1);
        case TYPE_U8:  return node_u8 (mod, 0xFFu);
        case TYPE_U16: return node_u16(mod, 0xFFFFu);
        case TYPE_U32: return node_u32(mod, 0xFFFFFFFFu);
        case TYPE_U64: return node_u64(mod, 0xFFFFFFFFFFFFFFFFull);
        default:       return NULL;
    }
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

const node_t* node_tuple(mod_t* mod, size_t nops, const node_t** ops, const dbg_t* dbg) {
    if (nops == 1) return ops[0];
    const type_t* type_ops[nops];
    for (size_t i = 0; i < nops; ++i)
        type_ops[i] = ops[i]->type;
    return make_node(mod, (node_t) {
        .tag  = NODE_TUPLE,
        .nops = nops,
        .ops  = ops,
        .type = type_tuple(mod, nops, type_ops),
        .dbg  = dbg
    });
}

const node_t* node_array(mod_t* mod, size_t nops, const node_t** ops, const dbg_t* dbg) {
    const type_t* elem_type = ops[0]->type;
#ifndef NDEBUG
    for (size_t i = 1; i < nops; ++i)
        assert(ops[i]->type == elem_type);
#endif
    return make_node(mod, (node_t) {
        .tag  = NODE_ARRAY,
        .nops = nops,
        .ops  = ops,
        .type = type_array(mod, elem_type),
        .dbg  = dbg
    });
}

const node_t* node_extract(mod_t* mod, const node_t* value, const node_t* index, const dbg_t* dbg) {
    assert(value->type->tag == TYPE_TUPLE || value->type->tag == TYPE_ARRAY);
    assert(type_is_u(index->type) || type_is_i(index->type));
    const type_t* elem_type = NULL;
    if (value->type->tag == TYPE_TUPLE) {
        assert(index->tag == NODE_LITERAL);
        assert(index->box.u64 < value->type->nops);
        elem_type = value->type->ops[index->box.u64];
        if (value->tag == NODE_TUPLE)
            return value->ops[index->box.u64];
    } else if (value->type->tag == TYPE_ARRAY) {
        elem_type = value->type->ops[0];
        if (value->tag == NODE_ARRAY) {
            assert(index->box.u64 < value->nops);
            return value->ops[index->box.u64];
        } else if (value->tag == NODE_UNDEF) {
            return node_undef(mod, elem_type);
        }
    }
    const node_t* ops[] = { value, index };
    return make_node(mod, (node_t) {
        .tag  = NODE_EXTRACT,
        .nops = 2,
        .ops  = ops,
        .type = elem_type,
        .dbg  = dbg
    });
    return NULL;
}

const node_t* node_insert(mod_t* mod, const node_t* value, const node_t* index, const node_t* elem, const dbg_t* dbg) {
    assert(value->type->tag == TYPE_TUPLE || value->type->tag == TYPE_ARRAY);
    assert(type_is_u(index->type) || type_is_i(index->type));
    if (value->type->tag == TYPE_TUPLE) {
        assert(index->tag == NODE_LITERAL);
        assert(index->box.u64 < value->type->nops);
        assert(elem->type == value->type->ops[index->box.u64]);
        if (value->tag == NODE_TUPLE) {
            const node_t* ops[value->nops];
            for (size_t i = 0; i < value->nops; ++i)
                ops[i] = value->ops[i];
            ops[index->box.u64] = elem;
            return node_tuple(mod, value->nops, ops, dbg);
        }
    } else if (value->type->tag == TYPE_ARRAY) {
        assert(elem->type == value->type->ops[0]);
        if (value->tag == NODE_ARRAY && index->tag == NODE_LITERAL) {
            assert(index->box.u64 < value->nops);
            const node_t* ops[value->nops];
            for (size_t i = 0; i < value->nops; ++i)
                ops[i] = value->ops[i];
            ops[index->box.u64] = elem;
            return node_array(mod, value->nops, ops, dbg);
        } else if (value->tag == NODE_UNDEF) {
            return value;
        }
    }
    const node_t* ops[] = { value, index, elem };
    return make_node(mod, (node_t) {
        .tag  = NODE_INSERT,
        .nops = 3,
        .ops  = ops,
        .type = value->type,
        .dbg  = dbg
    });
}

const node_t* node_bitcast(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
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
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

#define CMPOP_I1(op, res, left, right) \
    case TYPE_I1:  res  = left.i1  op right.i1;  break;

#define CMPOP_I(op, res, left, right) \
    case TYPE_I8:  res = left.i8  op right.i8;  break; \
    case TYPE_I16: res = left.i16 op right.i16; break; \
    case TYPE_I32: res = left.i32 op right.i32; break; \
    case TYPE_I64: res = left.i64 op right.i64; break;

#define CMPOP_U(op, res, left, right) \
    case TYPE_U8:  res = left.u8  op right.u8;  break; \
    case TYPE_U16: res = left.u16 op right.u16; break; \
    case TYPE_U32: res = left.u32 op right.u32; break; \
    case TYPE_U64: res = left.u64 op right.u64; break;

#define CMPOP_F(op, res, left, right) \
    case TYPE_F32: res = left.f32 op right.f32; break; \
    case TYPE_F64: res = left.f64 op right.f64; break;

#define CMPOP_IUF(op, res, left, right) \
    CMPOP_I(op, res, left, right) \
    CMPOP_U(op, res, left, right) \
    CMPOP_F(op, res, left, right)

#define CMPOP_I1IUF(op, res, left, right) \
    CMPOP_I1(op, res, left, right) \
    CMPOP_IUF(op, res, left, right)

#define CMPOP(tag, ...) \
    switch (tag) { \
        __VA_ARGS__ \
        default: \
            assert(false); \
            break; \
    }

static inline const node_t* make_cmpop(mod_t* mod, uint32_t tag, const node_t* left, const node_t* right, const dbg_t* dbg) {
    assert(left->type == right->type);
    assert(type_is_prim(left->type));
    assert(tag == NODE_CMPEQ || left->type->tag != TYPE_I1);

    if (left->tag == NODE_LITERAL && right->tag == NODE_LITERAL) {
        bool res = false;
        switch (tag) {
            case NODE_CMPGT: CMPOP(left->type->tag, CMPOP_IUF  (>,  res, left->box, right->box)) break;
            case NODE_CMPLT: CMPOP(left->type->tag, CMPOP_IUF  (<,  res, left->box, right->box)) break;
            case NODE_CMPEQ: CMPOP(left->type->tag, CMPOP_I1IUF(==, res, left->box, right->box)) break;
            default:
                assert(false);
                break;
        }
        return node_i1(mod, res);
    }

    if (left == right) {
        if (tag == NODE_CMPGT || tag == NODE_CMPLT) return node_i1(mod, false);
        if (tag == NODE_CMPEQ) return node_i1(mod, true);
    }
    if (tag == NODE_CMPLT && type_is_u(left->type) && node_is_zero(right))
        return node_i1(mod, false);

    const node_t* ops[] = { left, right };
    return make_node(mod, (node_t) {
        .tag = tag,
        .nops = 2,
        .ops  = ops,
        .type = type_i1(mod),
        .dbg  = dbg
    });
}

#define NODE_CMPOP(name, tag) \
    const node_t* node_##name(mod_t* mod, const node_t* left, const node_t* right, const dbg_t* dbg) { \
        return make_cmpop(mod, tag, left, right, dbg); \
    }

NODE_CMPOP(cmpgt, NODE_CMPGT)
NODE_CMPOP(cmplt, NODE_CMPLT)
NODE_CMPOP(cmpeq, NODE_CMPEQ)

#define BINOP_I1(op, res, left, right) \
    case TYPE_I1:  res.i1  = left.i1  op right.i1;  break;

#define BINOP_I(op, res, left, right) \
    case TYPE_I8:  res.i8  = left.i8  op right.i8;  break; \
    case TYPE_I16: res.i16 = left.i16 op right.i16; break; \
    case TYPE_I32: res.i32 = left.i32 op right.i32; break; \
    case TYPE_I64: res.i64 = left.i64 op right.i64; break;

#define BINOP_U(op, res, left, right) \
    case TYPE_U8:  res.u8  = left.u8  op right.u8;  break; \
    case TYPE_U16: res.u16 = left.u16 op right.u16; break; \
    case TYPE_U32: res.u32 = left.u32 op right.u32; break; \
    case TYPE_U64: res.u64 = left.u64 op right.u64; break;

#define BINOP_F(op, res, left, right) \
    case TYPE_F32: res.f32 = left.f32 op right.f32; break; \
    case TYPE_F64: res.f64 = left.f64 op right.f64; break;

#define BINOP_IUF(op, res, left, right) \
    BINOP_I(op, res, left, right) \
    BINOP_U(op, res, left, right) \
    BINOP_F(op, res, left, right)

#define BINOP_IU(op, res, left, right) \
    BINOP_I(op, res, left, right) \
    BINOP_U(op, res, left, right)

#define BINOP_I1IU(op, res, left, right) \
    BINOP_I1(op, res, left, right) \
    BINOP_IU(op, res, left, right)

#define BINOP(tag, ...) \
    switch (tag) { \
        __VA_ARGS__ \
        default: \
            assert(false); \
            break; \
    }

static inline const node_t* make_binop(mod_t* mod, uint32_t tag, const node_t* left, const node_t* right, const dbg_t* dbg) {
    assert(left->type == right->type);
    assert(type_is_prim(left->type));
    bool is_shft    = tag == NODE_LSHFT || tag == NODE_RSHFT;
    bool is_bitwise = tag == NODE_AND || tag == NODE_OR || tag == NODE_XOR;
    (void)is_shft;
    assert(is_bitwise || left->type->tag != TYPE_I1);
    assert((!is_bitwise && !is_shft) || !type_is_f(left->type));
    assert(tag != NODE_MOD || !type_is_f(left->type));

    // Constant folding
    if (left->tag == NODE_LITERAL && right->tag == NODE_LITERAL) {
        box_t res;
        memset(&res, 0, sizeof(box_t));
        switch (tag) {
            case NODE_ADD:   BINOP(left->type->tag, BINOP_IUF (+,  res, left->box, right->box)) break;
            case NODE_SUB:   BINOP(left->type->tag, BINOP_IUF (-,  res, left->box, right->box)) break;
            case NODE_MUL:   BINOP(left->type->tag, BINOP_IUF (*,  res, left->box, right->box)) break;
            case NODE_DIV:   BINOP(left->type->tag, BINOP_IUF (/,  res, left->box, right->box)) break;
            case NODE_MOD:   BINOP(left->type->tag, BINOP_IU  (%,  res, left->box, right->box)) break;
            case NODE_AND:   BINOP(left->type->tag, BINOP_I1IU(&,  res, left->box, right->box)) break;
            case NODE_OR:    BINOP(left->type->tag, BINOP_I1IU(|,  res, left->box, right->box)) break;
            case NODE_XOR:   BINOP(left->type->tag, BINOP_I1IU(^,  res, left->box, right->box)) break;
            case NODE_LSHFT: BINOP(left->type->tag, BINOP_IU  (<<, res, left->box, right->box)) break;
            case NODE_RSHFT: BINOP(left->type->tag, BINOP_IU  (>>, res, left->box, right->box)) break;
            default:
                assert(false);
                break;
        }
        return make_literal(mod, left->type, res);
    }

    if (left->tag  == NODE_UNDEF) return left;
    if (right->tag == NODE_UNDEF) return right;

    // Literals always go to the left in commutative expressions
    bool is_commutative = mod_is_commutative(mod, tag, left->type);
    if (right->tag == NODE_LITERAL && is_commutative) {
        const node_t* tmp = left;
        left = right;
        right = tmp;
    }

    // Simplification rules
    if (node_is_zero(left)) {
        if (tag == NODE_ADD || tag == NODE_OR)  return right;
        if (tag == NODE_MUL || tag == NODE_AND) return node_zero(mod, left->type);
        if (tag == NODE_XOR) return right;
    }
    if (node_is_all_ones(left)) {
        if (tag == NODE_AND) return right;
        if (tag == NODE_OR)  return left;
    }
    if (tag == NODE_MUL && node_is_one(left))
        return right;
    if (node_is_zero(right)) {
        if (tag == NODE_LSHFT || tag == NODE_RSHFT || tag == NODE_SUB) return left;
        assert(tag != NODE_DIV && tag != NODE_MOD);
    }
    if (node_is_one(right)) {
        if (tag == NODE_DIV) return left;
        if (tag == NODE_MOD) return node_zero(mod, left->type);
    }
    if (left == right) {
        if (tag == NODE_AND || tag == NODE_OR)  return left;
        if (tag == NODE_XOR || tag == NODE_MOD || tag == NODE_SUB) return node_zero(mod, left->type);
        if (tag == NODE_DIV) return node_one(mod, left->type);
    }
    if (tag == NODE_AND) {
        if (right->tag == NODE_OR && (right->ops[0] == left || right->ops[1] == left))
            return left;
        if (left->tag == NODE_OR && (left->ops[0] == right || left->ops[1] == right))
            return right;
    }
    if (tag == NODE_OR) {
        if (right->tag == NODE_AND && (right->ops[0] == left || right->ops[1] == left))
            return left;
        if (left->tag == NODE_AND && (left->ops[0] == right || left->ops[1] == right))
            return right;
    }
    if (tag == NODE_XOR) {
        if (right->tag == NODE_XOR) {
            if (right->ops[0] == left) return right->ops[1];
            if (right->ops[1] == left) return right->ops[0];
        } else if (left->tag == NODE_XOR) {
            if (left->ops[0] == right) return left->ops[1];
            if (left->ops[1] == right) return left->ops[0];
        }
    }
    bool left_factorizable = mod_is_distributive(mod, right->tag, tag, left->type);
    if (left_factorizable && right->ops[0]->tag == NODE_LITERAL && right->ops[1] == left) {
        const node_t* one = is_bitwise ? node_all_ones(mod, left->type) : node_one(mod, left->type);
        const node_t* K   = make_binop(mod, tag, one, right->ops[0], dbg);
        assert(K->tag == NODE_LITERAL);
        return make_binop(mod, right->tag, K, left, dbg);
    }
    bool right_factorizable = mod_is_distributive(mod, left->tag, tag, left->type);
    if (right_factorizable && left->ops[0]->tag == NODE_LITERAL && left->ops[1] == right) {
        const node_t* one = is_bitwise ? node_all_ones(mod, left->type) : node_one(mod, left->type);
        const node_t* K   = make_binop(mod, tag, left->ops[0], one, dbg);
        assert(K->tag == NODE_LITERAL);
        return make_binop(mod, left->tag, K, right, dbg);
    }
    bool both_factorizable = left_factorizable & right_factorizable;
    if (both_factorizable) {
        const node_t* l1 = left->ops[0];
        const node_t* l2 = left->ops[1];
        const node_t* r1 = right->ops[0];
        const node_t* r2 = right->ops[1];
        assert(left->tag == right->tag);
        bool inner_commutative = mod_is_commutative(mod, left->tag, left->type);
        if (inner_commutative && l1 == r2) { const node_t* tmp = r2; r2 = r1; r1 = tmp; }
        if (inner_commutative && l2 == r1) { const node_t* tmp = l2; l2 = l1; l1 = tmp; }
        if (l1 == r1)
            return make_binop(mod, left->tag, l1, make_binop(mod, tag, l2, r2, dbg), dbg);
        if (l2 == r2)
            return make_binop(mod, left->tag, make_binop(mod, tag, l1, r1, dbg), l2, dbg);
    }

    const node_t* ops[] = { left, right };
    return make_node(mod, (node_t) {
        .tag = tag,
        .nops = 2,
        .ops  = ops,
        .type = left->type,
        .dbg  = dbg
    });
}

#define NODE_BINOP(name, tag) \
    const node_t* node_##name(mod_t* mod, const node_t* left, const node_t* right, const dbg_t* dbg) { \
        return make_binop(mod, tag, left, right, dbg); \
    }

NODE_BINOP(add, NODE_ADD)
NODE_BINOP(sub, NODE_SUB)
NODE_BINOP(mul, NODE_MUL)
NODE_BINOP(div, NODE_DIV)
NODE_BINOP(mod, NODE_MOD)
NODE_BINOP(and, NODE_AND)
NODE_BINOP(or,  NODE_OR)
NODE_BINOP(xor, NODE_XOR)
NODE_BINOP(lshft, NODE_LSHFT)
NODE_BINOP(rshft, NODE_RSHFT)

const node_t* node_if(mod_t* mod, const node_t* cond, const node_t* if_true, const node_t* if_false, const dbg_t* dbg) {
    assert(cond->type->tag == TYPE_I1);
    assert(if_true->type == if_false->type);
    if (cond->tag == NODE_LITERAL)
        return cond->box.i1 ? if_true : if_false;
    if (if_true == if_false)
        return if_true;
    const node_t* ops[] = { cond, if_true, if_false };
    return make_node(mod, (node_t) {
        .tag  = NODE_IF,
        .nops = 3,
        .ops  = ops,
        .type = if_true->type,
        .dbg  = dbg
    });
}

void node_bind(mod_t* mod, const node_t* fn, const node_t* call) {
    assert(fn->tag == NODE_FN);
    assert(fn->type->ops[1] == call->type);
    node_t* node = (node_t*)fn;
    if (node->ops[0])
        unregister_use(0, node->ops[0], node);
    node->ops[0] = call;
    register_use(mod, 0, node->ops[0], node);
}

void node_run_if(mod_t* mod, const node_t* fn, const node_t* cond) {
    assert(fn->tag == NODE_FN);
    assert(cond->type->tag == TYPE_I1);
    node_t* node = (node_t*)fn;
    if (node->ops[1])
        unregister_use(1, node->ops[1], node);
    node->ops[1] = cond;
    register_use(mod, 1, node->ops[1], node);
}

const node_t* node_fn(mod_t* mod, const type_t* type, const dbg_t* dbg) {
    assert(type->tag == TYPE_FN);
    node_t* node = mpool_alloc(&mod->pool, sizeof(node_t));
    const node_t** ops = mpool_alloc(&mod->pool, sizeof(node_t*) * 2);
    ops[0] = NULL;
    ops[1] = NULL;
    *node = (node_t) {
        .tag  = NODE_FN,
        .nops = 2,
        .ops  = ops,
        .type = type,
        .dbg  = dbg
    };
    node_vec_push(&mod->fns, node);
    return node;
}

const node_t* node_param(mod_t* mod, const node_t* node, const dbg_t* dbg) {
    assert(node->tag == NODE_FN);
    return make_node(mod, (node_t) {
        .tag  = NODE_PARAM,
        .nops = 1,
        .ops  = &node,
        .type = node->type->ops[0],
        .dbg  = dbg
    });
}

const node_t* node_known(mod_t* mod, const node_t* node, const dbg_t* dbg) {
    if (node->tag == NODE_LITERAL || node->tag == NODE_FN)
        return node_i1(mod, true);
    if (node->tag == NODE_TUPLE || node->tag == NODE_ARRAY) {
        const node_t* res = node_i1(mod, true);
        for (size_t i = 0; i < node->nops; ++i)
            res = node_and(mod, res, node_known(mod, node->ops[i], dbg), dbg);
        return res;
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_KNOWN,
        .nops = 1,
        .ops  = &node,
        .type = type_i1(mod),
        .dbg  = dbg
    });
}

const node_t* node_app(mod_t* mod, const node_t* fn, const node_t* arg, const dbg_t* dbg) {
    assert(fn->type->tag == TYPE_FN);
    const node_t* ops[] = { fn, arg };
    return make_node(mod, (node_t) {
        .tag  = NODE_APP,
        .nops = 2,
        .ops  = ops,
        .type = fn->type->ops[1],
        .dbg  = dbg
    });
}

const type_t* type_rebuild(mod_t* mod, const type_t* type, const type_t** ops) {
    switch (type->tag) {
        case TYPE_I1:    return type_i1(mod);
        case TYPE_I8:    return type_i8(mod);
        case TYPE_I16:   return type_i16(mod);
        case TYPE_I32:   return type_i32(mod);
        case TYPE_I64:   return type_i64(mod);
        case TYPE_U8:    return type_u8(mod);
        case TYPE_U16:   return type_u16(mod);
        case TYPE_U32:   return type_u32(mod);
        case TYPE_U64:   return type_u64(mod);
        case TYPE_F32:   return type_f32(mod);
        case TYPE_F64:   return type_f64(mod);
        case TYPE_TUPLE: return type_tuple(mod, type->nops, ops);
        case TYPE_ARRAY: return type_array(mod, ops[0]);
        case TYPE_FN:    return type_fn(mod, ops[0], ops[1]);
        default:
            assert(false);
            return NULL;
    }
}

const node_t* node_rebuild(mod_t* mod, const node_t* node, const node_t** ops, const type_t* type) {
    switch (node->tag) {
        case NODE_LITERAL: return make_literal(mod, type, node->box);
        case NODE_UNDEF:   return node_undef(mod, type);
        case NODE_TUPLE:   return node_tuple(mod, node->nops, ops, node->dbg);
        case NODE_ARRAY:   return node_array(mod, node->nops, ops, node->dbg);
        case NODE_EXTRACT: return node_extract(mod, ops[0], ops[1], node->dbg);
        case NODE_INSERT:  return node_insert(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_BITCAST: return node_bitcast(mod, ops[0], type, node->dbg);
        case NODE_CMPLT:   return node_cmplt(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPGT:   return node_cmpgt(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPEQ:   return node_cmpeq(mod, ops[0], ops[1], node->dbg);
        case NODE_ADD:     return node_add(mod, ops[0], ops[1], node->dbg);
        case NODE_SUB:     return node_sub(mod, ops[0], ops[1], node->dbg);
        case NODE_MUL:     return node_mul(mod, ops[0], ops[1], node->dbg);
        case NODE_DIV:     return node_div(mod, ops[0], ops[1], node->dbg);
        case NODE_MOD:     return node_mod(mod, ops[0], ops[1], node->dbg);
        case NODE_AND:     return node_and(mod, ops[0], ops[1], node->dbg);
        case NODE_OR:      return node_or(mod, ops[0], ops[1], node->dbg);
        case NODE_XOR:     return node_xor(mod, ops[0], ops[1], node->dbg);
        case NODE_LSHFT:   return node_lshft(mod, ops[0], ops[1], node->dbg);
        case NODE_RSHFT:   return node_rshft(mod, ops[0], ops[1], node->dbg);
        case NODE_IF:      return node_if(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_PARAM:   return node_param(mod, ops[0], node->dbg);
        case NODE_APP:     return node_app(mod, ops[0], ops[1], node->dbg);
        case NODE_KNOWN:   return node_known(mod, ops[0], node->dbg);
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_rewrite(mod_t* mod, const type_t* type, type2type_t* new_types) {
    const type_t* new_ops[type->nops];
    for (size_t i = 0; i < type->nops; ++i) {
        const type_t* op = type->ops[i];
        const type_t** found = type2type_lookup(new_types, op);
        new_ops[i] = found ? *found : type_rewrite(mod, op, new_types);
    }

    const type_t* new_type = type_rebuild(mod, type, new_ops);
    type2type_insert(new_types, type, new_type);
    return new_type;
}

const node_t* node_rewrite(mod_t* mod, const node_t* node, node2node_t* new_nodes, type2type_t* new_types) {
    const node_t* new_ops[node->nops];
    for (size_t i = 0; i < node->nops; ++i) {
        const node_t* op = node->ops[i];
        const node_t** found = node2node_lookup(new_nodes, op);
        new_ops[i] = found ? *found : node_rewrite(mod, op, new_nodes, new_types);
    }

    const node_t* new_node = node_rebuild(mod, node, new_ops, new_types ? type_rewrite(mod, node->type, new_types) : node->type);
    node2node_insert(new_nodes, node, new_node);
    return new_node;
}

void node_replace(const node_t* node, const node_t* with) {
    assert(node->type == with->type);
    ((node_t*)node)->rep = with;
}

const use_t* use_find(const use_t* use, size_t index, const node_t* user) {
    while (use) {
        if ((index == INVALID_INDEX || use->index == index) &&
            (user  == NULL || use->user == user))
            return use;
        use = use->next;
    }
    return NULL;
}

size_t node_count_uses(const node_t* node) {
    size_t i = 0;
    const use_t* use = node->uses;
    while (use) {
        i++;
        use = use->next;
    }
    return i;
}

void type_print(const type_t* type, bool colorize) {
    const char* prefix = colorize ? "\33[;34;1m" : "";
    const char* suffix = colorize ? "\33[0m"     : "";
    switch (type->tag) {
        case TYPE_I1:  printf("%si1%s",  prefix, suffix); break;
        case TYPE_I8:  printf("%si8%s",  prefix, suffix); break;
        case TYPE_I16: printf("%si16%s", prefix, suffix); break;
        case TYPE_I32: printf("%si32%s", prefix, suffix); break;
        case TYPE_I64: printf("%si64%s", prefix, suffix); break;
        case TYPE_U8:  printf("%su8%s",  prefix, suffix); break;
        case TYPE_U16: printf("%su16%s", prefix, suffix); break;
        case TYPE_U32: printf("%su32%s", prefix, suffix); break;
        case TYPE_U64: printf("%su64%s", prefix, suffix); break;
        case TYPE_F32: printf("%sf32%s", prefix, suffix); break;
        case TYPE_F64: printf("%sf64%s", prefix, suffix); break;
        case TYPE_TUPLE:
            printf("(");
            for (size_t i = 0; i < type->nops; ++i) {
                type_print(type->ops[i], colorize);
                if (i != type->nops - 1)
                    printf(", ");
            }
            printf(")");
            break;
        case TYPE_ARRAY:
            printf("[");
            type_print(type->ops[0], colorize);
            printf("]");
            break;
        case TYPE_FN:
            if (type->ops[0]->tag == TYPE_FN) printf("(");
            type_print(type->ops[0], colorize);
            if (type->ops[0]->tag == TYPE_FN) printf(")");
            printf(" -> ");
            type_print(type->ops[1], colorize);
            break;
        default:
            assert(false);
            break;
    }
}

static inline void node_print_name(const node_t* node, bool colorize) {
    const char* prefix = colorize ? "\33[;33m"   : "";
    const char* suffix = colorize ? "\33[0m"     : "";
    if (node->dbg && strlen(node->dbg->name) > 0)
        printf("<%s : %s%"PRIxPTR"%s>", node->dbg->name, prefix, (uintptr_t)node, suffix);
    else
        printf("<%s%"PRIxPTR"%s>", prefix, (uintptr_t)node, suffix);
}

void node_print(const node_t* node, bool colorize) {
    const char* tprefix = colorize ? "\33[;34;1m" : "";
    const char* nprefix = colorize ? "\33[;36;1m" : "";
    const char* suffix  = colorize ? "\33[0m"     : "";
    if (node->tag == NODE_LITERAL) {
        switch (node->type->tag) {
            case TYPE_I1:  printf("%si1%s %s", tprefix, suffix, node->box.i1 ? "true" : "false"); break;
            case TYPE_I8:  printf("%si8%s  %"PRIi8,  tprefix, suffix, node->box.i8);  break;
            case TYPE_I16: printf("%si16%s %"PRIi16, tprefix, suffix, node->box.i16); break;
            case TYPE_I32: printf("%si32%s %"PRIi32, tprefix, suffix, node->box.i32); break;
            case TYPE_I64: printf("%si64%s %"PRIi64, tprefix, suffix, node->box.i64); break;
            case TYPE_U8:  printf("%su8%s  %"PRIu8,  tprefix, suffix, node->box.u8);  break;
            case TYPE_U16: printf("%su16%s %"PRIu16, tprefix, suffix, node->box.u16); break;
            case TYPE_U32: printf("%su32%s %"PRIu32, tprefix, suffix, node->box.u32); break;
            case TYPE_U64: printf("%su64%s %"PRIu64, tprefix, suffix, node->box.u64); break;
            case TYPE_F32: printf("%sf32%s %f", tprefix, suffix, node->box.f32); break;
            case TYPE_F64: printf("%sf64%s %g", tprefix, suffix, node->box.f64); break;
            default:
                assert(false);
                break;
        }
    } else {
        node_print_name(node, colorize);
        printf(" = ");
        type_print(node->type, colorize);
        const char* op = NULL;
        switch (node->tag) {
            case NODE_UNDEF:   op = "undef";   break;
            case NODE_TUPLE:   op = "tuple";   break;
            case NODE_ARRAY:   op = "array";   break;
            case NODE_EXTRACT: op = "extract"; break;
            case NODE_INSERT:  op = "insert";  break;
            case NODE_CMPLT:   op = "cmplt";   break;
            case NODE_CMPGT:   op = "cmpgt";   break;
            case NODE_CMPEQ:   op = "cmpeq";   break;
            case NODE_ADD:     op = "add";     break;
            case NODE_SUB:     op = "sub";     break;
            case NODE_MUL:     op = "mul";     break;
            case NODE_DIV:     op = "div";     break;
            case NODE_MOD:     op = "mod";     break;
            case NODE_AND:     op = "and";     break;
            case NODE_OR:      op = "or";      break;
            case NODE_XOR:     op = "xor";     break;
            case NODE_LSHFT:   op = "lshft";   break;
            case NODE_RSHFT:   op = "rshft";   break;
            case NODE_IF:      op = "if";      break;
            case NODE_FN:      op = "fn";      break;
            case NODE_PARAM:   op = "param";   break;
            case NODE_APP:     op = "app";     break;
            case NODE_KNOWN:   op = "known";   break;
            default:
                assert(false);
                break;
        }
        printf(" %s%s%s ", nprefix, op, suffix);
        for (size_t i = 0; i < node->nops; ++i) {
            if (node->ops[i]->nops == 0) {
                // Print literals and undefs inline
                node_print(node->ops[i], colorize);
            } else {
                node_print_name(node->ops[i], colorize);
            }
            if (i != node->nops - 1)
                printf(", ");
        }
    }
}

void type_dump(const type_t* type) {
    type_print(type, true);
    printf("\n");
}

void node_dump(const node_t* node) {
    node_print(node, true);
    printf("\n");
}
