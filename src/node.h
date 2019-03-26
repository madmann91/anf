#ifndef NODE_H
#define NODE_H

#include "mod.h"
#include "type.h"

#define NODE_LIST(f) \
    f(NODE_TOP,     "top") \
    f(NODE_BOTTOM,  "bottom") \
    f(NODE_ALLOC,   "alloc") \
    f(NODE_DEALLOC, "dealloc") \
    f(NODE_LOAD,    "load") \
    f(NODE_STORE,   "store") \
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
    f(NODE_EXTEND,  "extend") \
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
    f(NODE_TAPP,    "tapp") \
    f(NODE_KNOWN,   "known")

typedef union  box_u box_t;
typedef struct loc_s loc_t;
typedef struct dbg_s dbg_t;
typedef struct use_s use_t;

enum node_tag_e {
#define NODE(name, str) name,
    NODE_LIST(NODE)
#undef NODE
};

union box_u {
    bool     b;
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

enum fn_flags_e {
    FN_EXPORTED = 0x01,  // The function is visible in other modules
    FN_IMPORTED = 0x02,  // The function is from another module
    FN_INTRINSIC = 0x04  // The function is a built-in intrinsic
};

struct node_s {
    uint32_t tag;
    size_t   nops;
    use_t*   uses;
    union {
        box_t box;
        uint32_t fn_flags;
        const type_t* map;
    } data;
    size_t dsize;
    const node_t*  rep;
    const node_t** ops;
    const type_t*  type;
    const dbg_t*   dbg;
};

uint64_t node_value_u(const node_t*);
int64_t  node_value_i(const node_t*);
double   node_value_f(const node_t*);
bool     node_value_b(const node_t*);
bool node_is_const(const node_t*);
bool node_is_zero(const node_t*);
bool node_is_one(const node_t*);
bool node_is_all_ones(const node_t*);
const node_t* node_top(mod_t*, const type_t*);
const node_t* node_bottom(mod_t*, const type_t*);
const node_t* node_zero(mod_t*, const type_t*);
const node_t* node_one(mod_t*, const type_t*);
const node_t* node_all_ones(mod_t*, const type_t*);
const node_t* node_bool(mod_t*, bool);
const node_t* node_i8(mod_t*, int8_t);
const node_t* node_i16(mod_t*, int16_t);
const node_t* node_i32(mod_t*, int32_t);
const node_t* node_i64(mod_t*, int64_t);
const node_t* node_u8(mod_t*, uint8_t);
const node_t* node_u16(mod_t*, uint16_t);
const node_t* node_u32(mod_t*, uint32_t);
const node_t* node_u64(mod_t*, uint64_t);
const node_t* node_f32(mod_t*, float, uint32_t);
const node_t* node_f64(mod_t*, double, uint32_t);
const node_t* node_literal(mod_t*, const type_t*, box_t);

bool node_is_unit(const node_t*);
bool node_is_not(const node_t*);
bool node_is_cmp(const node_t*);
bool node_implies(mod_t*, const node_t*, const node_t*, bool, bool);
const node_t* node_unit(mod_t*);
const node_t* node_tuple(mod_t*, size_t, const node_t**, const dbg_t*);
const node_t* node_array(mod_t*, size_t, const node_t**, const type_t*, const dbg_t*);
const node_t* node_struct(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_tuple_from_args(mod_t*, size_t, const dbg_t*, ...);
const node_t* node_array_from_args(mod_t*, size_t, const type_t*, const dbg_t*, ...);
const node_t* node_string(mod_t*, const char*, const dbg_t*);
const node_t* node_extract(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_insert(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_bitcast(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_extend(mod_t*, const node_t*, const type_t*, const dbg_t*);
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

bool node_has_mem(const node_t*);
const node_t* node_in_mem(const node_t*);
const node_t* node_out_mem(mod_t*, const node_t*);
const node_t* node_from_mem(const node_t*);
const node_t* node_alloc(mod_t*, const node_t*, const type_t*, const dbg_t*);
const node_t* node_dealloc(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_load(mod_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_store(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);

const node_t* node_known(mod_t*, const node_t*, const dbg_t*);
const node_t* node_select(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);

const node_t* node_fn(mod_t*, const type_t*, uint32_t, const dbg_t*);
const node_t* node_param(mod_t*, const node_t*, const dbg_t*);
const node_t* node_app(mod_t*, const node_t*, const node_t*, const node_t*, const dbg_t*);
const node_t* node_tapp(mod_t*, const node_t*, const type_t*, const type_t*, const dbg_t*);

const node_t* node_rebuild(mod_t*, const node_t*, const node_t**, const type_t*);
const node_t* node_rewrite(mod_t*, const node_t*, node2node_t*, type2type_t*, uint32_t);
void node_replace(const node_t*, const node_t*);

const use_t* use_find(const use_t*, size_t, const node_t*);
size_t use_count(const use_t*);

void node_dump(const node_t*);

#endif // NODE_H
