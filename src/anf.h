#ifndef ANF_H
#define ANF_H

#include <stdint.h>
#include <stdbool.h>

#include "mpool.h"
#include "htable.h"

typedef union  box_u  box_t;
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

struct use_s {
    uint32_t id;
    node_t*  node;
    use_t*   next;
};

struct node_s {
    uint32_t tag;
    uint32_t nops;
    const type_t* type;
    use_t* uses;
};

struct type_s {
    uint32_t tag;
    uint32_t nops;
};

struct mod_s {
    mpool_t*  pool;
    htable_t* nodes;
    htable_t* types;
};

enum prim_e {
    PRIM_I1,
    PRIM_I8,
    PRIM_I16,
    PRIM_I32,
    PRIM_I64,
    PRIM_U8,
    PRIM_U16,
    PRIM_U32,
    PRIM_U64,
    PRIM_F32,
    PRIM_F64
};

enum binop_e {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_AND,
    BINOP_OR,
    BINOP_XOR,
    BINOP_LSHFT,
    BINOP_RSHFT,
    BINOP_CMPLT,
    BINOP_CMPGT,
    BINOP_CMPLE,
    BINOP_CMPGE,
    BINOP_CMPEQ
};

bool is_undef(const node_t*);
bool is_zero(const node_t*);
bool is_one(const node_t*);

const node_t* node_ops(const node_t*);
const type_t* type_ops(const type_t*);

// Module
mod_t* mod_create();
void mod_destroy(mod_t*);

// Types
const type_t* type_prim(mod_t*, uint32_t);
const type_t* type_tuple(mod_t*, ...);
const type_t* type_record(mod_t*, uint32_t, ...);
const type_t* type_fn(mod_t*, const type_t*, const type_t*);

// Values
const node_t* node_undef(mod_t*, const type_t*);
const node_t* node_literal(mod_t*, uint32_t, box_t);

// Aggregates
const node_t* node_tuple(mod_t*, ...);
const node_t* node_record(mod_t*, const type_t*, ...);
const node_t* node_extract(mod_t*, const node_t*, const node_t*);
const node_t* node_insert(mod_t*, const node_t*, const node_t*, const node_t*);
const node_t* node_extract_const(mod_t*, const node_t*, uint32_t);
const node_t* node_insert_const(mod_t*, const node_t*, uint32_t, const node_t*);

// Operations
const node_t* node_binop(mod_t*, uint32_t, const node_t*, const node_t*);

#endif // ANF_H
