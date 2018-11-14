#include <ctype.h>
#include <stdio.h>

#include "lex.h"
#include "util.h"

// Generated file
#include "lex.inc"

#define MIN_UTF8_BYTES 2
#define MAX_UTF8_BYTES 4

static inline loc_t make_loc(lexer_t* lexer, size_t brow, size_t bcol) {
    return (loc_t) {
        .brow = brow,
        .bcol = bcol,
        .erow = lexer->row,
        .ecol = lexer->col
    };
}

static inline size_t check_utf8(lexer_t* lexer, size_t n) {
    loc_t loc = make_loc(lexer, lexer->row, lexer->col);
    if (lexer->size < n || n > MAX_UTF8_BYTES || n < MIN_UTF8_BYTES)
        goto error;
    for (size_t i = 1; i < n; ++i) {
        uint8_t c = ((uint8_t*)lexer->str)[i];
        if ((c & 0xC0) != 0x80)
            goto error;
    }
    return n;
error:
    log_error(lexer->log, &loc, "invalid UTF-8 character");
    return 1;
}

static inline void eat(lexer_t* lexer) {
    assert(lexer->size > 0);
    if (*lexer->str & 0x80) {
        // UTF-8 characters
        uint8_t c = *(uint8_t*)lexer->str;
        size_t n = 0;
        while (c & 0x80 && n <= MAX_UTF8_BYTES) c <<= 1, n++;
        n = check_utf8(lexer, n);
        lexer->str += n;
        lexer->size -=n;
        lexer->col++;
    } else {
        // Other characters
        if (*lexer->str == '\n') {
            lexer->row++;
            lexer->col = 1;
        } else {
            lexer->col++;
        }

        lexer->str++;
        lexer->size--;
    }
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
            log_error(lexer->log, &loc, "non-terminated multiline comment");
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
    if (lexer->size == 0 || !accept(lexer, str ? '\"' : '\'')) {
        loc_t loc = make_loc(lexer, lexer->row, lexer->col);
        log_error(lexer->log, &loc, str ? "unterminated string literal" : "unterminated character literal");
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
        if (accept(lexer, '['))  return (tok_t) { .tag = TOK_LBRACKET, .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, ']'))  return (tok_t) { .tag = TOK_RBRACKET, .loc = make_loc(lexer, brow, bcol) };
        if (accept(lexer, '.'))  return (tok_t) { .tag = TOK_DOT,      .loc = make_loc(lexer, brow, bcol) };
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

            loc_t loc = make_loc(lexer, brow, bcol);
            return tok_from_str(beg, loc); // Generated implementation in lex.inc
        }

        if (isdigit(*lexer->str))
            return parse_num(lexer, brow, bcol);

        loc_t loc = make_loc(lexer, brow, bcol);
        log_error(lexer->log, &loc, "unknown token '%c'", *lexer->str);
        eat(lexer);
        return (tok_t) { .tag = TOK_ERR, .loc = loc };
    }
}
