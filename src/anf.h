#ifndef ANF_H
#define ANF_H

#include <stdint.h>
#include <stdbool.h>

#include "mpool.h"
#include "adt.h"

typedef union  box_u  box_t;
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

struct dbg_s {
    const char* name;
    const char* file;
    size_t brow;
    size_t bcol;
    size_t erow;
    size_t ecol;
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
};

enum node_tag_e {
    NODE_UNDEF,
    NODE_LITERAL,
    NODE_TUPLE,
    NODE_ARRAY,
    NODE_EXTRACT,
    NODE_INSERT,
    NODE_BITCAST,
    NODE_CMPGT,
    NODE_CMPGE,
    NODE_CMPLT,
    NODE_CMPLE,
    NODE_CMPNE,
    NODE_CMPEQ,
    NODE_WIDEN,
    NODE_TRUNC,
    NODE_ITOF,
    NODE_FTOI,
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_MOD,
    NODE_AND,
    NODE_OR,
    NODE_XOR,
    NODE_LSHFT,
    NODE_RSHFT,
    NODE_SELECT,
    NODE_FN,
    NODE_PARAM,
    NODE_APP,
    NODE_KNOWN
};

enum type_tag_e {
    TYPE_I1,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_TUPLE,
    TYPE_ARRAY,
    TYPE_FN
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
bool mod_save(const mod_t* mod, const char*);
mod_t* mod_load(const char*, mpool_t**);

void mod_import(mod_t*, mod_t*);
void mod_opt(mod_t**);

// Types
size_t type_bitwidth(const type_t*);
bool type_is_prim(const type_t*);
bool type_is_i(const type_t*);
bool type_is_u(const type_t*);
bool type_is_f(const type_t*);
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
const type_t* type_tuple(mod_t*, size_t, const type_t**);
const type_t* type_array(mod_t*, const type_t*);
const type_t* type_fn(mod_t*, const type_t*, const type_t*);

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
const node_t* node_array(mod_t*, size_t, const node_t**, const dbg_t*);
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
const node_t* node_mod(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_and(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_or(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_xor(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_not(mod_t*, const node_t*, const dbg_t*);
const node_t* node_lshft(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_rshft(mod_t*, const node_t*, const node_t*, const dbg_t*);

// Misc.
const node_t* node_known(mod_t*, const node_t*, const dbg_t*);
const node_t* node_select(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);

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
const type_t* type_rewrite(mod_t*, const type_t*, type2type_t*);
const node_t* node_rewrite(mod_t*, const node_t*, node2node_t*, type2type_t*, bool);
void node_replace(const node_t*, const node_t*);

// Uses
const use_t* use_find(const use_t*, size_t, const node_t*);
size_t node_count_uses(const node_t*);

// Debugging/Inspection
void type_print(const type_t*, bool);
void node_print(const node_t*, bool);
void type_dump(const type_t*);
void node_dump(const node_t*);

#endif // ANF_H
