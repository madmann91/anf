#ifndef PARSE_H
#define PARSE_H

#include "anf.h"
#include "ast.h"

typedef struct parser_s parser_t;

struct parser_s {
    size_t    errs;
    loc_t     prev_loc;
    tok_t     ahead;
    lex_t*    lex;
    mpool_t** pool;
    void      (*error_fn)(parser_t*, const char*);
};

ast_t* parse(parser_t*);
void parse_error(parser_t*, const char*, ...);

#endif // PARSE_H
