#ifndef LEX_H
#define LEX_H

#include <stdint.h>

#include "anf.h"

#define TOK2STR_BUF_SIZE 32

#define TOK_LIST(f) \
    f(TOK_LIT_I,    "integer literal") \
    f(TOK_LIT_F,    "floating point literal") \
    f(TOK_STR,      "string literal") \
    f(TOK_ID,       "identifier") \
    f(TOK_NL,       "new line") \
    f(TOK_DEF,      "def") \
    f(TOK_VAR,      "var") \
    f(TOK_VAL,      "val") \
    f(TOK_IF,       "if") \
    f(TOK_ELSE,     "else") \
    f(TOK_MOD,      "mod") \
    f(TOK_QUOTE,    "\'") \
    f(TOK_DBLQUOTE, "\"") \
    f(TOK_LPAREN,   "(") \
    f(TOK_RPAREN,   ")") \
    f(TOK_LBRACE,   "{") \
    f(TOK_RBRACE,   "}") \
    f(TOK_LANGLE,   "<") \
    f(TOK_RANGLE,   ">") \
    f(TOK_COMMA,    ",") \
    f(TOK_COLON,    ":") \
    f(TOK_DBLCOLON, "::") \
    f(TOK_SEMI,     ";") \
    f(TOK_ADD,      "+") \
    f(TOK_SUB,      "-") \
    f(TOK_MUL,      "*") \
    f(TOK_DIV,      "/") \
    f(TOK_REM,      "%") \
    f(TOK_AND,      "&") \
    f(TOK_OR,       "|") \
    f(TOK_XOR,      "^") \
    f(TOK_LSHFT,    "<<") \
    f(TOK_RSHFT,    ">>") \
    f(TOK_NOT,      "!") \
    f(TOK_EQ,       "=") \
    f(TOK_INC,      "++") \
    f(TOK_DEC,      "--") \
    f(TOK_NOTEQ,    "!=") \
    f(TOK_CMPEQ,    "==") \
    f(TOK_CMPGE,    ">=") \
    f(TOK_CMPLE,    "<=") \
    f(TOK_ADDEQ,    "+=") \
    f(TOK_SUBEQ,    "-=") \
    f(TOK_MULEQ,    "*=") \
    f(TOK_DIVEQ,    "/=") \
    f(TOK_REMEQ,    "%=") \
    f(TOK_ANDEQ,    "&=") \
    f(TOK_OREQ,     "|=") \
    f(TOK_XOREQ,    "^=") \
    f(TOK_LSHFTEQ,  "<<=") \
    f(TOK_RSHFTEQ,  ">>=") \
    f(TOK_DBLAND,   "&&") \
    f(TOK_DBLOR,    "||") \
    f(TOK_ERR,      "invalid token") \
    f(TOK_EOF,      "eof")

typedef union  lit_u lit_t;
typedef struct tok_s tok_t;
typedef struct lex_s lex_t;

enum tok_tag_e {
#define TOK(name, str) name,
    TOK_LIST(TOK)
#undef TOK
};

union lit_u {
    double   fval;
    uint64_t ival;
};

struct tok_s {
    uint32_t    tag;
    const char* str;
    lit_t       lit;
    loc_t       loc;
};

struct lex_s {
    char        tmp;
    const char* file;
    size_t      row;
    size_t      col;
    size_t      errs;
    char*       str;
    size_t      size;
    void        (*error_fn)(lex_t*, const loc_t*, const char*);
};

char* tok2str(uint32_t, char*);

tok_t lex(lex_t*);
void lex_error(lex_t*, const loc_t*, const char*, ...);

#endif // LEX_H