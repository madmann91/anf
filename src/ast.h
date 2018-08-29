#ifndef AST_H
#define AST_H

#include "anf.h"
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
    AST_BINOP,
    AST_UNOP,
    AST_LAMBDA,
    AST_IF,
    AST_MATCH,
    AST_WHILE,
    AST_FOR,
    AST_ERR
};

enum unop_tag_e {
    UNOP_NEG,
    UNOP_PLUS,
    UNOP_PRE_INC,
    UNOP_PRE_DEC,
    UNOP_POST_INC,
    UNOP_POST_DEC,
    UNOP_DEREF,
    UNOP_TAKE_ADDR
};

enum binop_tag_e {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_REM,
    BINOP_AND,
    BINOP_OR,
    BINOP_XOR,
    BINOP_LSHFT,
    BINOP_RSHFT,
    BINOP_ASSIGN_ADD,
    BINOP_ASSIGN_SUB,
    BINOP_ASSIGN_MUL,
    BINOP_ASSIGN_DIV,
    BINOP_ASSIGN_REM,
    BINOP_ASSIGN_AND,
    BINOP_ASSIGN_OR,
    BINOP_ASSIGN_XOR,
    BINOP_ASSIGN_LSHFT,
    BINOP_ASSIGN_RSHFT,
    BINOP_LOGIC_AND,
    BINOP_LOGIC_OR,
    BINOP_EQ,
    BINOP_LT,
    BINOP_GT,
    BINOP_GE,
    BINOP_LE
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
        struct {
            uint32_t    tag;
            ast_t*      left;
            ast_t*      right;
        } binop;
        struct {
            uint32_t    tag;
            ast_t*      op;
        } unop;
        struct {
            ast_t*      param;
            ast_t*      body;
        } lambda;
        struct {
            ast_t*      if_true;
            ast_t*      if_false;
        } if_;
        struct {
            ast_t*      cond;
            ast_t*      body;
        } while_;
        struct {
            ast_t*      vars;
            ast_t*      call;
            ast_t*      body;
        } for_;
    } data;
    const type_t* type;
    loc_t loc;
};

struct ast_list_s {
    ast_t*      ast;
    ast_list_t* next;
};

bool ast_is_refutable(const ast_t*);

void ast_print(const ast_t*, size_t, bool);
void ast_dump(const ast_t*);

#endif // AST_H
