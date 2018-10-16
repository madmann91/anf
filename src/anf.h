#ifndef ANF_H
#define ANF_H

#include <stdint.h>
#include <stdbool.h>

#include "mpool.h"
#include "adt.h"

#define NODE_LIST(f) \
    f(NODE_UNDEF,   "undef") \
    f(NODE_ALLOC,   "alloc") \
    f(NODE_DEALLOC, "dealloc") \
    f(NODE_LOAD,    "load") \
    f(NODE_STORE,   "store") \
    f(NODE_OFFSET,  "offset") \
    f(NODE_LITERAL, "literal") \
    f(NODE_TUPLE,   "tuple") \
    f(NODE_ARRAY,   "array") \
    f(NODE_STRUCT,  "struct") \
    f(NODE_EXTRACT, "extract") \
    f(NODE_INSERT,  "insert") \
    f(NODE_BITCAST, "bitcast") \
    f(NODE_CMPGT,   "cmpgt") \
    f(NODE_CMPGE,   "cmpge") \
    f(NODE_CMPLT,   "cmplt") \
    f(NODE_CMPLE,   "cmple") \
    f(NODE_CMPNE,   "cmpne") \
    f(NODE_CMPEQ,   "cmpeq") \
    f(NODE_WIDEN,   "widen") \
    f(NODE_TRUNC,   "trunc") \
    f(NODE_ITOF,    "itof") \
    f(NODE_FTOI,    "ftoi") \
    f(NODE_ADD,     "add") \
    f(NODE_SUB,     "sub") \
    f(NODE_MUL,     "mul") \
    f(NODE_DIV,     "div") \
    f(NODE_REM,     "rem") \
    f(NODE_AND,     "and") \
    f(NODE_OR,      "or") \
    f(NODE_XOR,     "xor") \
    f(NODE_LSHFT,   "lshft") \
    f(NODE_RSHFT,   "rshft") \
    f(NODE_SELECT,  "select") \
    f(NODE_FN,      "fn") \
    f(NODE_PARAM,   "param") \
    f(NODE_APP,     "app") \
    f(NODE_KNOWN,   "known") \
    f(NODE_TRAP,    "trap")

#define TYPE_LIST(f) \
    f(TYPE_I1,     "i1") \
    f(TYPE_I8,     "i8") \
    f(TYPE_I16,    "i16") \
    f(TYPE_I32,    "i32") \
    f(TYPE_I64,    "i64") \
    f(TYPE_U8,     "u8") \
    f(TYPE_U16,    "u16") \
    f(TYPE_U32,    "u32") \
    f(TYPE_U64,    "u64") \
    f(TYPE_F32,    "f32") \
    f(TYPE_F64,    "f64") \
    f(TYPE_MEM,    "mem") \
    f(TYPE_PTR,    "ptr") \
    f(TYPE_TUPLE,  "tuple") \
    f(TYPE_ARRAY,  "array") \
    f(TYPE_STRUCT, "struct") \
    f(TYPE_FN,     "fn") \
    f(TYPE_NORET,  "noret") \
    f(TYPE_VAR,    "var")

typedef union  box_u  box_t;
typedef struct loc_s  loc_t;
typedef struct dbg_s  dbg_t;
typedef struct use_s  use_t;
typedef struct node_s node_t;
typedef struct fn_s   fn_t;
typedef struct type_s type_t;
typedef struct mod_s  mod_t;

union box_u {
    bool     i1;
    int8_t   i8;
    int16_t  i16;
    int32_t  i32;
    int64_t  i64;
    uint8_t  u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float    f32;
    double   f64;
};

struct loc_s {
    size_t brow;
    size_t bcol;
    size_t erow;
    size_t ecol;
};

struct dbg_s {
    const char* name;
    const char* file;
    loc_t       loc;
};

struct use_s {
    size_t index;
    const node_t* user;
    use_t* next;
};

struct node_s {
    uint32_t tag;
    size_t   nops;
    use_t*   uses;
    box_t    box;

    const node_t*  rep;
    const node_t** ops;
    const type_t*  type;
    const dbg_t*   dbg;
};

struct fn_s {
    const node_t node;
    bool exported  : 1;
    bool imported  : 1;
    bool intrinsic : 1;
};

struct type_s {
    uint32_t tag;
    size_t   nops;
    const type_t** ops;
    bool fast : 1;
    uint32_t var : 31;
};

enum node_tag_e {
#define NODE(name, str) name,
    NODE_LIST(NODE)
#undef NODE
};

enum type_tag_e {
#define TYPE(name, str) name,
    TYPE_LIST(TYPE)
#undef TYPE
};

enum rewrite_flag_e {
    REWRITE_FNS     = 0x01,
    REWRITE_STRUCTS = 0x02
};

bool type_cmp(const void*, const void*);
bool node_cmp(const void*, const void*);
uint32_t type_hash(const void*);
uint32_t node_hash(const void*);

VEC(type_vec, const type_t*)
VEC(node_vec, const node_t*)
VEC(fn_vec, fn_t*)
HSET(internal_type_set, const type_t*, type_cmp, type_hash)
HSET(internal_node_set, const node_t*, node_cmp, node_hash)
HSET_DEFAULT(type_set, const type_t*)
HSET_DEFAULT(node_set, const node_t*)
HMAP_DEFAULT(type2type, const type_t*, const type_t*)
HMAP_DEFAULT(node2node, const node_t*, const node_t*)

struct mod_s {
    mpool_t*  pool;
    internal_node_set_t nodes;
    internal_type_set_t types;
    fn_vec_t fns;
};

// Module
mod_t* mod_create(void);
void mod_destroy(mod_t*);

// Optimization
void mod_import(mod_t*, mod_t*);
void mod_opt(mod_t**);
void mod_cleanup(mod_t**);

// Types
size_t type_bitwidth(const type_t*);
bool type_is_prim(const type_t*);
bool type_is_i(const type_t*);
bool type_is_u(const type_t*);
bool type_is_f(const type_t*);
bool type_contains(const type_t*, const type_t*);
const type_t* type_i1(mod_t*);
const type_t* type_i8(mod_t*);
const type_t* type_i16(mod_t*);
const type_t* type_i32(mod_t*);
const type_t* type_i64(mod_t*);
const type_t* type_u8(mod_t*);
const type_t* type_u16(mod_t*);
const type_t* type_u32(mod_t*);
const type_t* type_u64(mod_t*);
const type_t* type_f32(mod_t*);
const type_t* type_f64(mod_t*);
const type_t* type_mem(mod_t*);
const type_t* type_ptr(mod_t*, const type_t*);
const type_t* type_tuple(mod_t*, size_t, const type_t**);
const type_t* type_tuple_args(mod_t*, size_t, ...);
const type_t* type_array(mod_t*, const type_t*);
type_t* type_struct(mod_t*, size_t);
const type_t* type_fn(mod_t*, const type_t*, const type_t*);
const type_t* type_noret(mod_t*);
const type_t* type_var(mod_t*, uint32_t);

// Values
uint64_t node_value_u(const node_t*);
int64_t  node_value_i(const node_t*);
double   node_value_f(const node_t*);
bool node_is_const(const node_t*);
bool node_is_zero(const node_t*);
bool node_is_one(const node_t*);
bool node_is_all_ones(const node_t*);
const node_t* node_undef(mod_t*, const type_t*);
const node_t* node_zero(mod_t*, const type_t*);
const node_t* node_one(mod_t*, const type_t*);
const node_t* node_all_ones(mod_t*, const type_t*);
const node_t* node_i1(mod_t*, bool);
const node_t* node_i8(mod_t*, int8_t);
const node_t* node_i16(mod_t*, int16_t);
const node_t* node_i32(mod_t*, int32_t);
const node_t* node_i64(mod_t*, int64_t);
const node_t* node_u8(mod_t*, uint8_t);
const node_t* node_u16(mod_t*, uint16_t);
const node_t* node_u32(mod_t*, uint32_t);
const node_t* node_u64(mod_t*, uint64_t);
const node_t* node_f32(mod_t*, float);
const node_t* node_f64(mod_t*, double);

// Operations
bool node_is_not(const node_t*);
bool node_is_cmp(const node_t*);
bool node_implies(mod_t*, const node_t*, const node_t*, bool, bool);
const node_t* node_tuple(mod_t*, size_t, const node_t**, const dbg_t*);
const node_t* node_array(mod_t*, size_t, const node_t**, const type_t*, const dbg_t*);
const node_t* node_struct(mod_t*, size_t, const node_t**, const type_t*, const dbg_t*);
const node_t* node_tuple_args(mod_t*, size_t, const dbg_t*, ...);
const node_t* node_array_args(mod_t*, size_t, const type_t*, const dbg_t*, ...);
const node_t* node_string(mod_t*, const char*, const dbg_t*);
const node_t* node_extract(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_insert(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_bitcast(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_widen(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_trunc(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_itof(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_ftoi(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_cmpgt(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_cmpge(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_cmplt(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_cmple(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_cmpne(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_cmpeq(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_add(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_sub(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_mul(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_div(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_rem(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_and(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_or(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_xor(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_not(mod_t*, const node_t*, const dbg_t*);
const node_t* node_lshft(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_rshft(mod_t*, const node_t*, const node_t*, const dbg_t*);

// Memory operations
bool node_has_mem(const node_t*);
const node_t* node_in_mem(const node_t*);
const node_t* node_out_mem(mod_t*, const node_t*);
const node_t* node_from_mem(const node_t*);
const node_t* node_alloc(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_dealloc(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_load(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_store(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_offset(mod_t*, const node_t*, const node_t*, const dbg_t*);

// Misc.
const node_t* node_known(mod_t*, const node_t*, const dbg_t*);
const node_t* node_select(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_trap(mod_t*, const node_t*, const type_t*, const dbg_t*);

// Functions
void fn_bind(mod_t*, fn_t*, size_t, const node_t*);
fn_t* fn_cast(const node_t*);
const node_t* fn_inline(mod_t*, const fn_t*, const node_t*);
fn_t* node_fn(mod_t*, const type_t*, const dbg_t*);
const node_t* node_param(mod_t*, const fn_t*, const dbg_t*);
const node_t* node_app(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);

// Rebuild/Rewrite/Replace
const type_t* type_rebuild(mod_t*, const type_t*, const type_t**);
const node_t* node_rebuild(mod_t*, const node_t*, const node_t**, const type_t*);
const type_t* type_rewrite(mod_t*, const type_t*, type2type_t*, uint32_t);
const node_t* node_rewrite(mod_t*, const node_t*, node2node_t*, type2type_t*, uint32_t);
void node_replace(const node_t*, const node_t*);

// Uses
const use_t* use_find(const use_t*, size_t, const node_t*);
size_t node_count_uses(const node_t*);

#endif // ANF_H
