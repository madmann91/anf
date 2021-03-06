#ifndef CHECK_H
#define CHECK_H

#include "ast.h"
#include "log.h"

typedef struct checker_s checker_t;

struct checker_s {
    log_t* log;
    ast_set_t* defs;
    mod_t* mod;
};

const type_t* check(checker_t*, ast_t*, const type_t*);
const type_t* infer(checker_t*, ast_t*);

#endif // CHECK_H
