#include <stdarg.h>

#include "node.h"
#include "type.h"

static inline size_t box_size(uint32_t tag) {
    switch (tag) {
        case TYPE_BOOL: return sizeof(bool);
        case TYPE_I8:   return sizeof(int8_t);
        case TYPE_I16:  return sizeof(int16_t);
        case TYPE_I32:  return sizeof(int32_t);
        case TYPE_I64:  return sizeof(int64_t);
        case TYPE_U8:   return sizeof(uint8_t);
        case TYPE_U16:  return sizeof(uint16_t);
        case TYPE_U32:  return sizeof(uint32_t);
        case TYPE_U64:  return sizeof(uint64_t);
        case TYPE_F32:  return sizeof(float);
        case TYPE_F64:  return sizeof(double);
        default:
            assert(false);
            return 0;
    }
}

static inline const node_t* make_node(mod_t* mod, const node_t node) {
    return mod_insert_node(mod, &node);
}

uint64_t node_value_u(const node_t* node) {
    assert(node->tag == NODE_LITERAL);
    switch (node->type->tag) {
        case TYPE_BOOL: return node->data.box.b ? UINT64_C(1) : UINT64_C(0);
        case TYPE_I8:   return node->data.box.i8;
        case TYPE_I16:  return node->data.box.i16;
        case TYPE_I32:  return node->data.box.i32;
        case TYPE_I64:  return node->data.box.i64;
        case TYPE_U8:   return node->data.box.u8;
        case TYPE_U16:  return node->data.box.u16;
        case TYPE_U32:  return node->data.box.u32;
        case TYPE_U64:  return node->data.box.u64;
        case TYPE_F32:  return node->data.box.f32;
        case TYPE_F64:  return node->data.box.f64;
        default:
            assert(false);
            return 0;
    }
}

int64_t node_value_i(const node_t* node) {
    assert(node->tag == NODE_LITERAL);
    switch (node->type->tag) {
        case TYPE_BOOL: return node->data.box.b ? INT64_C(1) : INT64_C(0);
        case TYPE_I8:   return node->data.box.i8;
        case TYPE_I16:  return node->data.box.i16;
        case TYPE_I32:  return node->data.box.i32;
        case TYPE_I64:  return node->data.box.i64;
        case TYPE_U8:   return node->data.box.u8;
        case TYPE_U16:  return node->data.box.u16;
        case TYPE_U32:  return node->data.box.u32;
        case TYPE_U64:  return node->data.box.u64;
        case TYPE_F32:  return node->data.box.f32;
        case TYPE_F64:  return node->data.box.f64;
        default:
            assert(false);
            return 0;
    }
}

double node_value_f(const node_t* node) {
    assert(node->tag == NODE_LITERAL);
    switch (node->type->tag) {
        case TYPE_BOOL: return node->data.box.b ? 1.0 : 0.0;
        case TYPE_I8:   return node->data.box.i8;
        case TYPE_I16:  return node->data.box.i16;
        case TYPE_I32:  return node->data.box.i32;
        case TYPE_I64:  return node->data.box.i64;
        case TYPE_U8:   return node->data.box.u8;
        case TYPE_U16:  return node->data.box.u16;
        case TYPE_U32:  return node->data.box.u32;
        case TYPE_U64:  return node->data.box.u64;
        case TYPE_F32:  return node->data.box.f32;
        case TYPE_F64:  return node->data.box.f64;
        default:
            assert(false);
            return 0;
    }
}

bool node_value_b(const node_t* node) {
    assert(node->tag == NODE_LITERAL && node->type->tag == TYPE_BOOL);
    return node->data.box.b;
}

bool node_is_unit(const node_t* node) {
    return node->tag == NODE_TUPLE && node->nops == 0;
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
        case TYPE_BOOL: return !node->data.box.b;
        case TYPE_I8:   return node->data.box.i8  == 0;
        case TYPE_I16:  return node->data.box.i16 == 0;
        case TYPE_I32:  return node->data.box.i32 == 0;
        case TYPE_I64:  return node->data.box.i64 == 0;
        case TYPE_U8:   return node->data.box.u8  == 0;
        case TYPE_U16:  return node->data.box.u16 == 0;
        case TYPE_U32:  return node->data.box.u32 == 0;
        case TYPE_U64:  return node->data.box.u64 == 0;
        case TYPE_F32:  return node->data.box.f32 == 0.0f;
        case TYPE_F64:  return node->data.box.f64 == 0.0;
        default:        return false;
    }
}

bool node_is_one(const node_t* node) {
    if (node->tag != NODE_LITERAL)
        return false;
    switch (node->type->tag) {
        case TYPE_BOOL: return node->data.box.b;
        case TYPE_I8:   return node->data.box.i8  == 1;
        case TYPE_I16:  return node->data.box.i16 == 1;
        case TYPE_I32:  return node->data.box.i32 == 1;
        case TYPE_I64:  return node->data.box.i64 == 1;
        case TYPE_U8:   return node->data.box.u8  == 1;
        case TYPE_U16:  return node->data.box.u16 == 1;
        case TYPE_U32:  return node->data.box.u32 == 1;
        case TYPE_U64:  return node->data.box.u64 == 1;
        case TYPE_F32:  return node->data.box.f32 == 1.0f;
        case TYPE_F64:  return node->data.box.f64 == 1.0;
        default:        return false;
    }
}

bool node_is_all_ones(const node_t* node) {
    if (node->tag != NODE_LITERAL)
        return false;
    switch (node->type->tag) {
        case TYPE_BOOL: return node->data.box.b;
        case TYPE_I8:   return node->data.box.i8  == -1;
        case TYPE_I16:  return node->data.box.i16 == -1;
        case TYPE_I32:  return node->data.box.i32 == -1;
        case TYPE_I64:  return node->data.box.i64 == -1;
        case TYPE_U8:   return node->data.box.u8  == (uint8_t) (-1);
        case TYPE_U16:  return node->data.box.u16 == (uint16_t)(-1);
        case TYPE_U32:  return node->data.box.u32 == (uint32_t)(-1);
        case TYPE_U64:  return node->data.box.u64 == (uint64_t)(-1);
        default:        return false;
    }
}

const node_t* node_top(mod_t* mod, const type_t* type) {
    if (type->tag == TYPE_TUPLE) {
        const node_t* ops[type->nops];
        for (size_t i = 0; i < type->nops; ++i)
            ops[i] = node_top(mod, type->ops[i]);
        return node_tuple(mod, type->nops, ops, NULL);
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_TOP,
        .nops = 0,
        .type = type,
    });
}

const node_t* node_bottom(mod_t* mod, const type_t* type) {
    if (type->tag == TYPE_TUPLE) {
        const node_t* ops[type->nops];
        for (size_t i = 0; i < type->nops; ++i)
            ops[i] = node_bottom(mod, type->ops[i]);
        return node_tuple(mod, type->nops, ops, NULL);
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_BOTTOM,
        .nops = 0,
        .type = type,
    });
}

const node_t* node_zero(mod_t* mod, const type_t* type) {
    assert(type_is_prim(type));
    switch (type->tag) {
        case TYPE_BOOL: return node_literal(mod, type, (box_t) { .b   = false });
        case TYPE_I8:   return node_literal(mod, type, (box_t) { .i8  = 0 });
        case TYPE_I16:  return node_literal(mod, type, (box_t) { .i16 = 0 });
        case TYPE_I32:  return node_literal(mod, type, (box_t) { .i32 = 0 });
        case TYPE_I64:  return node_literal(mod, type, (box_t) { .i64 = 0 });
        case TYPE_U8:   return node_literal(mod, type, (box_t) { .u8  = 0 });
        case TYPE_U16:  return node_literal(mod, type, (box_t) { .u16 = 0 });
        case TYPE_U32:  return node_literal(mod, type, (box_t) { .u32 = 0 });
        case TYPE_U64:  return node_literal(mod, type, (box_t) { .u64 = 0 });
        case TYPE_F32:  return node_literal(mod, type, (box_t) { .f32 = 0.0f });
        case TYPE_F64:  return node_literal(mod, type, (box_t) { .f64 = 0.0 });
        default:
            assert(false);
            return NULL;
    }
}

const node_t* node_one(mod_t* mod, const type_t* type) {
    assert(type_is_prim(type));
    switch (type->tag) {
        case TYPE_BOOL: return node_literal(mod, type, (box_t) { .b   = true });
        case TYPE_I8:   return node_literal(mod, type, (box_t) { .i8  = 1 });
        case TYPE_I16:  return node_literal(mod, type, (box_t) { .i16 = 1 });
        case TYPE_I32:  return node_literal(mod, type, (box_t) { .i32 = 1 });
        case TYPE_I64:  return node_literal(mod, type, (box_t) { .i64 = 1 });
        case TYPE_U8:   return node_literal(mod, type, (box_t) { .u8  = 1 });
        case TYPE_U16:  return node_literal(mod, type, (box_t) { .u16 = 1 });
        case TYPE_U32:  return node_literal(mod, type, (box_t) { .u32 = 1 });
        case TYPE_U64:  return node_literal(mod, type, (box_t) { .u64 = 1 });
        case TYPE_F32:  return node_literal(mod, type, (box_t) { .f32 = 1.0f });
        case TYPE_F64:  return node_literal(mod, type, (box_t) { .f64 = 1.0 });
        default:
            assert(false);
            return NULL;
    }
}

const node_t* node_all_ones(mod_t* mod, const type_t* type) {
    assert(type_is_prim(type));
    assert(!type_is_f(type));
    switch (type->tag) {
        case TYPE_BOOL: return node_literal(mod, type, (box_t) { .b   = true });
        case TYPE_I8:   return node_literal(mod, type, (box_t) { .i8  = -1 });
        case TYPE_I16:  return node_literal(mod, type, (box_t) { .i16 = -1 });
        case TYPE_I32:  return node_literal(mod, type, (box_t) { .i32 = -1 });
        case TYPE_I64:  return node_literal(mod, type, (box_t) { .i64 = -1 });
        case TYPE_U8:   return node_literal(mod, type, (box_t) { .u8  = -1 });
        case TYPE_U16:  return node_literal(mod, type, (box_t) { .u16 = -1 });
        case TYPE_U32:  return node_literal(mod, type, (box_t) { .u32 = -1 });
        case TYPE_U64:  return node_literal(mod, type, (box_t) { .u64 = -1 });
        default:
            assert(false);
            return NULL;
    }
}

const node_t* node_bool(mod_t* mod, bool     b)   { return node_literal(mod, type_bool(mod), (box_t) { .b   = b   }); }
const node_t* node_i8  (mod_t* mod, int8_t   i8)  { return node_literal(mod, type_i8(mod),   (box_t) { .i8  = i8  }); }
const node_t* node_i16 (mod_t* mod, int16_t  i16) { return node_literal(mod, type_i16(mod),  (box_t) { .i16 = i16 }); }
const node_t* node_i32 (mod_t* mod, int32_t  i32) { return node_literal(mod, type_i32(mod),  (box_t) { .i32 = i32 }); }
const node_t* node_i64 (mod_t* mod, int64_t  i64) { return node_literal(mod, type_i64(mod),  (box_t) { .i64 = i64 }); }
const node_t* node_u8  (mod_t* mod, uint8_t  u8)  { return node_literal(mod, type_u8(mod),   (box_t) { .u8  = u8  }); }
const node_t* node_u16 (mod_t* mod, uint16_t u16) { return node_literal(mod, type_u16(mod),  (box_t) { .u16 = u16 }); }
const node_t* node_u32 (mod_t* mod, uint32_t u32) { return node_literal(mod, type_u32(mod),  (box_t) { .u32 = u32 }); }
const node_t* node_u64 (mod_t* mod, uint64_t u64) { return node_literal(mod, type_u64(mod),  (box_t) { .u64 = u64 }); }

const node_t* node_f32(mod_t* mod, float  f32, uint32_t fp_flags) { return node_literal(mod, type_f32(mod, fp_flags), (box_t) { .f32 = f32 }); }
const node_t* node_f64(mod_t* mod, double f64, uint32_t fp_flags) { return node_literal(mod, type_f64(mod, fp_flags), (box_t) { .f64 = f64 }); }

const node_t* node_literal(mod_t* mod, const type_t* type, box_t box) {
    return make_node(mod, (node_t) {
        .tag  = NODE_LITERAL,
        .nops = 0,
        .data = { .box = box },
        .dsize = box_size(type->tag),
        .type = type,
    });
}

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
    assert(left->type->tag  == TYPE_BOOL);
    assert(right->type->tag == TYPE_BOOL);
    if (left->tag == NODE_LITERAL) {
        // (0 => X) <=> 1
        if (( not_left &&  node_value_b(left)) ||
            (!not_left && !node_value_b(left)))
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
                        return node_value_b(node_cmplt(mod, left->ops[0], right->ops[0], NULL));
                    else
                        return node_value_b(node_cmple(mod, left->ops[0], right->ops[0], NULL));
                }
                // K1 < X => K2 < X
                if ((left->tag  == NODE_CMPLT || left->tag  == NODE_CMPLE) &&
                    (right->tag == NODE_CMPLT || right->tag == NODE_CMPLE)) {
                    // 3 < x does not imply 3 <= x, but the converse is true
                    if (left->tag == NODE_CMPLT && right->tag == NODE_CMPLE)
                        return node_value_b(node_cmpgt(mod, left->ops[0], right->ops[0], NULL));
                    else
                        return node_value_b(node_cmpge(mod, left->ops[0], right->ops[0], NULL));
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

const node_t* node_unit(mod_t* mod) {
    return node_tuple(mod, 0, NULL, NULL);
}

const node_t* node_tuple(mod_t* mod, size_t nops, const node_t** ops, const dbg_t* dbg) {
    if (nops == 1) return ops[0];
    // (extract(t, 0), extract(t, 1), extract(t, 2), ...) <=> t
    const node_t* base = try_fold_tuple(nops, ops);
    if (base && base->type->tag == TYPE_TUPLE && base->type->nops == nops)
        return base;
    TMP_BUF_ALLOC(type_ops, const type_t*, nops)
    for (size_t i = 0; i < nops; ++i)
        type_ops[i] = ops[i]->type;
    const type_t* type = type_tuple(mod, nops, type_ops);
    TMP_BUF_FREE(type_ops);
    return make_node(mod, (node_t) {
        .tag  = NODE_TUPLE,
        .nops = nops,
        .ops  = ops,
        .type = type,
        .dbg  = dbg
    });
}

const node_t* node_array(mod_t* mod, size_t nops, const node_t** ops, const type_t* elem_type, const dbg_t* dbg) {
#ifndef NDEBUG
    for (size_t i = 0; i < nops; ++i)
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

const node_t* node_struct(mod_t* mod, const node_t* op, const type_t* type, const dbg_t* dbg) {
    assert(type->tag == TYPE_STRUCT);
    // Reminder: The operands of a struct type are *not* the types of its members
    return make_node(mod, (node_t) {
        .tag  = NODE_STRUCT,
        .nops = 1,
        .ops  = &op,
        .type = type,
        .dbg  = dbg
    });
}

const node_t* node_string(mod_t* mod, const char* str, const dbg_t* dbg) {
    size_t n = strlen(str) + 1;
    bool use_stack = n < 256;
    const node_t* stack_ops[use_stack ? n : 0];
    const node_t** ops = use_stack ? stack_ops : xmalloc(sizeof(const node_t*) * n);
    const type_t* char_type = type_prim(mod, TYPE_U8);
    for (size_t i = 0; i < n; ++i)
        ops[i] = node_literal(mod, char_type, (box_t) { .u8 = str[i] });
    const node_t* array = node_array(mod, n, ops, char_type, dbg);
    if (!use_stack) free(ops);
    return array;
}

const node_t* node_tuple_from_args(mod_t* mod, size_t nops, const dbg_t* dbg, ...) {
    TMP_BUF_ALLOC(ops, const node_t*, nops)
    va_list args;
    va_start(args, dbg);
    for (size_t i = 0; i < nops; ++i)
        ops[i] = va_arg(args, const node_t*);
    va_end(args);
    const node_t* node = node_tuple(mod, nops, ops, dbg);
    TMP_BUF_FREE(ops)
    return node;
}

const node_t* node_array_from_args(mod_t* mod, size_t nops, const type_t* elem_type, const dbg_t* dbg, ...) {
    TMP_BUF_ALLOC(ops, const node_t*, nops)
    va_list args;
    va_start(args, dbg);
    for (size_t i = 0; i < nops; ++i)
        ops[i] = va_arg(args, const node_t*);
    va_end(args);
    const node_t* node = node_array(mod, nops, ops, elem_type, dbg);
    TMP_BUF_FREE(ops)
    return node;
}

const node_t* node_extract(mod_t* mod, const node_t* value, const node_t* index, const dbg_t* dbg) {
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

    uint64_t index_value = index->tag == NODE_LITERAL ? node_value_u(index) : 0;
    if (value->type->tag == TYPE_TUPLE || value->type->tag == TYPE_STRUCT) {
        assert(index->tag == NODE_LITERAL);
        assert(index_value < type_member_count(value->type));
        elem_type = type_member(mod, value->type, index_value);
        // Constant folding
        if (value->tag == NODE_TUPLE)
            return value->ops[index_value];
        else if (value->tag == NODE_STRUCT)
            return node_extract(mod, value->ops[0], index, dbg);
        // Normalization of index types for tuples/structs
        if (index->type->tag != TYPE_U64)
            index = node_u64(mod, index_value);
    } else if (value->type->tag == TYPE_ARRAY) {
        elem_type = value->type->ops[0];
        if (value->tag == NODE_ARRAY && index->tag == NODE_LITERAL && index_value < value->nops) {
            return value->ops[index_value];
        } else if (value->tag == NODE_BOTTOM) {
            return node_bottom(mod, elem_type);
        }
    } else {
        // Tolerate extracts for non aggregate types when the index is zero
        assert(node_is_zero(index));
        return value;
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
    assert(type_is_u(index->type) || type_is_i(index->type));

    uint64_t index_value = index->tag == NODE_LITERAL ? node_value_u(index) : 0;
    if (value->type->tag == TYPE_TUPLE || value->type->tag == TYPE_STRUCT) {
        assert(index->tag == NODE_LITERAL);
        assert(index_value < type_member_count(value->type));
        assert(elem->type == type_member(mod, value->type, index_value));
        // Constant folding
        if (value->tag == NODE_TUPLE) {
            const node_t* ops[value->nops];
            for (size_t i = 0; i < value->nops; ++i)
                ops[i] = value->ops[i];
            ops[index_value] = elem;
            return node_tuple(mod, value->nops, ops, dbg);
        } else if (value->tag == NODE_STRUCT) {
            return node_struct(mod, node_insert(mod, value->ops[0], index, elem, dbg), value->type, dbg);
        }
        // Normalization of index types for tuples/structs
        if (index->type->tag != TYPE_U64)
            index = node_u64(mod, index_value);
    } else if (value->type->tag == TYPE_ARRAY) {
        assert(elem->type == value->type->ops[0]);
        if (value->tag == NODE_ARRAY && index->tag == NODE_LITERAL && index_value < value->nops) {
            const node_t* ops[value->nops];
            for (size_t i = 0; i < value->nops; ++i)
                ops[i] = value->ops[i];
            ops[index_value] = elem;
            return node_array(mod, value->nops, ops, elem->type, dbg);
        } else if (value->tag == NODE_BOTTOM) {
            return value;
        }
    } else {
        // Tolerate inserts for non aggregate types when the index is zero
        assert(node_is_zero(index));
        return elem;
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
    assert((value->type->tag == TYPE_PTR && type->tag == TYPE_PTR) || type_bitwidth(value->type) == type_bitwidth(type));
    while (value->tag == NODE_BITCAST && value->type != type)
        value = value->ops[0];
    if (value->type == type)
        return value;
    if (value->tag == NODE_LITERAL) {
        // TODO: Bitcast using memcpy()
        return node_literal(mod, type, value->data.box);
    }
    if (value->tag == NODE_BOTTOM)
        return node_bottom(mod, type);
    return make_node(mod, (node_t) {
        .tag  = NODE_BITCAST,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

#define EXTENDOP_I(value, type) \
    switch (type->tag) { \
        case TYPE_BOOL: return node_literal(mod, type, (box_t) { .b  = value != 0 ? true : false }); \
        case TYPE_I8:   return node_literal(mod, type, (box_t) { .i8  = value }); \
        case TYPE_I16:  return node_literal(mod, type, (box_t) { .i16 = value }); \
        case TYPE_I32:  return node_literal(mod, type, (box_t) { .i32 = value }); \
        case TYPE_I64:  return node_literal(mod, type, (box_t) { .i64 = value }); \
        default: \
            assert(false); \
            break; \
    }

#define EXTENDOP_U(value, type) \
    switch (type->tag) { \
        case TYPE_U8:  return node_literal(mod, type, (box_t) { .u8  = value }); \
        case TYPE_U16: return node_literal(mod, type, (box_t) { .u16 = value }); \
        case TYPE_U32: return node_literal(mod, type, (box_t) { .u32 = value }); \
        case TYPE_U64: return node_literal(mod, type, (box_t) { .u64 = value }); \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_extend(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert((type_is_i(type) && type_is_i(value->type)) ||
           (type_is_u(type) && type_is_u(value->type)));
    assert(type_bitwidth(value->type) <= type_bitwidth(type));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_BOOL:
                {
                    int8_t i = value->data.box.b ? -1 : 0;
                    EXTENDOP_I(i, type)
                }
                break;
            case TYPE_I8:  EXTENDOP_I(value->data.box.i8,  type) break;
            case TYPE_I16: EXTENDOP_I(value->data.box.i16, type) break;
            case TYPE_I32: EXTENDOP_I(value->data.box.i32, type) break;
            case TYPE_I64: EXTENDOP_I(value->data.box.i64, type) break;
            case TYPE_U8:  EXTENDOP_U(value->data.box.u8,  type) break;
            case TYPE_U16: EXTENDOP_U(value->data.box.u16, type) break;
            case TYPE_U32: EXTENDOP_U(value->data.box.u32, type) break;
            case TYPE_U64: EXTENDOP_U(value->data.box.u64, type) break;
            default:
                assert(false);
                break;
        }
    }
    if (type == value->type)
        return value;
    return make_node(mod, (node_t) {
        .tag  = NODE_EXTEND,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

#define TRUNCOP_I(value, type) \
    switch (type->tag) { \
        case TYPE_BOOL: return node_literal(mod, type, (box_t) { .b   = value & 1 ? true : false }); \
        case TYPE_I8:   return node_literal(mod, type, (box_t) { .i8  = value }); \
        case TYPE_I16:  return node_literal(mod, type, (box_t) { .i16 = value }); \
        case TYPE_I32:  return node_literal(mod, type, (box_t) { .i32 = value }); \
        case TYPE_I64:  return node_literal(mod, type, (box_t) { .i64 = value }); \
        default: \
            assert(false); \
            break; \
    }

#define TRUNCOP_U(value, type) \
    switch (type->tag) { \
        case TYPE_U8:  return node_literal(mod, type, (box_t) { .u8  = value }); \
        case TYPE_U16: return node_literal(mod, type, (box_t) { .u16 = value }); \
        case TYPE_U32: return node_literal(mod, type, (box_t) { .u32 = value }); \
        case TYPE_U64: return node_literal(mod, type, (box_t) { .u64 = value }); \
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
            case TYPE_BOOL:
                {
                    int8_t i = value->data.box.b ? -1 : 0;
                    TRUNCOP_I(i, type)
                }
                break;
            case TYPE_I8:  TRUNCOP_I(value->data.box.i8,  type) break;
            case TYPE_I16: TRUNCOP_I(value->data.box.i16, type) break;
            case TYPE_I32: TRUNCOP_I(value->data.box.i32, type) break;
            case TYPE_I64: TRUNCOP_I(value->data.box.i64, type) break;
            case TYPE_U8:  TRUNCOP_U(value->data.box.u8,  type) break;
            case TYPE_U16: TRUNCOP_U(value->data.box.u16, type) break;
            case TYPE_U32: TRUNCOP_U(value->data.box.u32, type) break;
            case TYPE_U64: TRUNCOP_U(value->data.box.u64, type) break;
            default:
                assert(false);
                break;
        }
    }
    if (type == value->type)
        return value;
    // trunc(extend(x : i32, i64), i32) <=> x
    if (value->tag == NODE_EXTEND) {
        const node_t* ext = value->ops[0];
        while (ext->tag == NODE_EXTEND && ext->type != type)
            ext = ext->ops[0];
        if (ext->type == type)
            return ext;
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
        case TYPE_F32: return node_literal(mod, type, (box_t) { .f32 = value }); \
        case TYPE_F64: return node_literal(mod, type, (box_t) { .f64 = value }); \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_itof(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert(type_is_f(type) && (type_is_i(value->type) || type_is_u(value->type)));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_BOOL:
                {
                    int8_t i = value->data.box.b ? -1 : 0;
                    ITOFOP(i, type)
                }
                break;
            case TYPE_I8:  ITOFOP(value->data.box.i8,  type) break;
            case TYPE_I16: ITOFOP(value->data.box.i16, type) break;
            case TYPE_I32: ITOFOP(value->data.box.i32, type) break;
            case TYPE_I64: ITOFOP(value->data.box.i64, type) break;
            case TYPE_U8:  ITOFOP(value->data.box.u8,  type) break;
            case TYPE_U16: ITOFOP(value->data.box.u16, type) break;
            case TYPE_U32: ITOFOP(value->data.box.u32, type) break;
            case TYPE_U64: ITOFOP(value->data.box.u64, type) break;
            default:
                assert(false);
                break;
        }
    }
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
        case TYPE_BOOL: return node_literal(mod, type, (box_t) { .b   = value != 0 ? true : false }); \
        case TYPE_I8:   return node_literal(mod, type, (box_t) { .i8  = value }); \
        case TYPE_I16:  return node_literal(mod, type, (box_t) { .i16 = value }); \
        case TYPE_I32:  return node_literal(mod, type, (box_t) { .i32 = value });  \
        case TYPE_I64:  return node_literal(mod, type, (box_t) { .i64 = value });  \
        case TYPE_U8:   return node_literal(mod, type, (box_t) { .u8  = value });  \
        case TYPE_U16:  return node_literal(mod, type, (box_t) { .u16 = value });  \
        case TYPE_U32:  return node_literal(mod, type, (box_t) { .u32 = value });  \
        case TYPE_U64:  return node_literal(mod, type, (box_t) { .u64 = value });  \
        default: \
            assert(false); \
            break; \
    }

const node_t* node_ftoi(mod_t* mod, const node_t* value, const type_t* type, const dbg_t* dbg) {
    assert(type_is_f(value->type) && (type_is_i(type) || type_is_u(type)));
    if (value->tag == NODE_LITERAL) {
        switch (value->type->tag) {
            case TYPE_F32: FTOIOP(value->data.box.f32, type) break;
            case TYPE_F64: FTOIOP(value->data.box.f64, type) break;
            default:
                assert(false);
                break;
        }
    }
    return make_node(mod, (node_t) {
        .tag  = NODE_FTOI,
        .nops = 1,
        .ops  = &value,
        .type = type,
        .dbg  = dbg
    });
}

static inline bool node_is_commutative(uint32_t tag) {
    switch (tag) {
        case NODE_ADD:
        case NODE_MUL:
        case NODE_AND:
        case NODE_OR:
        case NODE_XOR:
            return true;
        default: return false;
    }
}

static inline bool node_is_distributive(uint32_t tag1, uint32_t tag2, const type_t* type) {
    switch (tag1) {
        case NODE_MUL: return (tag2 == NODE_ADD || tag2 == NODE_SUB) &&
                              (!type_is_f(type) || (type->data.fp_flags & FP_ASSOCIATIVE_MATH));
        case NODE_AND: return tag2 == NODE_OR;
        case NODE_OR:  return tag2 == NODE_AND;
        default: return false;
    }
}

static inline bool node_can_switch_comparands(uint32_t tag, const type_t* type) {
    return tag == NODE_CMPEQ || !type_is_f(type) || (type->data.fp_flags & FP_NO_NAN_MATH);
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

#define CMPOP_B(op, res, left, right) \
    case TYPE_BOOL: res = left.b op right.b; break;

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

#define CMPOP_BIUF(op, res, left, right) \
    CMPOP_B(op, res, left, right) \
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
    assert(tag == NODE_CMPEQ || tag == NODE_CMPNE || left->type->tag != TYPE_BOOL);

    if (left->tag == NODE_LITERAL && right->tag == NODE_LITERAL) {
        bool res = false;
        switch (tag) {
            case NODE_CMPGT: CMPOP(left->type->tag, CMPOP_IUF (>,  res, left->data.box, right->data.box)) break;
            case NODE_CMPGE: CMPOP(left->type->tag, CMPOP_IUF (>=, res, left->data.box, right->data.box)) break;
            case NODE_CMPLT: CMPOP(left->type->tag, CMPOP_IUF (<,  res, left->data.box, right->data.box)) break;
            case NODE_CMPLE: CMPOP(left->type->tag, CMPOP_IUF (<=, res, left->data.box, right->data.box)) break;
            case NODE_CMPNE: CMPOP(left->type->tag, CMPOP_BIUF(!=, res, left->data.box, right->data.box)) break;
            case NODE_CMPEQ: CMPOP(left->type->tag, CMPOP_BIUF(==, res, left->data.box, right->data.box)) break;
            default:
                assert(false);
                break;
        }
        return node_bool(mod, res);
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
        if (tag == NODE_CMPNE || tag == NODE_CMPGT || tag == NODE_CMPLT) return node_bool(mod, false);
        // X == X <=> true
        // X >= X <=> true
        // X <= X <=> true
        if (tag == NODE_CMPEQ || tag == NODE_CMPGE || tag == NODE_CMPLE) return node_bool(mod, true);
    }
    if (type_is_u(left->type) && node_is_zero(left)) {
        // 0 > X <=> false
        if (tag == NODE_CMPGT) return node_bool(mod, false);
        // 0 <= X <=> true
        if (tag == NODE_CMPLE) return node_bool(mod, true);
    }

    const node_t* ops[] = { left, right };
    return make_node(mod, (node_t) {
        .tag = tag,
        .nops = 2,
        .ops  = ops,
        .type = type_bool(mod),
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

#define BINOP_B(op, res, left, right) \
    case TYPE_BOOL: res.b = left.b op right.b; break;

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

#define BINOP_BIU(op, res, left, right) \
    BINOP_B(op, res, left, right) \
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
    assert(is_bitwise || left->type->tag != TYPE_BOOL);
    assert((!is_bitwise && !is_shft) || !type_is_f(left->type));
    assert(tag != NODE_REM || !type_is_f(left->type));

    // Constant folding
    if (left->tag == NODE_LITERAL && right->tag == NODE_LITERAL) {
        box_t res;
        memset(&res, 0, sizeof(box_t));
        switch (tag) {
            case NODE_ADD:   BINOP(left->type->tag, BINOP_IUF(+,  res, left->data.box, right->data.box)) break;
            case NODE_SUB:   BINOP(left->type->tag, BINOP_IUF(-,  res, left->data.box, right->data.box)) break;
            case NODE_MUL:   BINOP(left->type->tag, BINOP_IUF(*,  res, left->data.box, right->data.box)) break;
            case NODE_DIV:   BINOP(left->type->tag, BINOP_IUF(/,  res, left->data.box, right->data.box)) break;
            case NODE_REM:   BINOP(left->type->tag, BINOP_IU (%,  res, left->data.box, right->data.box)) break;
            case NODE_AND:   BINOP(left->type->tag, BINOP_BIU(&,  res, left->data.box, right->data.box)) break;
            case NODE_OR:    BINOP(left->type->tag, BINOP_BIU(|,  res, left->data.box, right->data.box)) break;
            case NODE_XOR:   BINOP(left->type->tag, BINOP_BIU(^,  res, left->data.box, right->data.box)) break;
            case NODE_LSHFT: BINOP(left->type->tag, BINOP_IU (<<, res, left->data.box, right->data.box)) break;
            case NODE_RSHFT: BINOP(left->type->tag, BINOP_IU (>>, res, left->data.box, right->data.box)) break;
            default:
                assert(false);
                break;
        }
        return node_literal(mod, left->type, res);
    }

    if (left->tag  == NODE_BOTTOM) return left;
    if (right->tag == NODE_BOTTOM) return right;

    if (node_should_switch_ops(left, right) && node_is_commutative(tag))
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
        if (tag == NODE_REM) return node_zero(mod, left->type);
    }
    if (left == right) {
        // a & a <=> a
        // a | a <=> a
        if (tag == NODE_AND || tag == NODE_OR)  return left;
        // a ^ a <=> 0
        // a % a <=> 0
        // a - a <=> 0
        if (tag == NODE_XOR || tag == NODE_REM || tag == NODE_SUB) return node_zero(mod, left->type);
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
    // Factorizations
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
    if (both_factorizable) {
        const node_t* l1 = left->ops[0];
        const node_t* l2 = left->ops[1];
        const node_t* r1 = right->ops[0];
        const node_t* r2 = right->ops[1];
        assert(left->tag == right->tag);
        bool inner_commutative = node_is_commutative(left->tag);
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
    if (left->type->tag == TYPE_BOOL) {
        if (tag == NODE_AND) {
            // (A => B) => (A & B <=> A)
            if (node_implies(mod, left, right, false, false)) return left;
            // (B => A) => (A & B <=> B)
            if (node_implies(mod, right, left, false, false)) return right;
            // (A => ~B) => (A & B <=> 0)
            if (node_implies(mod, left, right, false, true)) return node_bool(mod, false);
            // (B => ~A) => (A & B <=> 0)
            if (node_implies(mod, left, right, false, true)) return node_bool(mod, false);
        } else if (tag == NODE_OR) {
            // (~A => ~B) => (A | B <=> A)
            if (node_implies(mod, left, right, true, true)) return left;
            // (~B => ~A) => (A | B <=> B)
            if (node_implies(mod, right, left, true, true)) return right;
            // (~A => B) => (A | B <=> 1)
            if (node_implies(mod, left, right, true, false)) return node_bool(mod, true);
            // (~B => A) => (A | B <=> 1)
            if (node_implies(mod, right, left, true, false)) return node_bool(mod, true);
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
NODE_BINOP(rem, NODE_REM)
NODE_BINOP(and, NODE_AND)
NODE_BINOP(or,  NODE_OR)
NODE_BINOP(xor, NODE_XOR)
NODE_BINOP(lshft, NODE_LSHFT)
NODE_BINOP(rshft, NODE_RSHFT)

const node_t* node_not(mod_t* mod, const node_t* node, const dbg_t* dbg) {
    return node_xor(mod, node_all_ones(mod, node->type), node, dbg);
}

bool node_has_mem(const node_t* node) {
    return node->tag == NODE_ALLOC ||
           node->tag == NODE_DEALLOC ||
           node->tag == NODE_LOAD ||
           node->tag == NODE_STORE;
}

const node_t* node_in_mem(const node_t* node) {
    assert(node_has_mem(node));
    return node->ops[0];
}

const node_t* node_out_mem(mod_t* mod, const node_t* node) {
    switch (node->tag) {
        case NODE_ALLOC:
        case NODE_LOAD:
            return node_extract(mod, node, node_i32(mod, 0), node->dbg);
        case NODE_STORE:
        case NODE_DEALLOC:
            return node;
        default:
            assert(false);
            return NULL;
    }
}

const node_t* node_from_mem(const node_t* node) {
    assert(node->type->tag == TYPE_MEM);
    switch (node->tag) {
        case NODE_EXTRACT:
            if (node->ops[0]->tag == NODE_LOAD ||
                node->ops[0]->tag == NODE_ALLOC)
                return node->ops[0];
            return NULL;
        case NODE_STORE:
        case NODE_DEALLOC:
            return node;
        default:
            return NULL;
    }
}

const node_t* node_alloc(mod_t* mod, const node_t* mem, const type_t* type, const dbg_t* dbg) {
    assert(mem->type->tag == TYPE_MEM);
    const type_t* type_ops[] = { mem->type, type_ptr(mod, type) };
    const type_t* alloc_type = type_tuple(mod, 2, type_ops);
    return make_node(mod, (node_t) {
        .tag  = NODE_ALLOC,
        .nops = 1,
        .ops  = &mem,
        .type = alloc_type,
        .dbg  = dbg
    });
}

const node_t* node_dealloc(mod_t* mod, const node_t* mem, const node_t* ptr, const dbg_t* dbg) {
    assert(mem->type->tag == TYPE_MEM);
    assert(ptr->type->tag == TYPE_PTR);
    assert(ptr->tag == NODE_EXTRACT && ptr->ops[0]->tag == NODE_ALLOC);
    const node_t* ops[] = { mem, ptr };
    return make_node(mod, (node_t) {
        .tag  = NODE_DEALLOC,
        .nops = 2,
        .ops  = ops,
        .type = mem->type,
        .dbg  = dbg
    });
}

const node_t* node_load(mod_t* mod, const node_t* mem, const node_t* ptr, const dbg_t* dbg) {
    assert(mem->type->tag == TYPE_MEM);
    assert(ptr->type->tag == TYPE_PTR);
    const type_t* load_type = type_tuple_from_args(mod, 2, mem->type, ptr->type->ops[0]);
    if (type_is_unit(load_type))
        return node_tuple_from_args(mod, 2, dbg, mem, node_unit(mod));
    const node_t* ops[] = { mem, ptr };
    return make_node(mod, (node_t) {
        .tag  = NODE_LOAD,
        .nops = 2,
        .ops  = ops,
        .type = load_type,
        .dbg  = dbg
    });
}

const node_t* node_store(mod_t* mod, const node_t* mem, const node_t* ptr, const node_t* val, const dbg_t* dbg) {
    assert(mem->type->tag == TYPE_MEM);
    assert(ptr->type->tag == TYPE_PTR);
    assert(val->type == ptr->type->ops[0]);
    const node_t* ops[] = { mem, ptr, val };
    if (type_is_unit(val->type))
        return mem;
    return make_node(mod, (node_t) {
        .tag  = NODE_STORE,
        .nops = 3,
        .ops  = ops,
        .type = mem->type,
        .dbg  = dbg
    });
}

const node_t* node_known(mod_t* mod, const node_t* node, const dbg_t* dbg) {
    if (node_is_const(node))
        return node_bool(mod, true);
    return make_node(mod, (node_t) {
        .tag  = NODE_KNOWN,
        .nops = 1,
        .ops  = &node,
        .type = type_bool(mod),
        .dbg  = dbg
    });
}

const node_t* node_select(mod_t* mod, const node_t* cond, const node_t* if_true, const node_t* if_false, const dbg_t* dbg) {
    assert(cond->type->tag == TYPE_BOOL);
    assert(if_true->type == if_false->type);
    if (cond->tag == NODE_LITERAL)
        return node_value_b(cond) ? if_true : if_false;
    if (cond->tag == NODE_BOTTOM)
        return if_true; // Arbitrary, could be if_false
    if (if_true == if_false)
        return if_true;
    // select(~a, b, c) => select(a, c, b)
    if (node_is_not(cond)) {
        cond = cond->ops[1];
        const node_t* tmp = if_true;
        if_true  = if_false;
        if_false = tmp;
    }
    const node_t* ops[] = { cond, if_true, if_false };
    return make_node(mod, (node_t) {
        .tag  = NODE_SELECT,
        .nops = 3,
        .ops  = ops,
        .type = if_true->type,
        .dbg  = dbg
    });
}

const node_t* node_fn(mod_t* mod, const type_t* type, uint32_t fn_flags, const dbg_t* dbg) {
    assert(type->tag == TYPE_FN);
    const node_t* ops[] = { node_bottom(mod, type->ops[1]), node_bool(mod, false) };
    return make_node(mod, (node_t) {
        .tag = NODE_FN,
        .nops = 2,
        .ops = ops,
        .type = type,
        .data = { .fn_flags = fn_flags },
        .dsize = sizeof(uint32_t),
        .dbg = dbg
    });
}

const node_t* node_param(mod_t* mod, const node_t* fn, const dbg_t* dbg) {
    assert(fn->tag == NODE_FN);
    return make_node(mod, (node_t) {
        .tag  = NODE_PARAM,
        .nops = 1,
        .ops  = &fn,
        .type = fn->type->ops[0],
        .dbg  = dbg
    });
}

const node_t* node_app(mod_t* mod, const node_t* callee, const node_t* arg, const node_t* cond, const dbg_t* dbg) {
    assert(callee->type->tag == TYPE_FN);
    assert(callee->type->ops[0] == arg->type);
    const node_t* ops[] = { callee, arg, cond };
    return make_node(mod, (node_t) {
        .tag  = NODE_APP,
        .nops = 3,
        .ops  = ops,
        .type = callee->type->ops[1],
        .dbg  = dbg
    });
}

const node_t* node_rebuild(mod_t* mod, const node_t* node, const node_t** ops, const type_t* type) {
    switch (node->tag) {
        case NODE_TOP:     return node_top(mod, type);
        case NODE_BOTTOM:  return node_bottom(mod, type);
        case NODE_LITERAL: return node_literal(mod, type, node->data.box);
        case NODE_DEALLOC: return node_dealloc(mod, ops[0], ops[1], node->dbg);
        case NODE_LOAD:    return node_load(mod, ops[0], ops[1], node->dbg);
        case NODE_STORE:   return node_store(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_TUPLE:   return node_tuple(mod, node->nops, ops, node->dbg);
        case NODE_ARRAY:   return node_array(mod, node->nops, ops, type->ops[0], node->dbg);
        case NODE_STRUCT:  return node_struct(mod, ops[0], type, node->dbg);
        case NODE_EXTRACT: return node_extract(mod, ops[0], ops[1], node->dbg);
        case NODE_INSERT:  return node_insert(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_BITCAST: return node_bitcast(mod, ops[0], type, node->dbg);
        case NODE_CMPGT:   return node_cmpgt(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPGE:   return node_cmpge(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPLT:   return node_cmplt(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPLE:   return node_cmple(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPNE:   return node_cmpne(mod, ops[0], ops[1], node->dbg);
        case NODE_CMPEQ:   return node_cmpeq(mod, ops[0], ops[1], node->dbg);
        case NODE_EXTEND:  return node_extend(mod, ops[0], type, node->dbg);
        case NODE_TRUNC:   return node_trunc(mod, ops[0], type, node->dbg);
        case NODE_ITOF:    return node_itof(mod, ops[0], type, node->dbg);
        case NODE_FTOI:    return node_ftoi(mod, ops[0], type, node->dbg);
        case NODE_ADD:     return node_add(mod, ops[0], ops[1], node->dbg);
        case NODE_SUB:     return node_sub(mod, ops[0], ops[1], node->dbg);
        case NODE_MUL:     return node_mul(mod, ops[0], ops[1], node->dbg);
        case NODE_DIV:     return node_div(mod, ops[0], ops[1], node->dbg);
        case NODE_REM:     return node_rem(mod, ops[0], ops[1], node->dbg);
        case NODE_AND:     return node_and(mod, ops[0], ops[1], node->dbg);
        case NODE_OR:      return node_or(mod, ops[0], ops[1], node->dbg);
        case NODE_XOR:     return node_xor(mod, ops[0], ops[1], node->dbg);
        case NODE_LSHFT:   return node_lshft(mod, ops[0], ops[1], node->dbg);
        case NODE_RSHFT:   return node_rshft(mod, ops[0], ops[1], node->dbg);
        case NODE_SELECT:  return node_select(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_PARAM:   return node_param(mod, ops[0], node->dbg);
        case NODE_APP:     return node_app(mod, ops[0], ops[1], ops[2], node->dbg);
        case NODE_KNOWN:   return node_known(mod, ops[0], node->dbg);
        case NODE_ALLOC:
            assert(type->tag == TYPE_TUPLE);
            assert(type->ops[0]->tag == TYPE_MEM);
            assert(type->ops[1]->tag == TYPE_PTR);
            return node_alloc(mod, ops[0], type->ops[1]->ops[0], node->dbg);
        default:
            assert(false);
            return NULL;
    }
}

void node_replace(const node_t* node, const node_t* with) {
    assert(node->type == with->type);
    while (with->rep) with = with->rep;
    if (with == node)
        return;
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

size_t use_count(const use_t* use) {
    size_t i = 0;
    while (use) {
        i++;
        use = use->next;
    }
    return i;
}
