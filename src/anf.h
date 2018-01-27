#ifndef ANF_H
#define ANF_H

#include <stdint.h>
#include <stdbool.h>

#include "mpool.h"
#include "htable.h"

typedef union  box_u  box_t;
typedef struct loc_s  loc_t;
typedef struct node_s node_t;
typedef struct type_s type_t;
typedef struct use_s  use_t;
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
    const char* file;
    size_t brow;
    size_t bcol;
    size_t erow;
    size_t ecol;
};

struct use_s {
    uint32_t index;
    node_t*  node;
    use_t*   next;
};

struct node_s {
    uint32_t tag;
    size_t   nops;
    use_t*   uses;
    box_t    box;
    const node_t** ops;
    const type_t* type;
    const loc_t*  loc;
};

struct type_s {
    uint32_t tag;
    size_t   nops;
    const type_t** ops;
};

struct mod_s {
    mpool_t*  pool;
    htable_t* nodes;
    htable_t* types;

    bool commutative_fp  : 1;
    bool distributive_fp : 1;
};

enum node_tag_e {
    NODE_UNDEF,
    NODE_LITERAL,
    NODE_TUPLE,
    NODE_ARRAY,
    NODE_EXTRACT,
    NODE_INSERT,
    NODE_SELECT,
    NODE_BITCAST,
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_MOD,
    NODE_AND,
    NODE_OR,
    NODE_XOR,
    NODE_LSHFT,
    NODE_RSHFT
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

// Module
mod_t* mod_create(void);
void mod_destroy(mod_t*);

bool mod_is_commutative(const mod_t*, uint32_t, const type_t*);
bool mod_is_distributive(const mod_t* mod, uint32_t, uint32_t, const type_t*);

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
const node_t* node_tuple(mod_t*, size_t, const node_t**, const loc_t*);
const node_t* node_array(mod_t*, size_t, const node_t**, const loc_t*);
const node_t* node_extract(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_insert(mod_t*, const node_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_select(mod_t*, const node_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_bitcast(mod_t*, const node_t*, const type_t*, const loc_t*);
const node_t* node_add(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_sub(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_mul(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_div(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_mod(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_and(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_or(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_xor(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_lshft(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_rshft(mod_t*, const node_t*, const node_t*, const loc_t*);

#endif // ANF_H
