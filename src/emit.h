#ifndef EMIT_H
#define EMIT_H

#include "mod.h"
#include "node.h"
#include "type.h"
#include "ast.h"

typedef struct emitter_s emitter_t;
typedef struct emitter_state_s emitter_state_t;

struct emitter_state_s {
    const node_t* mem;
    const node_t* cur;
    const node_t* brk;
    const node_t* cnt;
    const node_t* ret;
};

struct emitter_s {
    mod_t* mod;
    emitter_state_t state;
    type2type_t* types;
    const char* file;
};

const node_t* emit(emitter_t*, ast_t*);

#endif // EMIT_H
