#ifndef LEX_H
#define LEX_H

#include <stdint.h>

#include "type.h"
#include "node.h"
#include "log.h"

#define TOK_LIST(f) \
    f(TOK_INT,      "integer literal") \
    f(TOK_FLT,      "floating point literal") \
    f(TOK_STR,      "string literal") \
    f(TOK_CHR,      "character literal") \
    f(TOK_BLT,      "boolean literal") \
    f(TOK_ID,       "identifier") \
    f(TOK_NL,       "new line") \
    PRIM_LIST(TOK_, f) \
    f(TOK_DEF,      "def") \
    f(TOK_VAR,      "var") \
    f(TOK_VAL,      "val") \
    f(TOK_IF,       "if") \
    f(TOK_ELSE,     "else") \
    f(TOK_WHILE,    "while") \
    f(TOK_FOR,      "for") \
    f(TOK_MATCH,    "match") \
    f(TOK_CASE,     "case") \
    f(TOK_BREAK,    "break") \
    f(TOK_CONTINUE, "continue") \
    f(TOK_RETURN,   "return") \
    f(TOK_MOD,      "mod") \
    f(TOK_STRUCT,   "struct") \
    f(TOK_BYREF,    "byref") \
    f(TOK_LPAREN,   "(") \
    f(TOK_RPAREN,   ")") \
    f(TOK_LBRACE,   "{") \
    f(TOK_RBRACE,   "}") \
    f(TOK_LBRACKET, "[") \
    f(TOK_RBRACKET, "]") \
    f(TOK_LANGLE,   "<") \
    f(TOK_RANGLE,   ">") \
    f(TOK_DOT,      ".") \
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
    f(TOK_LARROW,   "<-") \
    f(TOK_RARROW,   "=>") \
    f(TOK_ERR,      "invalid token") \
    f(TOK_EOF,      "end of file")

typedef union  lit_u lit_t;
typedef struct tok_s tok_t;
typedef struct lexer_s lexer_t;

enum tok_tag_e {
#define TOK(name, str) name,
    TOK_LIST(TOK)
#undef TOK
};

union lit_u {
    bool        bval;
    double      fval;
    uint64_t    ival;
};

struct tok_s {
    uint32_t    tag;
    const char* str;
    lit_t       lit;
    loc_t       loc;
};

struct lexer_s {
    char   tmp;
    size_t row;
    size_t col;
    char*  str;
    size_t size;
    log_t* log;
};

char* tok2str(uint32_t, char*);

tok_t lex(lexer_t*);

#endif // LEX_H
