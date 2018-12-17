#ifndef AST_H
#define AST_H

#include "type.h"
#include "lex.h"

typedef struct ast_s      ast_t;
typedef struct ast_list_s ast_list_t;

#define MAX_BINOP_PRECEDENCE 10
#define INVALID_TAG ((uint32_t)-1)

#define FORALL_AST(first, elem, ...) \
    for (ast_list_t* list = (first); list; list = list->next) { \
        ast_t* elem = list->ast; \
        __VA_ARGS__ \
    }

enum ast_tag_e {
    AST_ID,
    AST_LIT,
    AST_MOD,
    AST_STRUCT,
    AST_DEF,
    AST_VAR,
    AST_VAL,
    AST_ANNOT,
    AST_PRIM,
    AST_BLOCK,
    AST_TUPLE,
    AST_ARRAY,
    AST_FIELD,
    AST_BINOP,
    AST_UNOP,
    AST_LAMBDA,
    AST_CALL,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_MATCH,
    AST_CASE,
    AST_CONT,
    AST_PROGRAM,
    AST_ERR
};

enum lit_tag_e {
    LIT_INT  = TOK_INT,
    LIT_FLT  = TOK_FLT,
    LIT_STR  = TOK_STR,
    LIT_CHR  = TOK_CHR,
    LIT_BOOL = TOK_BLT
};

enum unop_tag_e {
    UNOP_NOT,
    UNOP_NEG,
    UNOP_PLUS,
    UNOP_PRE_INC,
    UNOP_PRE_DEC,
    UNOP_POST_INC,
    UNOP_POST_DEC
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
    BINOP_ASSIGN,
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
    BINOP_CMPEQ,
    BINOP_CMPNE,
    BINOP_CMPGT,
    BINOP_CMPLT,
    BINOP_CMPGE,
    BINOP_CMPLE
};

enum cont_tag_e {
    CONT_BREAK,
    CONT_CONTINUE,
    CONT_RETURN
};

enum prim_tag_e {
#define PRIM(name, str) PRIM_##name = TYPE_##name,
    PRIM_LIST(, PRIM)
#undef PRIM
};

struct ast_s {
    uint32_t tag;
    union {
        struct {
            const char*  str;
            const ast_t* to;
        } id;
        struct {
            uint32_t    tag;
            const char* str;
            lit_t       value;
        } lit;
        struct {
            ast_t*      id;
            ast_list_t* decls;
        } mod;
        struct {
            bool        byref;
            ast_t*      id;
            ast_t*      members;
        } struct_;
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
            ast_t*      ast;
            ast_t*      type;
        } annot;
        struct {
            uint32_t    tag;
        } prim;
        struct {
            ast_list_t* stmts;
        } block;
        struct {
            ast_list_t* args;
        } tuple;
        struct {
            bool        regular;
            ast_list_t* elems;
        } array;
        struct {
            ast_t*      arg;
            ast_t*      id;
        } field;
        struct {
            uint32_t    tag;
            ast_t*      left;
            ast_t*      right;
        } binop;
        struct {
            uint32_t    tag;
            ast_t*      arg;
        } unop;
        struct {
            ast_t*      param;
            ast_t*      body;
        } lambda;
        struct {
            ast_t*      callee;
            ast_t*      arg;
        } call;
        struct {
            ast_t*      cond;
            ast_t*      if_true;
            ast_t*      if_false;
        } if_;
        struct {
            ast_t*      cond;
            ast_t*      body;
        } while_;
        struct {
            ast_t*      vars;
            ast_t*      expr;
            ast_t*      body;
        } for_;
        struct {
            ast_t*      arg;
            ast_list_t* cases;
        } match;
        struct {
            ast_t*      ptrn;
            ast_t*      value;
        } case_;
        struct {
            uint32_t    tag;
            ast_t*      parent;
        } cont;
        struct {
            ast_list_t* mods;
        } program;
    } data;
    const type_t* type;
    loc_t loc;
};

struct ast_list_s {
    ast_t*      ast;
    ast_list_t* next;
};

bool ast_is_ptrn(const ast_t*);
bool ast_is_refutable(const ast_t*);

size_t ast_list_length(const ast_list_t*);

const char* prim2str(uint32_t);

int binop_precedence(uint32_t);
uint32_t binop_tag_from_token(uint32_t);
const char* binop_symbol(uint32_t);

bool unop_is_prefix(uint32_t);
const char* unop_symbol(uint32_t);

void ast_dump(const ast_t*);

#endif // AST_H
