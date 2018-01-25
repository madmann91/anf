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
    const loc_t*  loc;
    const node_t** ops;
    const type_t* type;
};

struct type_s {
    uint32_t tag;
    uint32_t name;
    size_t   nops;
    const type_t** ops;
};

struct mod_s {
    mpool_t*  pool;
    htable_t* nodes;
    htable_t* types;
};

enum node_tag_e {
    NODE_UNDEF,
    NODE_LITERAL,
    NODE_TUPLE,
    NODE_RECORD,
    NODE_EXTRACT,
    NODE_INSERT,
    NODE_BINOP
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
    TYPE_FN
};

// Module
mod_t* mod_create(void);
void mod_destroy(mod_t*);

// Types
size_t type_bitwidth(const type_t*);
bool type_is_prim(const type_t*);
const type_t* type_i(mod_t*, size_t);
const type_t* type_u(mod_t*, size_t);
const type_t* type_f(mod_t*, size_t);
const type_t* type_tuple(mod_t*, size_t, const type_t**);
const type_t* type_fn(mod_t*, const type_t*, const type_t*);

// Values
bool node_is_undef(const node_t*);
bool node_is_zero(const node_t*);
bool node_is_one(const node_t*);

const node_t* node_undef(mod_t*, const type_t*);
const node_t* node_literal(mod_t*, const type_t*, box_t);

// Aggregates
const node_t* node_tuple(mod_t*, size_t, const node_t**, const loc_t*);
const node_t* node_record(mod_t*, const type_t*, size_t, const node_t**, const loc_t*);
const node_t* node_extract(mod_t*, const node_t*, const node_t*, const loc_t*);
const node_t* node_insert(mod_t*, const node_t*, const node_t*, const node_t*, const loc_t*);

// Operations
const node_t* node_binop(mod_t*, uint32_t, const node_t*, const node_t*, const loc_t*);

#endif // ANF_H
