#include <stdarg.h>

#include "type.h"

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

bool type_contains(const type_t* type, const type_t* op) {
    for (size_t i = 0; i < type->nops; ++i) {
        if (type_contains(type->ops[i], op))
            return true;
    }
    return false;
}

size_t type_order(const type_t* type) {
    if (type->tag == TYPE_FN) {
        size_t dom   = type_order(type->ops[0]);
        size_t codom = type_order(type->ops[1]);
        return 1 + (dom > codom + 1 ? dom : codom + 1);
    } else if (type->tag == TYPE_NORET) {
        return -1;
    } else {
        size_t order = 0;
        for (size_t i = 0; i < type->nops; ++i) {
            size_t op = type_order(type->ops[i]);
            order = order > op ? order : op;
        }
        return order;
    }
}

static inline const type_t* make_type(mod_t* mod, type_t type) {
    return mod_insert_type(mod, &type);
}

const type_t* type_i1(mod_t* mod)  { return make_type(mod, (type_t) { .tag = TYPE_I1,  .nops = 0 }); }
const type_t* type_i8(mod_t* mod)  { return make_type(mod, (type_t) { .tag = TYPE_I8,  .nops = 0 }); }
const type_t* type_i16(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I16, .nops = 0 }); }
const type_t* type_i32(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I32, .nops = 0 }); }
const type_t* type_i64(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_I64, .nops = 0 }); }
const type_t* type_u8(mod_t* mod)  { return make_type(mod, (type_t) { .tag = TYPE_U8,  .nops = 0 }); }
const type_t* type_u16(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U16, .nops = 0 }); }
const type_t* type_u32(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U32, .nops = 0 }); }
const type_t* type_u64(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_U64, .nops = 0 }); }

const type_t* type_prim(mod_t* mod, uint32_t tag) {
    assert(tag != TYPE_F32 && tag != TYPE_F64);
    return make_type(mod, (type_t) { .tag = tag, .nops = 0 });
}

const type_t* type_prim_fp(mod_t* mod, uint32_t tag, fp_flags_t flags) {
    assert(tag == TYPE_F32 || tag == TYPE_F64);
    return make_type(mod, (type_t) { .tag = tag, .nops = 0, .data = { .fp_flags = flags } });
}

const type_t* type_mem(mod_t* mod) {
    return make_type(mod, (type_t) { .tag = TYPE_MEM, .nops = 0 });
}

const type_t* type_ptr(mod_t* mod, const type_t* pointee) {
    assert(pointee->tag != TYPE_MEM);
    return make_type(mod, (type_t) { .tag = TYPE_PTR, .nops = 1, .ops = &pointee });
}

const type_t* type_tuple(mod_t* mod, size_t nops, const type_t** ops) {
    if (nops == 1) return ops[0];
    return make_type(mod, (type_t) { .tag = TYPE_TUPLE, .nops = nops, .ops = ops });
}

const type_t* type_tuple_args(mod_t* mod, size_t nops, ...) {
    const type_t* ops[nops];
    va_list args;
    va_start(args, nops);
    for (size_t i = 0; i < nops; ++i)
        ops[i] = va_arg(args, const type_t*);
    va_end(args);
    return type_tuple(mod, nops, ops);
}

const type_t* type_array(mod_t* mod, const type_t* elem_type) {
    assert(elem_type->tag != TYPE_MEM);
    return make_type(mod, (type_t) { .tag = TYPE_ARRAY, .nops = 1, .ops = &elem_type });
}

const type_t* type_struct(mod_t* mod, uint32_t id, size_t nops, const type_t** ops) {
    return make_type(mod, (type_t) { .tag = TYPE_STRUCT, .nops = nops, .ops = ops, .data = { .id = id } });
}

const type_t* type_fn(mod_t* mod, const type_t* from, const type_t* to) {
    const type_t* ops[] = { from, to };
    return make_type(mod, (type_t) { .tag = TYPE_FN, .nops = 2, .ops = ops });
}

const type_t* type_noret(mod_t* mod) {
    return make_type(mod, (type_t) { .tag = TYPE_NORET, .nops = 0 });
}

const type_t* type_var(mod_t* mod, uint32_t id) {
    return make_type(mod, (type_t) { .tag = TYPE_VAR, .nops = 0, .data = { .id = id } });
}

const type_t* type_mod(mod_t* mod, uint32_t id) {
    return make_type(mod, (type_t) { .tag = TYPE_MOD, .nops = 0, .data = { .id = id } });
}

const type_t* type_rebuild(mod_t* mod, const type_t* type, const type_t** ops) {
    switch (type->tag) {
        case TYPE_PTR:    return type_ptr(mod, ops[0]);
        case TYPE_TUPLE:  return type_tuple(mod, type->nops, ops);
        case TYPE_ARRAY:  return type_array(mod, ops[0]);
        case TYPE_STRUCT: return type_struct(mod, type->data.id, type->nops, ops);
        case TYPE_FN:     return type_fn(mod, ops[0], ops[1]);
        default:
            return make_type(mod, (type_t) { .nops = 0, .tag = type->tag, .data = type->data });
    }
}
