#ifndef TYPE_H
#define TYPE_H

#include "mod.h"

#define PRIM_LIST(prefix, f) \
    f(prefix##BOOL,   "bool") \
    f(prefix##I8,     "i8") \
    f(prefix##I16,    "i16") \
    f(prefix##I32,    "i32") \
    f(prefix##I64,    "i64") \
    f(prefix##U8,     "u8") \
    f(prefix##U16,    "u16") \
    f(prefix##U32,    "u32") \
    f(prefix##U64,    "u64") \
    f(prefix##F32,    "f32") \
    f(prefix##F64,    "f64")

#define TYPE_LIST(f) \
    PRIM_LIST(TYPE_, f) \
    f(TYPE_TOP,    "top") \
    f(TYPE_BOTTOM, "bottom") \
    f(TYPE_MEM,    "mem") \
    f(TYPE_PTR,    "ptr") \
    f(TYPE_TUPLE,  "tuple") \
    f(TYPE_ARRAY,  "array") \
    f(TYPE_STRUCT, "struct") \
    f(TYPE_FN,     "fn") \
    f(TYPE_NORET,  "noret") \
    f(TYPE_VAR,    "var") \

typedef struct fp_flags_s   fp_flags_t;
typedef struct struct_def_s struct_def_t;
typedef struct enum_def_s   enum_def_t;

struct fp_flags_s {
    bool associative_math : 1;   // Assume associativity of floating point operations
    bool reciprocal_math  : 1;   // Allow the use of reciprocal for 1/x
    bool finite_math      : 1;   // Assume no infs
    bool no_nan_math      : 1;   // Assume no NaNs
};

struct struct_def_s {
    const char* name;
    bool byref;
    const type_t* members;
};

struct enum_def_s {
    void* empty; // TODO
};

enum type_tag_e {
#define TYPE(name, str) name,
    TYPE_LIST(TYPE)
#undef TYPE
};

struct type_s {
    uint32_t tag;
    size_t   nops;
    const type_t** ops;
    union {
        uint32_t      id;
        fp_flags_t    fp_flags;
        struct_def_t* struct_def;
        enum_def_t*   enum_def;
    } data;
};

fp_flags_t fp_flags_strict();
fp_flags_t fp_flags_relaxed();

size_t type_bitwidth(const type_t*);
bool type_is_unit(const type_t*);
bool type_is_prim(const type_t*);
bool type_is_i(const type_t*);
bool type_is_u(const type_t*);
bool type_is_f(const type_t*);
bool type_contains(const type_t*, const type_t*);
size_t type_order(const type_t*);
const type_t* type_members(mod_t*, const type_t*);

const type_t* type_bool(mod_t*);
const type_t* type_i8(mod_t*);
const type_t* type_i16(mod_t*);
const type_t* type_i32(mod_t*);
const type_t* type_i64(mod_t*);
const type_t* type_u8(mod_t*);
const type_t* type_u16(mod_t*);
const type_t* type_u32(mod_t*);
const type_t* type_u64(mod_t*);
const type_t* type_f32(mod_t*, fp_flags_t);
const type_t* type_f64(mod_t*, fp_flags_t);

const type_t* type_top(mod_t*);
const type_t* type_bottom(mod_t*);

const type_t* type_prim(mod_t*, uint32_t);
const type_t* type_prim_fp(mod_t*, uint32_t, fp_flags_t);
const type_t* type_mem(mod_t*);
const type_t* type_ptr(mod_t*, const type_t*);
const type_t* type_unit(mod_t*);
const type_t* type_tuple(mod_t*, size_t, const type_t**);
const type_t* type_tuple_args(mod_t*, size_t, ...);
const type_t* type_array(mod_t*, const type_t*);
const type_t* type_struct(mod_t*, struct_def_t*, size_t, const type_t**);
const type_t* type_fn(mod_t*, const type_t*, const type_t*);
const type_t* type_noret(mod_t*);
const type_t* type_var(mod_t*, uint32_t);

const type_t* type_rebuild(mod_t*, const type_t*, const type_t**);
const type_t* type_rewrite(mod_t*, const type_t*, type2type_t*);

void type_dump(const type_t*);

#endif // TYPE_H
