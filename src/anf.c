#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "anf.h"
#include "hash.h"
#include "scope.h"

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
        type1->nops != type2->nops ||
        type1->fast != type2->fast)
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
    h = hash_uint8(h, type->fast ? 0xFF : 0);
    for (size_t i = 0; i < type->nops; ++i)
        h = hash_ptr(h, type->ops[i]);
    return h;
}

mod_t* mod_create(void) {
    mod_t* mod = malloc(sizeof(mod_t));
    mod->pool  = mpool_create(4096);
    mod->nodes = internal_node_set_create(256);
    mod->types = internal_type_set_create(64);
    mod->fns   = fn_vec_create(64);
    return mod;
}

void mod_destroy(mod_t* mod) {
    mpool_destroy(mod->pool);
    internal_node_set_destroy(&mod->nodes);
    internal_type_set_destroy(&mod->types);
    fn_vec_destroy(&mod->fns);
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
        case TYPE_F32:  return true;
        case TYPE_F64:  return true;
        default:        return false;
    }
}

static inline const type_t* make_type(mod_t* mod, const type_t type) {
    const type_t** lookup = internal_type_set_lookup(&mod->types, &type);
    if (lookup)
        return *lookup;

    type_t* type_ptr = mpool_alloc(&mod->pool, sizeof(type_t));
    *type_ptr = type;
    if (type.nops > 0) {
        const type_t** type_ops = mpool_alloc(&mod->pool, sizeof(type_t*) * type.nops);
        for (size_t i = 0; i < type.nops; ++i) type_ops[i] = type.ops[i];
        type_ptr->ops = type_ops;
    }

    bool success = internal_type_set_insert(&mod->types, type_ptr);
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
    const node_t** lookup = internal_node_set_lookup(&mod->nodes, &node);
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

    bool success = internal_node_set_insert(&mod->nodes, node_ptr);
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

uint64_t node_value_u(const node_t* node) {
    assert(node->tag == NODE_LITERAL);
    switch (node->type->tag) {
        case TYPE_I1:  return node->box.i1 ? UINT64_C(1) : UINT64_C(0);
        case TYPE_I8:  return node->box.i8;
        case TYPE_I16: return node->box.i16;
        case TYPE_I32: return node->box.i32;
        case TYPE_I64: return node->box.i64;
        case TYPE_U8:  return node->box.u8;
        case TYPE_U16: return node->box.u16;
        case TYPE_U32: return node->box.u32;
        case TYPE_U64: return node->box.u64;
        case TYPE_F32: return node->box.f32;
        case TYPE_F64: return node->box.f64;
        default:       assert(false); return 0;
    }
}

int64_t node_value_i(const node_t* node) {
    assert(node->tag == NODE_LITERAL);
    switch (node->type->tag) {
        case TYPE_I1:  return node->box.i1 ? INT64_C(1) : INT64_C(0);
        case TYPE_I8:  return node->box.i8;
        case TYPE_I16: return node->box.i16;
        case TYPE_I32: return node->box.i32;
        case TYPE_I64: return node->box.i64;
        case TYPE_U8:  return node->box.u8;
        case TYPE_U16: return node->box.u16;
        case TYPE_U32: return node->box.u32;
        case TYPE_U64: return node->box.u64;
        case TYPE_F32: return node->box.f32;
        case TYPE_F64: return node->box.f64;
        default:       assert(false); return 0;
    }
}

double node_value_f(const node_t* node) {
    assert(node->tag == NODE_LITERAL);
    switch (node->type->tag) {
        case TYPE_I1:  return node->box.i1 ? 1.0 : 0.0;
        case TYPE_I8:  return node->box.i8;
        case TYPE_I16: return node->box.i16;
        case TYPE_I32: return node->box.i32;
        case TYPE_I64: return node->box.i64;
        case TYPE_U8:  return node->box.u8;
        case TYPE_U16: return node->box.u16;
        case TYPE_U32: return node->box.u32;
        case TYPE_U64: return node->box.u64;
        case TYPE_F32: return node->box.f32;
        case TYPE_F64: return node->box.f64;
        default:       assert(false); return 0;
    }
}

bool node_is_const(const node_t* node) {
    if (node->tag == NODE_LITERAL ||
        node->tag == NODE_FN)
        return true;
    if (node->tag == NODE_PARAM)
        return false;
    for (size_t i = 0; i < node->nops; ++i) {
        if (!node_is_const(node->ops[i]))
            return false;
    }
    return true;
}

bool node_is_zero(const node_t* node) {
    if (node->tag != NODE_LITERAL)
        return false;
    switch (node->type->tag) {
        case TYPE_I1:  return !node->box.i1;
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
        case TYPE_I1:  return node->box.i1;
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
        case TYPE_I1:  return node->box.i1;
        case TYPE_I8:  return node->box.i8  == -1;
        case TYPE_I16: return node->box.i16 == -1;
        case TYPE_I32: return node->box.i32 == -1;
        case TYPE_I64: return node->box.i64 == -1;
        case TYPE_U8:  return node->box.u8  == (uint8_t) (-1);
        case TYPE_U16: return node->box.u16 == (uint16_t)(-1);
        case TYPE_U32: return node->box.u32 == (uint32_t)(-1);
        case TYPE_U64: return node->box.u64 == (uint64_t)(-1);
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
        default:       assert(false); return NULL;
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
        default:       assert(false); return NULL;
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
        case TYPE_U8:  return node_u8 (mod, -1);
        case TYPE_U16: return node_u16(mod, -1);
        case TYPE_U32: return node_u32(mod, -1);
        case TYPE_U64: return node_u64(mod, -1);
        default:       assert(false); return NULL;
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

bool node_is_not(const node_t* node) {
    return node->tag == NODE_XOR && node_is_all_ones(node->ops[0]);
}

bool node_is_cmp(const node_t* node) {
    return node->tag == NODE_CMPGT ||
           node->tag == NODE_CMPGE ||
           node->tag == NODE_CMPLT ||
           node->tag == NODE_CMPLE ||
           node->tag == NODE_CMPNE ||
           node->tag == NODE_CMPEQ;
}

bool node_implies(mod_t* mod, const node_t* left, const node_t* right, bool not_left, bool not_right) {
    assert(left->type->tag == TYPE_I1);
    assert(right->type->tag == TYPE_I1);
    if (left->tag == NODE_LITERAL) {
        // (0 => X) <=> 1
        if (( not_left &&  left->box.i1) ||
            (!not_left && !left->box.i1))
            return true;
    }
    if (not_left == not_right && left == right)
        return true;
    if (left->tag == NODE_AND) {
        if (not_left) {
            // ~(X & Y) => right <=> (~X | ~Y) => right
            return node_implies(mod, left->ops[0], right, !not_left, not_right) &&
                   node_implies(mod, left->ops[1], right, !not_left, not_right);
        } else {
            // (X & Y) => right <=> (X => right) | (Y => right)
            return node_implies(mod, left->ops[0], right, not_left, not_right) ||
                   node_implies(mod, left->ops[1], right, not_left, not_right);
        }
    } else if (left->tag == NODE_OR) {
        if (not_left) {
            // ~(X | Y) => right <=> (~X & ~Y) => right
            return node_implies(mod, left->ops[0], right, !not_left, not_right) ||
                   node_implies(mod, left->ops[1], right, !not_left, not_right);
        } else {
            // X | Y => right <=> (X => right) & (Y => right)
            return node_implies(mod, left->ops[0], right, not_left, not_right) &&
                   node_implies(mod, left->ops[1], right, not_left, not_right);
        }
    } else if (left->tag == NODE_XOR) {
        if (node_is_not(left)) {
            return node_implies(mod, left->ops[1], right, !not_left, not_right);
        } else {
            if (not_left) {
                // ~(X ^ Y) => right <=> (~X | Y) & (X | ~Y) => right
                return (node_implies(mod, left->ops[0], right, !not_left, not_right) &&
                        node_implies(mod, left->ops[1], right,  not_left, not_right)) ||
                       (node_implies(mod, left->ops[0], right,  not_left, not_right) &&
                        node_implies(mod, left->ops[1], right, !not_left, not_right));
            } else {
                // (X ^ Y) => right <=> (X & ~Y) | (~X & Y) => right
                return (node_implies(mod, left->ops[0], right, !not_left, not_right) ||
                        node_implies(mod, left->ops[1], right,  not_left, not_right)) &&
                       (node_implies(mod, left->ops[0], right,  not_left, not_right) ||
                        node_implies(mod, left->ops[1], right, !not_left, not_right));
            }
        }
    } else if (right->tag == NODE_AND) {
        if (not_right) {
            // left => ~(X & Y) <=> left => (~X | ~Y)
            return node_implies(mod, left, right->ops[0], not_left, !not_right) ||
                   node_implies(mod, left, right->ops[1], not_left, !not_right);
        } else {
            // left => X & Y <=> (left => X) & (left => Y)
            return node_implies(mod, left, right->ops[0], not_left, not_right) &&
                   node_implies(mod, left, right->ops[1], not_left, not_right);
        }
    } else if (right->tag == NODE_OR) {
        if (not_right) {
            // left => ~(X | Y) <=> left => (~X & ~Y)
            return node_implies(mod, left, right->ops[0], not_left, !not_right) &&
                   node_implies(mod, left, right->ops[1], not_left, !not_right);
        } else {
            // left => X | Y <=> (left => X) | (left => Y)
            return node_implies(mod, left, right->ops[0], not_left, not_right) ||
                   node_implies(mod, left, right->ops[1], not_left, not_right);
        }
    } else if (right->tag == NODE_XOR) {
        if (node_is_not(right)) {
            return node_implies(mod, left, right->ops[1], not_left, !not_right);
        } else {
            if (not_right) {
                // left => ~(X ^ Y) <=> left => (~X | Y) & (X | ~Y)
                return (node_implies(mod, left, right->ops[0], not_left, !not_right) ||
                        node_implies(mod, left, right->ops[1], not_left,  not_right)) &&
                       (node_implies(mod, left, right->ops[0], not_left,  not_right) ||
                        node_implies(mod, left, right->ops[1], not_left, !not_right));
            } else {
                // left => (X ^ Y) <=> left => (X & ~Y) | (~X & Y)
                return (node_implies(mod, left, right->ops[0], not_left, !not_right) &&
                        node_implies(mod, left, right->ops[1], not_left,  not_right)) ||
                       (node_implies(mod, left, right->ops[0], not_left,  not_right) &&
                        node_implies(mod, left, right->ops[1], not_left, !not_right));
            }
        }
    } else {
        left  = not_left  ? node_not(mod, left, NULL)  : left;
        right = not_right ? node_not(mod, right, NULL) : right;
        if (left == right)
            return true;

        if (left->ops[1] == right->ops[1] && node_is_cmp(left) && node_is_cmp(right)) {
            if (left->ops[0]->tag == NODE_LITERAL && right->ops[0]->tag == NODE_LITERAL) {
                // K1 > X => K2 > X
                if ((left->tag  == NODE_CMPGT || left->tag  == NODE_CMPGE) &&
                    (right->tag == NODE_CMPGT || right->tag == NODE_CMPGE)) {
                    // 3 > x does not imply 3 >= x, but the converse is true
                    if (left->tag == NODE_CMPGT && right->tag == NODE_CMPGE)
                        return node_cmplt(mod, left->ops[0], right->ops[0], NULL)->box.i1;
                    else
                        return node_cmple(mod, left->ops[0], right->ops[0], NULL)->box.i1;
                }
                // K1 < X => K2 < X
                if ((left->tag  == NODE_CMPLT || left->tag  == NODE_CMPLE) &&
                    (right->tag == NODE_CMPLT || right->tag == NODE_CMPLE)) {
                    // 3 < x does not imply 3 <= x, but the converse is true
                    if (left->tag == NODE_CMPLT && right->tag == NODE_CMPLE)
                        return node_cmpgt(mod, left->ops[0], right->ops[0], NULL)->box.i1;
                    else
                        return node_cmpge(mod, left->ops[0], right->ops[0], NULL)->box.i1;
                }
            }

            if (left->ops[0] == right->ops[0] && left->ops[1] == right->ops[1]) {
                // X == Y => X <= Y
                // X == Y => X >= Y
                if (left->tag == NODE_CMPEQ && right->tag == NODE_CMPLE) return true;
                if (left->tag == NODE_CMPEQ && right->tag == NODE_CMPGE) return true;
                // X < Y => X != Y
                // X > Y => X != Y
                if (left->tag == NODE_CMPLT && right->tag == NODE_CMPNE) return true;
                if (left->tag == NODE_CMPGT && right->tag == NODE_CMPNE) return true;
            }
        }

        return false;
    }
}

static inline const node_t* try_fold_tuple(size_t nops, const node_t** ops) {
    const node_t* base = NULL;
    for (size_t i = 0; i < nops; ++i) {
        const node_t* op = ops[i];
        if (op->tag != NODE_EXTRACT ||
            (base && op->ops[0] != base) ||
            op->ops[1]->tag != NODE_LITERAL ||
            (size_t)node_value_u(op->ops[1]) != i)
            return NULL;
        base = op->ops[0];
    }
    return base;
}

const node_t* node_tuple(mod_t* mod, size_t nops, const node_t** ops, const dbg_t* dbg) {
    if (nops == 1) return ops[0];
    // (extract(t, 0), extract(t, 1), extract(t, 2), ...) <=> t
    const node_t* base = try_fold_tuple(nops, ops);
    if (base && base->type->tag == TYPE_TUPLE && base->type->nops == nops)
        return base;
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

    // extract(insert(value, index, elem), index) <=> elem
    if (value->tag == NODE_INSERT && index->tag == NODE_LITERAL) {
        const node_t* insert = value;
        do {
            if (insert->ops[1] == index)
                return insert->ops[2];
            insert = insert->ops[0];
        } while (insert->tag == NODE_INSERT);
    }

    if (value->type->tag == TYPE_TUPLE) {
        assert(index->tag == NODE_LITERAL);
        assert(index->box.u64 < value->type->nops);
        elem_type = value->type->ops[index->box.u64];
        if (value->tag == NODE_TUPLE)
            return value->ops[index->box.u64];
    } else if (value->type->tag == TYPE_ARRAY) {
        elem_type = value->type->ops[0];
        if (value->tag == NODE_ARRAY) {
            if (index->box.u64 >= value->nops)
                return node_undef(mod, elem_type);
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

#define WIDENOP_I(value, type) \
    switch (type->tag) { \
        case TYPE_I1:  return node_i1(mod,  value != 0 ? true : false); \
        case TYPE_I8:  return node_i8(mod,  value); \
        case TYPE_I16: return node_i16(mod, value); \
        case TYPE_I32: return node_i32(mod, value); \
        case TYPE_I64: return node_i64(mod, value); \
        default: \
            assert(false); \
            break; \
    }

#define WIDENOP_U(value, type) \
    switch (type->tag) { \
        case TYPE_U8:  return node_u8(mod,  value); \
        case TYPE_U16: return node_u16(mod, value); \
        case TYPE_U32: return node_u32(mod, value); \
        case TYPE_U64: return node_u64(mod, value); \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_widen(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert((type_is_i(type) && type_is_i(value->type)) ||
           (type_is_u(type) && type_is_u(value->type)));
    assert(type_bitwidth(value->type) <= type_bitwidth(type));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_I1:
                {
                    int8_t i = value->box.i1 ? -1 : 0;
                    WIDENOP_I(i, type)
                }
                break;
            case TYPE_I8:  WIDENOP_I(value->box.i8,  type) break;
            case TYPE_I16: WIDENOP_I(value->box.i16, type) break;
            case TYPE_I32: WIDENOP_I(value->box.i32, type) break;
            case TYPE_I64: WIDENOP_I(value->box.i64, type) break;
            case TYPE_U8:  WIDENOP_U(value->box.u8,  type) break;
            case TYPE_U16: WIDENOP_U(value->box.u16, type) break;
            case TYPE_U32: WIDENOP_U(value->box.u32, type) break;
            case TYPE_U64: WIDENOP_U(value->box.u64, type) break;
            default:
                assert(false);
                break;
        }
    }
    if (type == value->type)
        return value;
    return make_node(mod, (node_t) {
        .tag  = NODE_WIDEN,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

#define TRUNCOP_I(value, type) \
    switch (type->tag) { \
        case TYPE_I1:  return node_i1(mod,  value & 1 ? true : false); \
        case TYPE_I8:  return node_i8(mod,  value & (-1)); \
        case TYPE_I16: return node_i16(mod, value & (-1)); \
        case TYPE_I32: return node_i32(mod, value & (-1)); \
        case TYPE_I64: return node_i64(mod, value & (-1)); \
        default: \
            assert(false); \
            break; \
    }

#define TRUNCOP_U(value, type) \
    switch (type->tag) { \
        case TYPE_U8:  return node_u8(mod,  value & (-1)); \
        case TYPE_U16: return node_u16(mod, value & (-1)); \
        case TYPE_U32: return node_u32(mod, value & (-1)); \
        case TYPE_U64: return node_u64(mod, value & (-1)); \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_trunc(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert((type_is_i(type) && type_is_i(value->type)) ||
           (type_is_u(type) && type_is_u(value->type)));
    assert(type_bitwidth(value->type) >= type_bitwidth(type));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_I1:
                {
                    int8_t i = value->box.i1 ? -1 : 0;
                    TRUNCOP_I(i, type)
                }
                break;
            case TYPE_I8:  TRUNCOP_I(value->box.i8,  type) break;
            case TYPE_I16: TRUNCOP_I(value->box.i16, type) break;
            case TYPE_I32: TRUNCOP_I(value->box.i32, type) break;
            case TYPE_I64: TRUNCOP_I(value->box.i64, type) break;
            case TYPE_U8:  TRUNCOP_U(value->box.u8,  type) break;
            case TYPE_U16: TRUNCOP_U(value->box.u16, type) break;
            case TYPE_U32: TRUNCOP_U(value->box.u32, type) break;
            case TYPE_U64: TRUNCOP_U(value->box.u64, type) break;
            default:
                assert(false);
                break;
        }
    }
    if (type == value->type)
        return value;
    // trunc(widen(x : i32, i64), i32) <=> x
    if (value->tag == NODE_WIDEN) {
        const node_t* widen = value->ops[0];
        while (widen->tag == NODE_WIDEN && widen->type != type)
            widen = widen->ops[0];
        if (widen->type == type)
            return widen;
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_TRUNC,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

#define ITOFOP(value, type) \
    switch (type->tag) { \
        case TYPE_F32: return node_f32(mod, value); \
        case TYPE_F64: return node_f64(mod, value); \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_itof(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert(type_is_f(type) && (type_is_i(value->type) || type_is_u(value->type)));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_I1:
                {
                    int8_t i = value->box.i1 ? -1 : 0;
                    ITOFOP(i, type)
                }
                break;
            case TYPE_I8:  ITOFOP(value->box.i8,  type) break;
            case TYPE_I16: ITOFOP(value->box.i16, type) break;
            case TYPE_I32: ITOFOP(value->box.i32, type) break;
            case TYPE_I64: ITOFOP(value->box.i64, type) break;
            case TYPE_U8:  ITOFOP(value->box.u8,  type) break;
            case TYPE_U16: ITOFOP(value->box.u16, type) break;
            case TYPE_U32: ITOFOP(value->box.u32, type) break;
            case TYPE_U64: ITOFOP(value->box.u64, type) break;
            default:
                assert(false);
                break;
        }
    }
    // itof(ftoi(x)) <=> x if x signed
    if (value->tag == NODE_FTOI && type_is_i(value->type) && type->fast)
        return value->ops[0];
    return make_node(mod, (node_t) {
        .tag  = NODE_ITOF,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

#define FTOIOP(value, type) \
    switch (type->tag) { \
        case TYPE_I1:  return node_i1(mod,  value != 0 ? true : false); \
        case TYPE_I8:  return node_i8(mod,  value); \
        case TYPE_I16: return node_i16(mod, value); \
        case TYPE_I32: return node_i32(mod, value); \
        case TYPE_I64: return node_i64(mod, value); \
        case TYPE_U8:  return node_u8(mod,  value); \
        case TYPE_U16: return node_u16(mod, value); \
        case TYPE_U32: return node_u32(mod, value); \
        case TYPE_U64: return node_u64(mod, value); \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_ftoi(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert(type_is_f(value->type) && (type_is_i(type) || type_is_u(type)));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_F32: FTOIOP(value->box.f32, type) break;
            case TYPE_F64: FTOIOP(value->box.f64, type) break;
            default:
                assert(false);
                break;
        }
    }
    // ftoi(itof(x)) <=> x
    if (value->tag == NODE_ITOF && value->type->fast)
        return value->ops[0];
    return make_node(mod, (node_t) {
        .tag  = NODE_FTOI,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

static inline bool node_is_commutative(uint32_t tag, const type_t* type) {
    switch (tag) {
        case NODE_ADD: return !type_is_f(type) || type->fast;
        case NODE_MUL: return !type_is_f(type) || type->fast;
        case NODE_AND: return true;
        case NODE_OR:  return true;
        case NODE_XOR: return true;
        default: return false;
    }
}

static inline bool node_is_distributive(uint32_t tag1, uint32_t tag2, const type_t* type) {
    switch (tag1) {
        case NODE_MUL: return (tag2 == NODE_ADD || tag2 == NODE_SUB) &&
                              (!type_is_f(type) || type->fast);
        case NODE_AND: return tag2 == NODE_OR;
        case NODE_OR:  return tag2 == NODE_AND;
        default: return false;
    }
}

static inline bool node_can_switch_comparands(uint32_t tag, const type_t* type) {
    return tag == NODE_CMPEQ || !type_is_f(type) || type->fast;
}

static inline bool node_should_switch_ops(const node_t* left, const node_t* right) {
    // Establish a standardized order for operands in commutative expressions/comparisons
    // - Literals always go to the left
    // - Non-literal operands are ordered by address
    return right->tag == NODE_LITERAL || ((uintptr_t)left > (uintptr_t)right && left->tag != NODE_LITERAL);
}

static inline void node_switch_ops(const node_t** left, const node_t** right) {
    const node_t* tmp = *left;
    *left = *right;
    *right = tmp;
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
    assert(tag == NODE_CMPEQ || tag == NODE_CMPNE || left->type->tag != TYPE_I1);

    if (left->tag == NODE_LITERAL && right->tag == NODE_LITERAL) {
        bool res = false;
        switch (tag) {
            case NODE_CMPGT: CMPOP(left->type->tag, CMPOP_IUF  (>,  res, left->box, right->box)) break;
            case NODE_CMPGE: CMPOP(left->type->tag, CMPOP_IUF  (>=, res, left->box, right->box)) break;
            case NODE_CMPLT: CMPOP(left->type->tag, CMPOP_IUF  (<,  res, left->box, right->box)) break;
            case NODE_CMPLE: CMPOP(left->type->tag, CMPOP_IUF  (<=, res, left->box, right->box)) break;
            case NODE_CMPNE: CMPOP(left->type->tag, CMPOP_I1IUF(!=, res, left->box, right->box)) break;
            case NODE_CMPEQ: CMPOP(left->type->tag, CMPOP_I1IUF(==, res, left->box, right->box)) break;
            default:
                assert(false);
                break;
        }
        return node_i1(mod, res);
    }

    if (node_should_switch_ops(left, right) && node_can_switch_comparands(tag, left->type)) {
        node_switch_ops(&left, &right);
        switch (tag) {
            case NODE_CMPGT: tag = NODE_CMPLT; break;
            case NODE_CMPGE: tag = NODE_CMPLE; break;
            case NODE_CMPLT: tag = NODE_CMPGT; break;
            case NODE_CMPLE: tag = NODE_CMPGE; break;
            default:
                assert(tag == NODE_CMPEQ || tag == NODE_CMPNE);
                break;
        }
    }

    if (left == right) {
        // X > X  <=> false
        // X < X  <=> false
        // X != X <=> false
        if (tag == NODE_CMPNE || tag == NODE_CMPGT || tag == NODE_CMPLT) return node_i1(mod, false);
        // X == X <=> true
        // X >= X <=> true
        // X <= X <=> true
        if (tag == NODE_CMPEQ || tag == NODE_CMPGE || tag == NODE_CMPLE) return node_i1(mod, true);
    }
    if (type_is_u(left->type) && node_is_zero(left)) {
        // 0 > X <=> false
        if (tag == NODE_CMPGT) return node_i1(mod, false);
        // 0 <= X <=> true
        if (tag == NODE_CMPLE) return node_i1(mod, true);
    }

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
NODE_CMPOP(cmpge, NODE_CMPGE)
NODE_CMPOP(cmplt, NODE_CMPLT)
NODE_CMPOP(cmple, NODE_CMPLE)
NODE_CMPOP(cmpne, NODE_CMPNE)
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

    if (node_should_switch_ops(left, right) && node_is_commutative(tag, left->type))
        node_switch_ops(&left, &right);

    // Simplification rules
    if (node_is_zero(left)) {
        // 0 + a <=> a
        // 0 | a <=> a
        // 0 ^ a <=> a
        if (tag == NODE_ADD || tag == NODE_OR || tag == NODE_XOR) return right;
        // 0 * a <=> 0
        // 0 & a <=> 0
        if (tag == NODE_MUL || tag == NODE_AND) return node_zero(mod, left->type);
    }
    if (node_is_all_ones(left)) {
        // 1 & a <=> a
        if (tag == NODE_AND) return right;
        // 1 | a <=> 1
        if (tag == NODE_OR) return left;
        // ~(a cmp b) <=> a ~(cmp) b
        if (tag == NODE_XOR && node_is_cmp(right) && node_can_switch_comparands(right->tag, right->type)) {
            switch (right->tag) {
                case NODE_CMPGT: return node_cmple(mod, right->ops[0], right->ops[1], dbg);
                case NODE_CMPGE: return node_cmplt(mod, right->ops[0], right->ops[1], dbg);
                case NODE_CMPLT: return node_cmpge(mod, right->ops[0], right->ops[1], dbg);
                case NODE_CMPLE: return node_cmpgt(mod, right->ops[0], right->ops[1], dbg);
                case NODE_CMPNE: return node_cmpeq(mod, right->ops[0], right->ops[1], dbg);
                case NODE_CMPEQ: return node_cmpne(mod, right->ops[0], right->ops[1], dbg);
                default:
                    assert(false);
                    break;
            }
        }
    }
    // 1 * a <=> a
    if (tag == NODE_MUL && node_is_one(left))
        return right;
    if (node_is_zero(right)) {
        // a * 0 <=> 0
        if (tag == NODE_MUL) return node_zero(mod, left->type);
        // a + 0 <=> 0
        if (tag == NODE_ADD) return node_zero(mod, left->type);
        // a >> 0 <=> a
        // a << 0 <=> a
        // a - 0 <=> a
        if (tag == NODE_LSHFT || tag == NODE_RSHFT || tag == NODE_SUB) return left;
        assert(tag != NODE_AND && tag != NODE_OR); // Commutative operations should be handled before
    }
    if (node_is_one(right)) {
        // a / 1 <=> a
        // a * 1 <=> a
        if (tag == NODE_DIV || tag == NODE_MUL) return left;
        // a % 1 <=> 0
        if (tag == NODE_MOD) return node_zero(mod, left->type);
    }
    if (left == right) {
        // a & a <=> a
        // a | a <=> a
        if (tag == NODE_AND || tag == NODE_OR)  return left;
        // a ^ a <=> 0
        // a % a <=> 0
        // a - a <=> 0
        if (tag == NODE_XOR || tag == NODE_MOD || tag == NODE_SUB) return node_zero(mod, left->type);
        // a / a <=> 1
        if (tag == NODE_DIV) return node_one(mod, left->type);
    }
    if (tag == NODE_AND) {
        // a & (a | b) <=> a
        // a & (b | a) <=> a
        if (right->tag == NODE_OR && (right->ops[0] == left || right->ops[1] == left))
            return left;
        // (a | b) & a <=> a
        // (b | a) & a <=> a
        if (left->tag == NODE_OR && (left->ops[0] == right || left->ops[1] == right))
            return right;
        // a & ~a <=> 0
        // ~a & a <=> 0
        if ((right->tag == NODE_XOR && node_is_not(right) && right->ops[1] == left) ||
            (left->tag  == NODE_XOR && node_is_not(left)  && left->ops[1]  == right))
            return node_zero(mod, left->type);

    }
    if (tag == NODE_OR) {
        // a | (a & b) <=> a
        // a | (b & a) <=> a
        if (right->tag == NODE_AND && (right->ops[0] == left || right->ops[1] == left))
            return left;
        // (a & b) | a <=> a
        // (b & a) | a <=> a
        if (left->tag == NODE_AND && (left->ops[0] == right || left->ops[1] == right))
            return right;
        // a | ~a <=> 1
        // ~a | a <=> 1
        if ((right->tag == NODE_XOR && node_is_not(right) && right->ops[1] == left) ||
            (left->tag  == NODE_XOR && node_is_not(left)  && left->ops[1]  == right))
            return node_all_ones(mod, left->type);
    }
    if (tag == NODE_XOR) {
        if (right->tag == NODE_XOR) {
            // a ^ (a ^ b) <=> b
            if (right->ops[0] == left) return right->ops[1];
            // a ^ (b ^ a) <=> b
            if (right->ops[1] == left) return right->ops[0];
        } else if (left->tag == NODE_XOR) {
            // (a ^ b) ^ a <=> b
            if (left->ops[0] == right) return left->ops[1];
            // (b ^ a) ^ a <=> b
            if (left->ops[1] == right) return left->ops[0];
        }
    }
    bool left_factorizable = node_is_distributive(right->tag, tag, left->type);
    if (left_factorizable && right->ops[0]->tag == NODE_LITERAL && right->ops[1] == left) {
        const node_t* one = is_bitwise ? node_all_ones(mod, left->type) : node_one(mod, left->type);
        const node_t* K   = make_binop(mod, tag, one, right->ops[0], dbg);
        assert(K->tag == NODE_LITERAL);
        // a + K * a <=> (K + 1) * a
        return make_binop(mod, right->tag, K, left, dbg);
    }
    bool right_factorizable = node_is_distributive(left->tag, tag, left->type);
    if (right_factorizable && left->ops[0]->tag == NODE_LITERAL && left->ops[1] == right) {
        const node_t* one = is_bitwise ? node_all_ones(mod, left->type) : node_one(mod, left->type);
        const node_t* K   = make_binop(mod, tag, left->ops[0], one, dbg);
        assert(K->tag == NODE_LITERAL);
        // K * a + a <=> (K + 1) * a
        return make_binop(mod, left->tag, K, right, dbg);
    }
    bool both_factorizable = left_factorizable & right_factorizable;
    // Factorization should not be applied to booleans, as this would break normal forms (CNF/DNF)
    if (both_factorizable && left->type->tag != TYPE_I1) {
        const node_t* l1 = left->ops[0];
        const node_t* l2 = left->ops[1];
        const node_t* r1 = right->ops[0];
        const node_t* r2 = right->ops[1];
        assert(left->tag == right->tag);
        bool inner_commutative = node_is_commutative(left->tag, left->type);
        if (inner_commutative && l1 == r2) { const node_t* tmp = r2; r2 = r1; r1 = tmp; }
        if (inner_commutative && l2 == r1) { const node_t* tmp = l2; l2 = l1; l1 = tmp; }
        // (a * b) + (a * c) <=> a * (b + c)
        if (l1 == r1)
            return make_binop(mod, left->tag, l1, make_binop(mod, tag, l2, r2, dbg), dbg);
        // (b * a) + (c * a) <=> (b + c) * a
        if (l2 == r2)
            return make_binop(mod, left->tag, make_binop(mod, tag, l1, r1, dbg), l2, dbg);
    }

    // Logical implications
    if (left->type->tag == TYPE_I1) {
        if (tag == NODE_AND) {
            // (A => B) => (A & B <=> A)
            if (node_implies(mod, left, right, false, false)) return left;
            // (B => A) => (A & B <=> B)
            if (node_implies(mod, right, left, false, false)) return right;
            // (A => ~B) => (A & B <=> 0)
            if (node_implies(mod, left, right, false, true)) return node_i1(mod, false);
            // (B => ~A) => (A & B <=> 0)
            if (node_implies(mod, left, right, false, true)) return node_i1(mod, false);
        } else if (tag == NODE_OR) {
            // (~A => ~B) => (A | B <=> A)
            if (node_implies(mod, left, right, true, true)) return left;
            // (~B => ~A) => (A | B <=> B)
            if (node_implies(mod, right, left, true, true)) return right;
            // (~A => B) => (A | B <=> 1)
            if (node_implies(mod, left, right, true, false)) return node_i1(mod, true);
            // (~B => A) => (A | B <=> 1)
            if (node_implies(mod, right, left, true, false)) return node_i1(mod, true);
        }
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

const node_t* node_not(mod_t* mod, const node_t* node, const dbg_t* dbg) {
    return node_xor(mod, node_all_ones(mod, node->type), node, dbg);
}

const node_t* node_known(mod_t* mod, const node_t* node, const dbg_t* dbg) {
    if (node_is_const(node))
        return node_i1(mod, true);
    return make_node(mod, (node_t) {
        .tag  = NODE_KNOWN,
        .nops = 1,
        .ops  = &node,
        .type = type_i1(mod),
        .dbg  = dbg
    });
}

const node_t* node_select(mod_t* mod, const node_t* cond, const node_t* if_true, const node_t* if_false, const dbg_t* dbg) {
    assert(cond->type->tag == TYPE_I1);
    assert(if_true->type == if_false->type);
    if (cond->tag == NODE_LITERAL)
        return cond->box.i1 ? if_true : if_false;
    if (cond->tag == NODE_UNDEF)
        return if_true; // Arbitrary, could be if_false
    if (if_true == if_false)
        return if_true;
    const node_t* ops[] = { cond, if_true, if_false };
    return make_node(mod, (node_t) {
        .tag  = NODE_SELECT,
        .nops = 3,
        .ops  = ops,
        .type = if_true->type,
        .dbg  = dbg
    });
}

void fn_bind(mod_t* mod, fn_t* fn, const node_t* call) {
    assert(fn->node.type->ops[1] == call->type);
    unregister_use(0, fn->node.ops[0], &fn->node);
    fn->node.ops[0] = call;
    register_use(mod, 0, fn->node.ops[0], &fn->node);
}

void fn_run_if(mod_t* mod, fn_t* fn, const node_t* cond) {
    assert(cond->type->tag == TYPE_I1);
    unregister_use(1, fn->node.ops[1], &fn->node);
    fn->node.ops[1] = cond;
    register_use(mod, 1, fn->node.ops[1], &fn->node);
}

fn_t* fn_cast(const node_t* node) {
    assert(node->tag == NODE_FN);
    return (fn_t*)node;
}

fn_t* node_fn(mod_t* mod, const type_t* type, const dbg_t* dbg) {
    assert(type->tag == TYPE_FN);
    fn_t* fn = mpool_alloc(&mod->pool, sizeof(fn_t));
    memset(fn, 0, sizeof(fn_t));
    const node_t** ops = mpool_alloc(&mod->pool, sizeof(node_t*) * 2);
    ops[0] = node_undef(mod, type->ops[1]);
    ops[1] = node_i1(mod, false);
    register_use(mod, 0, ops[0], &fn->node);
    register_use(mod, 1, ops[1], &fn->node);
    *((node_t*)&fn->node) = (node_t) {
        .tag  = NODE_FN,
        .nops = 2,
        .ops  = ops,
        .type = type,
        .dbg  = dbg
    };
    fn_vec_push(&mod->fns, fn);
    return fn;
}

const node_t* node_param(mod_t* mod, const fn_t* fn, const dbg_t* dbg) {
    return make_node(mod, (node_t) {
        .tag  = NODE_PARAM,
        .nops = 1,
        .ops  = (const node_t**)&fn,
        .type = fn->node.type->ops[0],
        .dbg  = dbg
    });
}

const node_t* node_app(mod_t* mod, const node_t* callee, const node_t* arg, const dbg_t* dbg) {
    assert(callee->type->tag == TYPE_FN);
    assert(callee->type->ops[0] == arg->type);
    const node_t* ops[] = { callee, arg };
    return make_node(mod, (node_t) {
        .tag  = NODE_APP,
        .nops = 2,
        .ops  = ops,
        .type = callee->type->ops[1],
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
        case NODE_CMPGT:   return node_cmpgt(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPGE:   return node_cmpge(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPLT:   return node_cmplt(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPLE:   return node_cmple(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPNE:   return node_cmpne(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPEQ:   return node_cmpeq(mod, ops[0], ops[1], node->dbg);
        case NODE_WIDEN:   return node_widen(mod, ops[0], type, node->dbg);
        case NODE_TRUNC:   return node_trunc(mod, ops[0], type, node->dbg);
        case NODE_ITOF:    return node_itof(mod, ops[0], type, node->dbg);
        case NODE_FTOI:    return node_ftoi(mod, ops[0], type, node->dbg);
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
        case NODE_SELECT:  return node_select(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_PARAM:   return node_param(mod, fn_cast(ops[0]), node->dbg);
        case NODE_APP:     return node_app(mod, ops[0], ops[1], node->dbg);
        case NODE_KNOWN:   return node_known(mod, ops[0], node->dbg);
        default:
            assert(false);
            return NULL;
    }
}

const type_t* type_rewrite(mod_t* mod, const type_t* type, type2type_t* new_types) {
    const type_t** found = type2type_lookup(new_types, type);
    if (found)
        return *found;

    const type_t* new_ops[type->nops];
    for (size_t i = 0; i < type->nops; ++i)
        new_ops[i] = type_rewrite(mod, type->ops[i], new_types);

    const type_t* new_type = type_rebuild(mod, type, new_ops);
    type2type_insert(new_types, type, new_type);
    return new_type;
}

const node_t* node_rewrite(mod_t* mod, const node_t* node, node2node_t* new_nodes, type2type_t* new_types) {
    while (node->rep) node = node->rep;
    const node_t** found = node2node_lookup(new_nodes, node);
    if (found)
        return *found;

    const type_t* new_type = new_types ? type_rewrite(mod, node->type, new_types) : node->type;
    const node_t* new_node = NULL;
    if (node->tag == NODE_FN) {
        fn_t* fn = fn_cast(node);
        fn_t* new_fn = node_fn(mod, type_rewrite(mod, node->type, new_types), node->dbg);
        new_fn->exported  = fn->exported;
        new_fn->imported  = fn->imported;
        new_fn->intrinsic = fn->intrinsic;
        new_node = &new_fn->node;
        node2node_insert(new_nodes, node, new_node);
    }

    const node_t* new_ops[node->nops];
    for (size_t i = 0; i < node->nops; ++i)
        new_ops[i] = node_rewrite(mod, node->ops[i], new_nodes, new_types);

    if (node->tag == NODE_FN) {
        fn_t* new_fn = fn_cast(new_node);
        fn_bind(mod, new_fn, new_ops[0]);
        fn_run_if(mod, new_fn, new_ops[1]);
    } else {
        new_node = node_rebuild(mod, node, new_ops, new_type);
        node2node_insert(new_nodes, node, new_node);
    }

    return new_node;
}

void node_replace(const node_t* node, const node_t* with) {
    assert(node->type == with->type);
    while (with->rep) with = with->rep;
    do {
        const node_t* rep = node->rep;
        ((node_t*)node)->rep = with;
        node = rep;
    } while(node);
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
        if (node->nops > 0) {
            node_print_name(node, colorize);
            printf(" = ");
        }
        type_print(node->type, colorize);
        const char* op = NULL;
        switch (node->tag) {
            case NODE_UNDEF:   op = "undef";   break;
            case NODE_TUPLE:   op = "tuple";   break;
            case NODE_ARRAY:   op = "array";   break;
            case NODE_EXTRACT: op = "extract"; break;
            case NODE_INSERT:  op = "insert";  break;
            case NODE_CMPGT:   op = "cmpgt";   break;
            case NODE_CMPGE:   op = "cmpge";   break;
            case NODE_CMPLT:   op = "cmplt";   break;
            case NODE_CMPLE:   op = "cmple";   break;
            case NODE_CMPNE:   op = "cmpne";   break;
            case NODE_CMPEQ:   op = "cmpeq";   break;
            case NODE_WIDEN:   op = "widen";   break;
            case NODE_TRUNC:   op = "trunc";   break;
            case NODE_ITOF:    op = "itof";    break;
            case NODE_FTOI:    op = "ftoi";    break;
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
            case NODE_SELECT:  op = "select";  break;
            case NODE_FN:      op = "fn";      break;
            case NODE_PARAM:   op = "param";   break;
            case NODE_APP:     op = "app";     break;
            case NODE_KNOWN:   op = "known";   break;
            default:
                assert(false);
                break;
        }
        printf(" %s%s%s", nprefix, op, suffix);
        if (node->nops > 0) {
            printf(" ");
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
}

void type_dump(const type_t* type) {
    type_print(type, true);
    printf("\n");
}

void node_dump(const node_t* node) {
    node_print(node, true);
    printf("\n");
}
