#ifndef EMIT_H
#define EMIT_H

#include "mod.h"
#include "node.h"
#include "ast.h"

typedef struct emitter_s emitter_t;

struct emitter_s {
    mod_t* mod;
    const node_t* mem;
    const node_t* bb;
    const char* file;
};

const node_t* emit(emitter_t*, ast_t*);

#endif // EMIT_H
