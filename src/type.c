#include <stdarg.h>

#include "type.h"

size_t type_bitwidth(const type_t* type) {
    switch (type->tag) {
        case TYPE_BOOL: return 1;
        case TYPE_I8:   return 8;
        case TYPE_I16:  return 16;
        case TYPE_I32:  return 32;
        case TYPE_I64:  return 64;
        case TYPE_U8:   return 8;
        case TYPE_U16:  return 16;
        case TYPE_U32:  return 32;
        case TYPE_U64:  return 64;
        case TYPE_F32:  return 32;
        case TYPE_F64:  return 64;
        default:
            assert(false);
            return -1;
    }
}

bool type_is_unit(const type_t* type) {
    return type->tag == TYPE_TUPLE && type->nops == 0;
}

bool type_is_prim(const type_t* type) {
    switch (type->tag) {
        case TYPE_BOOL: return true;
        case TYPE_I8:   return true;
        case TYPE_I16:  return true;
        case TYPE_I32:  return true;
        case TYPE_I64:  return true;
        case TYPE_U8:   return true;
        case TYPE_U16:  return true;
        case TYPE_U32:  return true;
        case TYPE_U64:  return true;
        case TYPE_F32:  return true;
        case TYPE_F64:  return true;
        default:        return false;
    }
}

bool type_is_i(const type_t* type) {
    switch (type->tag) {
        case TYPE_BOOL: return true;
        case TYPE_I8:   return true;
        case TYPE_I16:  return true;
        case TYPE_I32:  return true;
        case TYPE_I64:  return true;
        default:        return false;
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

bool type_is_subtype(const type_t* src, const type_t* dst) {
    return src == dst || src->tag == TYPE_BOTTOM || dst->tag == TYPE_TOP;
}

bool type_contains(const type_t* type, const type_t* op) {
    if (type == op)
        return true;
    for (size_t i = 0; i < type->nops; ++i) {
        if (type_contains(type->ops[i], op))
            return true;
    }
    return false;
}

size_t type_arg_count(const type_t* type) {
    return type->tag == TYPE_TUPLE ? type->nops : 1;
}

size_t type_order(const type_t* type) {
    if (type->tag == TYPE_FN) {
        size_t dom   = type_order(type->ops[0]);
        size_t codom = type_order(type->ops[1]);
        return 1 + (dom > codom + 1 ? dom : codom + 1);
    } else if (type->tag == TYPE_BOTTOM) {
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

const type_t* type_arg(const type_t* type, size_t index) {
    assert(index == 0 || (type->tag == TYPE_TUPLE && index < type->nops));
    return type->tag == TYPE_TUPLE ? type->ops[index] : type;
}

const type_t* type_member(mod_t* mod, const type_t* type, size_t index) {
    assert(type->tag == TYPE_STRUCT);
    if (type->nops == 0)
        return type_arg(type->data.struct_def->members, index);
    type2type_t type2type = type2type_create();
    for (size_t i = 0; i < type->nops; ++i) {
        const type_t* var = type_var(mod, i);
        if (var != type->ops[i])
            type2type_insert(&type2type, var, type->ops[i]);
    }
    const type_t* member = type_rewrite(mod, type_arg(type->data.struct_def->members, index), &type2type);
    type2type_destroy(&type2type);
    return member;
}

static inline const type_t* make_type(mod_t* mod, type_t type) {
    return mod_insert_type(mod, &type);
}

const type_t* type_bool(mod_t* mod) { return type_prim(mod, TYPE_BOOL ); }
const type_t* type_i8(mod_t* mod)   { return type_prim(mod, TYPE_I8 ); }
const type_t* type_i16(mod_t* mod)  { return type_prim(mod, TYPE_I16); }
const type_t* type_i32(mod_t* mod)  { return type_prim(mod, TYPE_I32); }
const type_t* type_i64(mod_t* mod)  { return type_prim(mod, TYPE_I64); }
const type_t* type_u8(mod_t* mod)   { return type_prim(mod, TYPE_U8 ); }
const type_t* type_u16(mod_t* mod)  { return type_prim(mod, TYPE_U16); }
const type_t* type_u32(mod_t* mod)  { return type_prim(mod, TYPE_U32); }
const type_t* type_u64(mod_t* mod)  { return type_prim(mod, TYPE_U64); }

const type_t* type_f32(mod_t* mod, uint32_t fp_flags) { return type_prim_fp(mod, TYPE_F32, fp_flags); }
const type_t* type_f64(mod_t* mod, uint32_t fp_flags) { return type_prim_fp(mod, TYPE_F64, fp_flags); }

const type_t* type_top(mod_t* mod)    { return make_type(mod, (type_t) { .tag = TYPE_TOP,    .nops = 0 }); }
const type_t* type_bottom(mod_t* mod) { return make_type(mod, (type_t) { .tag = TYPE_BOTTOM, .nops = 0 }); }

const type_t* type_prim(mod_t* mod, uint32_t tag) {
    assert(tag != TYPE_F32 && tag != TYPE_F64);
    return make_type(mod, (type_t) { .tag = tag, .nops = 0 });
}

const type_t* type_prim_fp(mod_t* mod, uint32_t tag, uint32_t fp_flags) {
    assert(tag == TYPE_F32 || tag == TYPE_F64);
    return make_type(mod, (type_t) { .tag = tag, .nops = 0, .data = { .fp_flags = fp_flags }, .dsize = sizeof(uint32_t) });
}

const type_t* type_mem(mod_t* mod) {
    return make_type(mod, (type_t) { .tag = TYPE_MEM, .nops = 0 });
}

const type_t* type_ptr(mod_t* mod, const type_t* pointee) {
    assert(pointee->tag != TYPE_MEM);
    return make_type(mod, (type_t) { .tag = TYPE_PTR, .nops = 1, .ops = &pointee });
}

const type_t* type_unit(mod_t* mod) {
    return type_tuple(mod, 0, NULL);
}

const type_t* type_tuple(mod_t* mod, size_t nops, const type_t** ops) {
    if (nops == 1) return ops[0];
    return make_type(mod, (type_t) { .tag = TYPE_TUPLE, .nops = nops, .ops = ops });
}

const type_t* type_tuple_from_args(mod_t* mod, size_t nops, ...) {
    const type_t* ops[nops];
    va_list args;
    va_start(args, nops);
    for (size_t i = 0; i < nops; ++i)
        ops[i] = va_arg(args, const type_t*);
    va_end(args);
    return type_tuple(mod, nops, ops);
}

const type_t* type_tuple_from_struct(mod_t* mod, const type_t* type) {
    assert(type->tag == TYPE_STRUCT);
    const struct_def_t* struct_def = type->data.struct_def;
    if (type->nops == 0)
        return struct_def->members;
    type2type_t type2type = type2type_create();
    for (size_t i = 0; i < type->nops; ++i) {
        const type_t* var = type_var(mod, i);
        if (var != type->ops[i])
            type2type_insert(&type2type, var, type->ops[i]);
    }
    const type_t* tuple_type = type_rewrite(mod, struct_def->members, &type2type);
    type2type_destroy(&type2type);
    return tuple_type;
}

const type_t* type_array(mod_t* mod, uint32_t dim, const type_t* elem_type) {
    assert(elem_type->tag != TYPE_MEM);
    return make_type(mod, (type_t) { .tag = TYPE_ARRAY, .nops = 1, .ops = &elem_type, .data = { .dim = dim }, .dsize = sizeof(uint32_t) });
}

const type_t* type_struct(mod_t* mod, struct_def_t* struct_def, size_t nops, const type_t** ops) {
    return make_type(mod, (type_t) { .tag = TYPE_STRUCT, .nops = nops, .ops = ops, .data = { .struct_def = struct_def }, .dsize = sizeof(struct_def_t*) });
}

const type_t* type_fn(mod_t* mod, const type_t* from, const type_t* to) {
    const type_t* ops[] = { from, to };
    return make_type(mod, (type_t) { .tag = TYPE_FN, .nops = 2, .ops = ops });
}

const type_t* type_var(mod_t* mod, uint32_t var) {
    return make_type(mod, (type_t) { .tag = TYPE_VAR, .nops = 0, .data = { .var = var }, .dsize = sizeof(uint32_t) });
}

const type_t* type_rebuild(mod_t* mod, const type_t* type, const type_t** ops) {
    switch (type->tag) {
        case TYPE_PTR:    return type_ptr(mod, ops[0]);
        case TYPE_TUPLE:  return type_tuple(mod, type->nops, ops);
        case TYPE_ARRAY:  return type_array(mod, type->data.dim, ops[0]);
        case TYPE_STRUCT: return type_struct(mod, type->data.struct_def, type->nops, ops);
        case TYPE_FN:     return type_fn(mod, ops[0], ops[1]);
        default:
            assert(type->nops == 0);
            return make_type(mod, (type_t) { .nops = 0, .tag = type->tag, .data = type->data, .dsize = type->dsize });
    }
}

const type_t* type_rewrite(mod_t* mod, const type_t* type, type2type_t* type2type) {
    const type_t** found = type2type_lookup(type2type, type);
    if (found)
        return *found;
    const type_t* new_ops[type->nops];
    for (size_t i = 0; i < type->nops; ++i)
        new_ops[i] = type_rewrite(mod, type->ops[i], type2type);
    const type_t* new_type = type_rebuild(mod, type, new_ops);
    type2type_insert(type2type, type, new_type);
    return new_type;
}
