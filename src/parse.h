#ifndef PARSE_H
#define PARSE_H

#include "anf.h"
#include "ast.h"
#include "lex.h"

typedef struct parser_s parser_t;

struct parser_s {
    size_t    errs;
    loc_t     prev_loc;
    tok_t     ahead;
    lexer_t*  lexer;
    mpool_t** pool;
    void      (*error_fn)(parser_t*, const loc_t*, const char*);
};

ast_t* parse(parser_t*);
void parse_error(parser_t*, const loc_t*, const char*, ...);

#endif // PARSE_H
