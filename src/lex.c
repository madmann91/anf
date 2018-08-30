#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "lex.h"

#define ERR_BUF_SIZE 256

static inline loc_t make_loc(lex_t* lex, size_t brow, size_t bcol) {
    return (loc_t) {
        .brow = brow,
        .bcol = bcol,
        .erow = lex->row,
        .ecol = lex->col
    };
}

static inline void eat(lex_t* lex) {
    assert(lex->size > 0);
    if (*lex->str == '\n') {
        lex->row++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    lex->str++;
    lex->size--;
}

static inline bool accept(lex_t* lex, char c) {
    assert(lex->size > 0);
    if (*lex->str == c) {
        eat(lex);
        return true;
    }
    return false;
}

static inline void eat_spaces(lex_t* lex) {
    while (lex->size > 0 && *lex->str != '\n' && isspace(*lex->str)) eat(lex);
}

static void eat_comments(lex_t* lex) {
    while (true) {
        while (lex->size > 0 && *lex->str != '*') eat(lex);
        if (lex->size == 0) {
            loc_t loc = make_loc(lex, lex->row, lex->col);
            lex_error(lex, &loc, "non-terminated multiline comment");
            return;
        }
        eat(lex);
        if (accept(lex, '/')) break;
    }
}

static tok_t parse_lit(lex_t* lex, size_t brow, size_t bcol) {
    int base = 10;
    if (accept(lex, '0')) {
        if (accept(lex, 'b'))      base = 2;
        else if (accept(lex, 'x')) base = 16;
        else if (accept(lex, 'o')) base = 8;
    }

    const char* beg = lex->str;
    
    if (base ==  2) { while (*lex->str == '0' || *lex->str == '1')  eat(lex); }
    if (base ==  8) { while (isdigit(*lex->str) && *lex->str < '8') eat(lex); }
    if (base == 10) { while (isdigit(*lex->str))  eat(lex); }
    if (base == 16) { while (isxdigit(*lex->str)) eat(lex); }

    // Parse fractional part and exponent
    bool exp = false, fract = false;
    if (base == 10) {
        if (accept(lex, '.')) {
            fract = true;
            while (isdigit(*lex->str)) eat(lex);
        }

        if (accept(lex, 'e')) {
            exp = true;
            if (!accept(lex, '+')) accept(lex, '-');
            while (isdigit(*lex->str)) eat(lex);
        }
    }

    // Add a zero at the end of the string to terminate it
    lex->tmp = *lex->str;
    *lex->str = 0;

    if (exp || fract) {
        return (tok_t) {
            .tag = TOK_LIT_F,
            .lit = { .fval = strtod(beg, NULL) },
            .str = beg,
            .loc = make_loc(lex, brow, bcol)
        };
    }
    return (tok_t) {
        .tag = TOK_LIT_I,
        .lit = { .ival = strtoull(beg, NULL, base) },
        .str = beg,
        .loc = make_loc(lex, brow, bcol)
    };
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

tok_t lex(lex_t* lex) {
    // Restore previously removed character (if any)
    if (lex->tmp) *lex->str = lex->tmp, lex->tmp = 0;

    while (true) {
        eat_spaces(lex);

        size_t brow = lex->row;
        size_t bcol = lex->col;

        if (lex->size == 0)
            return (tok_t) { .tag = TOK_EOF, .loc = make_loc(lex, brow, bcol) };

        if (accept(lex, '\n')) return (tok_t) { .tag = TOK_NL,       .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, '\'')) return (tok_t) { .tag = TOK_QUOTE,    .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, '\"')) return (tok_t) { .tag = TOK_DBLQUOTE, .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, '('))  return (tok_t) { .tag = TOK_LPAREN,   .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, ')'))  return (tok_t) { .tag = TOK_RPAREN,   .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, '{'))  return (tok_t) { .tag = TOK_LBRACE,   .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, '}'))  return (tok_t) { .tag = TOK_RBRACE,   .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, ','))  return (tok_t) { .tag = TOK_COMMA,    .loc = make_loc(lex, brow, bcol) };
        if (accept(lex, ';'))  return (tok_t) { .tag = TOK_SEMI,     .loc = make_loc(lex, brow, bcol) };

        if (accept(lex, '<')) {
            if (accept(lex, '<')) {
                if (accept(lex, '=')) return (tok_t) { .tag = TOK_LSHFTEQ, .loc = make_loc(lex, brow, bcol) };
                return (tok_t) { .tag = TOK_LSHFT, .loc = make_loc(lex, brow, bcol) };
            }
            if (accept(lex, '=')) return (tok_t) { .tag = TOK_CMPLE, .loc = make_loc(lex, brow, bcol) };
            return (tok_t) { .tag = TOK_LANGLE, .loc = make_loc(lex, brow, bcol) };
        }

        if (accept(lex, '>')) {
            if (accept(lex, '>')) {
                if (accept(lex, '=')) return (tok_t) { .tag = TOK_RSHFTEQ, .loc = make_loc(lex, brow, bcol) };
                return (tok_t) { .tag = TOK_RSHFT, .loc = make_loc(lex, brow, bcol) };
            }
            if (accept(lex, '=')) return (tok_t) { .tag = TOK_CMPGE, .loc = make_loc(lex, brow, bcol) };
            return (tok_t) { .tag = TOK_RANGLE, .loc = make_loc(lex, brow, bcol) };
        }

        if (accept(lex, ':')) {
            if (accept(lex, ':')) return (tok_t) { .tag = TOK_DBLCOLON, .loc = make_loc(lex, brow, bcol) }; 
            return (tok_t) { .tag = TOK_COLON, .loc = make_loc(lex, brow, bcol) };
        }

#define LEX_BINOP(symbol, tag_, tag_eq, tag_dbl, dbl) \
    if (accept(lex, symbol)) { \
        if (accept(lex, '=')) return (tok_t) { .tag = tag_eq, .loc = make_loc(lex, brow, bcol) }; \
        if (dbl && accept(lex, symbol)) return (tok_t) { .tag = tag_dbl, .loc = make_loc(lex, brow, bcol) }; \
        return (tok_t) { .tag = tag_, .loc = make_loc(lex, brow, bcol) }; \
    }

        LEX_BINOP('+', TOK_ADD, TOK_ADDEQ, TOK_INC, true)
        LEX_BINOP('-', TOK_SUB, TOK_SUBEQ, TOK_DEC, true)
        LEX_BINOP('*', TOK_MUL, TOK_MULEQ, 0, false)
        LEX_BINOP('%', TOK_REM, TOK_REMEQ, 0, false)
        LEX_BINOP('&', TOK_AND, TOK_ANDEQ, TOK_DBLAND, true)
        LEX_BINOP('|', TOK_OR , TOK_OREQ , TOK_DBLOR,  true)
        LEX_BINOP('^', TOK_XOR, TOK_XOREQ, 0, false)
        LEX_BINOP('!', TOK_NOT, TOK_NOTEQ, 0, false)
        LEX_BINOP('=', TOK_EQ,  TOK_CMPEQ, 0, false)

#undef LEX_BINOP

        if (accept(lex, '/')) {
            if (accept(lex, '*')) {
                eat_comments(lex);
                continue;
            } else if (accept(lex, '/')) {
                while (lex->size > 0 && *lex->str != '\n') eat(lex);
                if (lex->size > 0) eat(lex);
                continue;
            }
            if (accept(lex, '=')) return (tok_t) { .tag = TOK_DIVEQ, .loc = make_loc(lex, brow, bcol) };
            return (tok_t) { .tag = TOK_DIV, .loc = make_loc(lex, brow, bcol) };
        }

        if (isalpha(*lex->str)) {
            const char* beg = lex->str;
            while (isalnum(*lex->str) || *lex->str == '_') eat(lex);
            char* end = lex->str;
            lex->tmp = *end;
            *end = 0;
            if (!strcmp(beg, "def"))  return (tok_t) { .tag = TOK_DEF,  .loc = make_loc(lex, brow, bcol) };
            if (!strcmp(beg, "var"))  return (tok_t) { .tag = TOK_VAR,  .loc = make_loc(lex, brow, bcol) };
            if (!strcmp(beg, "val"))  return (tok_t) { .tag = TOK_VAL,  .loc = make_loc(lex, brow, bcol) };
            if (!strcmp(beg, "if"))   return (tok_t) { .tag = TOK_IF,   .loc = make_loc(lex, brow, bcol) };
            if (!strcmp(beg, "else")) return (tok_t) { .tag = TOK_ELSE, .loc = make_loc(lex, brow, bcol) };
            if (!strcmp(beg, "mod"))  return (tok_t) { .tag = TOK_MOD,  .loc = make_loc(lex, brow, bcol) };
            return (tok_t) { .tag = TOK_ID, .str = beg, .loc = make_loc(lex, brow, bcol) };
        }

        if (isdigit(*lex->str))
            return parse_lit(lex, brow, bcol);

        eat(lex);
        loc_t loc = make_loc(lex, brow, bcol);
        lex_error(lex, &loc, "unknown token '%c'", *lex->str);
        return (tok_t) { .tag = TOK_ERR, .loc = loc };
    }
}

void lex_error(lex_t* lex, const loc_t* loc, const char* fmt, ...) {
    char buf[ERR_BUF_SIZE];
    lex->errs++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ERR_BUF_SIZE, fmt, args);
    lex->error_fn(lex, loc, buf);
    va_end(args);
}
