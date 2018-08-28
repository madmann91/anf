#ifndef AST_H
#define AST_H

#include "lex.h"

typedef struct ast_s      ast_t;
typedef struct ast_list_s ast_list_t;

enum ast_tag_e {
    AST_ID,
    AST_LIT,
    AST_MOD,
    AST_DEF,
    AST_VAR,
    AST_VAL,
    AST_BLOCK,
    AST_TUPLE,
    AST_ERR
};

struct ast_s {
    uint32_t tag;
    union {
        struct {
            const char* str;
        } id;
        struct {
            const char* str;
            lit_t       value;
            bool        integer;
        } lit;
        struct {
            ast_t*      id;
            ast_list_t* decls;
        } mod;
        struct {
            ast_t*      id;
            ast_t*      param;
            ast_t*      value;
        } def;
        struct {
            ast_t*      ptrn;
            ast_t*      value;
        } varl;
        struct {
            ast_list_t* stmts;
        } block;
        struct {
            ast_list_t* args;
        } tuple;
    } data;
    loc_t loc;
};

struct ast_list_s {
    ast_t*      ast;
    ast_list_t* next;
};

bool ast_is_decl(ast_t*);
bool ast_is_expr(ast_t*);
bool ast_is_ptrn(ast_t*);
bool ast_is_refutable(ast_t*);

void ast_print(ast_t*, size_t, bool);
void ast_dump(ast_t*);

#endif // AST_H
