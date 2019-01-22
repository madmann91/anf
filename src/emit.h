#ifndef EMIT_H
#define EMIT_H

#include "mod.h"
#include "node.h"
#include "ast.h"
#include "log.h"

typedef struct emitter_s emitter_t;

struct emitter_s {
    mod_t* mod;
    log_t* log;
    const node_t* mem;
    const node_t* cur;
    const node_t* break_;
    const node_t* continue_;
    const node_t* return_;
    const char* file;
};

const node_t* emit(emitter_t*, ast_t*);

#endif // EMIT_H
