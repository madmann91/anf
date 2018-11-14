#ifndef PARSE_H
#define PARSE_H

#include "anf.h"
#include "ast.h"
#include "lex.h"
#include "log.h"

typedef struct parser_s parser_t;

struct parser_s {
    loc_t     prev_loc;
    tok_t     ahead;
    log_t*    log;
    lexer_t*  lexer;
    mpool_t** pool;
};

ast_t* parse(parser_t*);

#endif // PARSE_H
