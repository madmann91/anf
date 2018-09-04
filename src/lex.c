#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "lex.h"

#define ERR_BUF_SIZE 256

static inline loc_t make_loc(lexer_t* lexer, size_t brow, size_t bcol) {
    return (loc_t) {
        .brow = brow,
        .bcol = bcol,
        .erow = lexer->row,
        .ecol = lexer->col
    };
}

static inline void eat(lexer_t* lexer) {
    assert(lexer->size > 0);
    if (*lexer->str == '\n') {
        lexer->row++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }
    lexer->str++;
    lexer->size--;
}

static inline bool accept(lexer_t* lexer, char c) {
    assert(lexer->size > 0);
    if (*lexer->str == c) {
        eat(lexer);
        return true;
    }
    return false;
}

static inline void eat_spaces(lexer_t* lexer) {
    while (lexer->size > 0 && *lexer->str != '\n' && isspace(*lexer->str)) eat(lexer);
}

static void eat_comments(lexer_t* lexer) {
    while (true) {
        while (lexer->size > 0 && *lexer->str != '*') eat(lexer);
        if (lexer->size == 0) {
            loc_t loc = make_loc(lexer, lexer->row, lexer->col);
            lex_error(lexer, &loc, "non-terminated multiline comment");
            return;
        }
        eat(lexer);
        if (accept(lexer, '/')) break;
    }
}

static tok_t parse_num(lexer_t* lexer, size_t brow, size_t bcol) {
    const char* beg = lexer->str;
    
    int base = 10;
    if (accept(lexer, '0')) {
        if (accept(lexer, 'b'))      base = 2;
        else if (accept(lexer, 'x')) base = 16;
        else if (accept(lexer, 'o')) base = 8;
    }

    if (base ==  2) { while (*lexer->str == '0' || *lexer->str == '1')  eat(lexer); }
    if (base ==  8) { while (isdigit(*lexer->str) && *lexer->str < '8') eat(lexer); }
    if (base == 10) { while (isdigit(*lexer->str))  eat(lexer); }
    if (base == 16) { while (isxdigit(*lexer->str)) eat(lexer); }

    // Parse fractional part and exponent
    bool exp = false, fract = false;
    if (base == 10) {
        if (accept(lexer, '.')) {
            fract = true;
            while (isdigit(*lexer->str)) eat(lexer);
        }

        if (accept(lexer, 'e')) {
            exp = true;
            if (!accept(lexer, '+')) accept(lexer, '-');
            while (isdigit(*lexer->str)) eat(lexer);
        }
    }

    // Add a zero at the end of the string to terminate it
    lexer->tmp = *lexer->str;
    *lexer->str = 0;

    if (exp || fract) {
        return (tok_t) {
            .tag = TOK_FLT,
            .lit = { .fval = strtod(beg, NULL) },
            .str = beg,
            .loc = make_loc(lexer, brow, bcol)
        };
    }
    return (tok_t) {
        .tag = TOK_INT,
        .lit = { .ival = strtoull(beg, NULL, base) },
        .str = beg,
        .loc = make_loc(lexer, brow, bcol)
    };
}

static tok_t parse_str_or_chr(lexer_t* lexer, bool str, int brow, int bcol) {
    const char* beg = lexer->str;
    if (!str) eat(lexer);
    else while (lexer->size > 0 && *lexer->str != '\"') eat(lexer);
    char* end = lexer->str;
    if (!accept(lexer, str ? '\"' : '\'')) {
        loc_t loc = make_loc(lexer, lexer->row, lexer->col);
        lex_error(lexer, &loc, str ? "unterminated string literal" : "unterminated character literal");
    }
    *end = 0;
    return (tok_t) { .tag = str ? TOK_STR : TOK_CHR, .str = beg, .loc = make_loc(lexer, brow, bcol) };
}

char* tok2str(uint32_t tag, char* buf) {
    switch (tag) {
#define TOK(name, str) case name: strcpy(buf, str); break;
    TOK_LIST(TOK)
#undef TOK
        default:
            assert(false);
            strcpy(buf, "");
            break;
    }
    return buf;
}

tok_t lex(lexer_t* lexer) {
    // Restore previously removed character (if any)
    if (lexer->tmp) *lexer->str = lexer->tmp, lexer->tmp = 0;

    while (true) {
        eat_spaces(lexer);

        size_t brow = lexer->row;
        size_t bcol = lexer->col;

        if (lexer->size == 0)
            return (tok_t) { .tag = TOK_EOF, .loc = make_loc(lexer, brow, bcol) };

        if (accept(lexer, '\n')) return (tok_t) { .tag = TOK_NL,       .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, '('))  return (tok_t) { .tag = TOK_LPAREN,   .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, ')'))  return (tok_t) { .tag = TOK_RPAREN,   .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, '{'))  return (tok_t) { .tag = TOK_LBRACE,   .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, '}'))  return (tok_t) { .tag = TOK_RBRACE,   .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, ','))  return (tok_t) { .tag = TOK_COMMA,    .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, ';'))  return (tok_t) { .tag = TOK_SEMI,     .loc = make_loc(lexer, brow, bcol) };

        if (accept(lexer, '\'')) return parse_str_or_chr(lexer, false, brow, bcol);
        if (accept(lexer, '\"')) return parse_str_or_chr(lexer, true,  brow, bcol);

        if (accept(lexer, '<')) {
            if (accept(lexer, '<')) {
                if (accept(lexer, '=')) return (tok_t) { .tag = TOK_LSHFTEQ, .loc = make_loc(lexer, brow, bcol) };
                return (tok_t) { .tag = TOK_LSHFT, .loc = make_loc(lexer, brow, bcol) };
            }
            if (accept(lexer, '-')) return (tok_t) { .tag = TOK_LARROW, .loc = make_loc(lexer, brow, bcol) };
            if (accept(lexer, '=')) return (tok_t) { .tag = TOK_CMPLE,  .loc = make_loc(lexer, brow, bcol) };
            return (tok_t) { .tag = TOK_LANGLE, .loc = make_loc(lexer, brow, bcol) };
        }

        if (accept(lexer, '>')) {
            if (accept(lexer, '>')) {
                if (accept(lexer, '=')) return (tok_t) { .tag = TOK_RSHFTEQ, .loc = make_loc(lexer, brow, bcol) };
                return (tok_t) { .tag = TOK_RSHFT, .loc = make_loc(lexer, brow, bcol) };
            }
            if (accept(lexer, '=')) return (tok_t) { .tag = TOK_CMPGE, .loc = make_loc(lexer, brow, bcol) };
            return (tok_t) { .tag = TOK_RANGLE, .loc = make_loc(lexer, brow, bcol) };
        }

        if (accept(lexer, ':')) {
            if (accept(lexer, ':')) return (tok_t) { .tag = TOK_DBLCOLON, .loc = make_loc(lexer, brow, bcol) };
            return (tok_t) { .tag = TOK_COLON, .loc = make_loc(lexer, brow, bcol) };
        }

#define LEX_BINOP(symbol, tag_, tag_eq, tag_dbl, dbl) \
    if (accept(lexer, symbol)) { \
        if (accept(lexer, '=')) return (tok_t) { .tag = tag_eq, .loc = make_loc(lexer, brow, bcol) }; \
        if (dbl && accept(lexer, symbol)) return (tok_t) { .tag = tag_dbl, .loc = make_loc(lexer, brow, bcol) }; \
        return (tok_t) { .tag = tag_, .loc = make_loc(lexer, brow, bcol) }; \
    }

        LEX_BINOP('+', TOK_ADD, TOK_ADDEQ, TOK_INC, true)
        LEX_BINOP('-', TOK_SUB, TOK_SUBEQ, TOK_DEC, true)
        LEX_BINOP('*', TOK_MUL, TOK_MULEQ, 0, false)
        LEX_BINOP('%', TOK_REM, TOK_REMEQ, 0, false)
        LEX_BINOP('&', TOK_AND, TOK_ANDEQ, TOK_DBLAND, true)
        LEX_BINOP('|', TOK_OR , TOK_OREQ , TOK_DBLOR,  true)
        LEX_BINOP('^', TOK_XOR, TOK_XOREQ, 0, false)
        LEX_BINOP('!', TOK_NOT, TOK_NOTEQ, 0, false)

#undef LEX_BINOP

        if (accept(lexer, '=')) {
            if (accept(lexer, '=')) return (tok_t) { .tag = TOK_CMPEQ,  .loc = make_loc(lexer, brow, bcol) };
            if (accept(lexer, '>')) return (tok_t) { .tag = TOK_RARROW, .loc = make_loc(lexer, brow, bcol) };
            return (tok_t) { .tag = TOK_EQ, .loc = make_loc(lexer, brow, bcol) };
        }

        if (accept(lexer, '/')) {
            if (accept(lexer, '*')) {
                eat_comments(lexer);
                continue;
            } else if (accept(lexer, '/')) {
                while (lexer->size > 0 && *lexer->str != '\n') eat(lexer);
                if (lexer->size > 0) eat(lexer);
                continue;
            }
            if (accept(lexer, '=')) return (tok_t) { .tag = TOK_DIVEQ, .loc = make_loc(lexer, brow, bcol) };
            return (tok_t) { .tag = TOK_DIV, .loc = make_loc(lexer, brow, bcol) };
        }

        if (isalpha(*lexer->str) || *lexer->str == '_') {
            const char* beg = lexer->str;
            while (isalnum(*lexer->str) || *lexer->str == '_') eat(lexer);
            char* end = lexer->str;
            lexer->tmp = *end;
            *end = 0;
            if (!strcmp(beg, "def"))      return (tok_t) { .tag = TOK_DEF,      .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "var"))      return (tok_t) { .tag = TOK_VAR,      .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "val"))      return (tok_t) { .tag = TOK_VAL,      .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "if"))       return (tok_t) { .tag = TOK_IF,       .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "else"))     return (tok_t) { .tag = TOK_ELSE,     .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "while"))    return (tok_t) { .tag = TOK_WHILE,    .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "for"))      return (tok_t) { .tag = TOK_FOR,      .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "match"))    return (tok_t) { .tag = TOK_MATCH,    .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "case"))     return (tok_t) { .tag = TOK_CASE,     .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "break"))    return (tok_t) { .tag = TOK_BREAK,    .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "continue")) return (tok_t) { .tag = TOK_CONTINUE, .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "return"))   return (tok_t) { .tag = TOK_RETURN,   .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "mod"))      return (tok_t) { .tag = TOK_MOD,      .loc = make_loc(lexer, brow, bcol) };
            if (!strcmp(beg, "true"))     return (tok_t) { .tag = TOK_BOOL,     .loc = make_loc(lexer, brow, bcol), .lit = { .bval = true  } };
            if (!strcmp(beg, "false"))    return (tok_t) { .tag = TOK_BOOL,     .loc = make_loc(lexer, brow, bcol), .lit = { .bval = false } };
            return (tok_t) { .tag = TOK_ID, .str = beg, .loc = make_loc(lexer, brow, bcol) };
        }

        if (isdigit(*lexer->str))
            return parse_num(lexer, brow, bcol);

        loc_t loc = make_loc(lexer, brow, bcol);
        lex_error(lexer, &loc, "unknown token '%c'", *lexer->str);
        eat(lexer);
        return (tok_t) { .tag = TOK_ERR, .loc = loc };
    }
}

void lex_error(lexer_t* lexer, const loc_t* loc, const char* fmt, ...) {
    char buf[ERR_BUF_SIZE];
    lexer->errs++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ERR_BUF_SIZE, fmt, args);
    lexer->error_fn(lexer, loc, buf);
    va_end(args);
}
