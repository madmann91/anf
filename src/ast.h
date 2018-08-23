#ifndef AST_H
#define AST_H

#include "anf.h"
#include "lex.h"

typedef struct ast_s      ast_t;
typedef struct ast_list_s ast_list_t;
typedef struct parser_s   parser_t;

enum ast_tag_e {
    AST_LIT,
    AST_ID,
    AST_MOD,
    AST_DEF,
    AST_VAR,
    AST_BLOCK,
    AST_TUPLE,
    AST_ERR
};

struct ast_s {
    uint32_t tag;
    union {
        const char*  str;
        lit_t        lit;
        struct {
            ast_t*       id;
            ast_list_t*  asts;
        } mod;
        struct {
            ast_t*       id;
            ast_t*       param;
            ast_t*       body;
        } def;
        struct {
            ast_t*       ptrn;
            ast_t*       value;
        } var;
        struct {
            ast_list_t*  stmts;
        } block;
        struct {
            ast_list_t*  args;
        } tuple;
    } data;
    loc_t loc;
};

struct ast_list_s {
    ast_t*      ast;
    ast_list_t* next;
};

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

#endif // AST_H
