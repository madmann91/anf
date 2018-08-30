#include <stdarg.h>
#include <stdio.h>

#include "parse.h"

#define ERR_BUF_SIZE 256

static inline void next(parser_t* parser) {
    parser->prev_loc = parser->ahead.loc;
    parser->ahead    = lex(parser->lex);
}

static inline void eat(parser_t* parser, uint32_t tag) {
    assert(parser->ahead.tag == tag);
    next(parser);
}

static inline bool accept(parser_t* parser, uint32_t tag) {
    if (parser->ahead.tag == tag) {
        next(parser);
        return true;
    }
    return false;
}

static inline bool eat_nl(parser_t* parser) {
    bool new_line = false;
    while (accept(parser, TOK_NL)) new_line = true;
    return new_line;
}

static inline bool eat_nl_or_semi(parser_t* parser) {
    bool new_line = false;
    while (accept(parser, TOK_NL) || accept(parser, TOK_SEMI)) new_line = true;
    return new_line;
}

static char* tok2str_with_quotes(uint32_t tag, char* buf) {
    if (tag == TOK_LIT_I ||
        tag == TOK_LIT_F ||
        tag == TOK_STR ||
        tag == TOK_ID ||
        tag == TOK_NL ||
        tag == TOK_ERR ||
        tag == TOK_EOF)
        return tok2str(tag, buf);
    buf[0] = '\'';
    tok2str(tag, buf + 1);
    size_t len = strlen(buf);
    buf[len] = '\'';
    buf[len + 1] = 0;
    return buf;
}

static inline bool expect(parser_t* parser, const char* msg, uint32_t tag) {
    assert(tag != TOK_ID);
    bool status = true;
    if (parser->ahead.tag != tag) {
        char buf1[TOK2STR_BUF_SIZE + 2];
        char buf2[TOK2STR_BUF_SIZE + 2];
        const char* str1 = tok2str_with_quotes(tag, buf1);
        const char* str2 = parser->ahead.tag == TOK_ID ? parser->ahead.str : tok2str_with_quotes(parser->ahead.tag, buf2);
        parse_error(parser, &parser->ahead.loc, "expected %s in %s, but got %s", str1, msg, str2);
        status = false;
    }
    next(parser);
    return status;
}

static inline ast_t* create_ast_with_loc(parser_t* parser, uint32_t tag, loc_t loc) {
    ast_t* ast = mpool_alloc(parser->pool, sizeof(ast_t));
    memset(ast, 0, sizeof(ast_t));
    ast->tag = tag;
    ast->loc = loc;
    return ast;
}

static inline ast_t* create_ast(parser_t* parser, uint32_t tag) {
    return create_ast_with_loc(parser, tag, parser->ahead.loc);
}

static inline ast_t* ast_finalize(parser_t* parser, ast_t* ast) {
    ast->loc.erow = parser->prev_loc.erow;
    ast->loc.ecol = parser->prev_loc.ecol;
    return ast;
}

static inline ast_list_t** ast_list_add(parser_t* parser, ast_list_t** list, ast_t* ast) {
    *list = mpool_alloc(parser->pool, sizeof(ast_list_t));
    (*list)->next = NULL;
    (*list)->ast = ast;
    return &(*list)->next;
}

// Basic syntax categories
static ast_t* parse_expr(parser_t*);
static ast_t* parse_ptrn(parser_t*);
static ast_t* parse_stmt(parser_t*);
static ast_t* parse_decl(parser_t*);

static ast_t* parse_err(parser_t*, const char*);

// Literals & identifiers
static ast_t* parse_id(parser_t*);
static ast_t* parse_lit(parser_t*);

// Expressions/Patterns
static ast_t* parse_primary_expr(parser_t*);
static ast_t* parse_binary_expr(parser_t*, ast_t*, int);
static ast_t* parse_tuple(parser_t*);
static ast_t* parse_block(parser_t*);

// Declarations
static ast_t* parse_def(parser_t*);
static ast_t* parse_varl(parser_t*, bool);
static ast_t* parse_mod(parser_t*);

static ast_t* parse_expr(parser_t* parser) {
    ast_t* ast = parse_primary_expr(parser);
    return parse_binary_expr(parser, ast, MAX_BINOP_PRECEDENCE);
}

static ast_t* parse_ptrn(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_LIT_I:
        case TOK_LIT_F:
            return parse_lit(parser);
        case TOK_ID:     return parse_id(parser);
        case TOK_LPAREN: return parse_tuple(parser);
        default:
            break;
    }
    return parse_err(parser, "pattern");
}

static ast_t* parse_stmt(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_ID:
        case TOK_LPAREN:
        case TOK_LBRACE:
            return parse_expr(parser);
        default:
            break;
    }
    return parse_err(parser, "statement");
}

static ast_t* parse_decl(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_DEF: return parse_def(parser);
        case TOK_VAR: return parse_varl(parser, true);
        case TOK_VAL: return parse_varl(parser, false);
        default:
            break;
    }
    return parse_err(parser, "declaration");
}

static ast_t* parse_err(parser_t* parser, const char* msg) {
    ast_t* ast = create_ast(parser, AST_ERR);
    char buf[TOK2STR_BUF_SIZE + 2];
    parse_error(parser, &parser->ahead.loc, "expected %s, got %s", msg, tok2str_with_quotes(parser->ahead.tag, buf));
    next(parser);
    return ast_finalize(parser, ast);
}

static ast_t* parse_id(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_ID);
    char* str = "";
    if (parser->ahead.tag != TOK_ID) {
        char buf[TOK2STR_BUF_SIZE + 2];
        parse_error(parser, &parser->ahead.loc, "identifier expected, got %s", tok2str_with_quotes(parser->ahead.tag, buf));
    } else {
        str = mpool_alloc(parser->pool, strlen(parser->ahead.str) + 1);
        strcpy(str, parser->ahead.str);
    }
    next(parser);
    ast->data.id.str = str;
    return ast_finalize(parser, ast);
}

static ast_t* parse_lit(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_LIT);
    if (parser->ahead.tag != TOK_LIT_I && parser->ahead.tag != TOK_LIT_F) {
        char buf[TOK2STR_BUF_SIZE + 2];
        parse_error(parser, &parser->ahead.loc, "literal expected, got %s", tok2str_with_quotes(parser->ahead.tag, buf));
        ast->data.lit.value   = (lit_t) { .ival = 0 };
        ast->data.lit.integer = true;
        ast->data.lit.str     = "";
    } else {
        char* str = mpool_alloc(parser->pool, strlen(parser->ahead.str) + 1);
        strcpy(str, parser->ahead.str);
        ast->data.lit.value   = parser->ahead.lit;
        ast->data.lit.integer = parser->ahead.tag == TOK_LIT_I;
        ast->data.lit.str     = str;
    }
    next(parser);
    return ast_finalize(parser, ast);
}

static ast_t* parse_primary_expr(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_DEC:
        case TOK_INC:
            {
                ast_t* ast = create_ast(parser, AST_UNOP);
                ast->data.unop.tag = parser->ahead.tag == TOK_DEC ? UNOP_PRE_DEC : UNOP_PRE_INC;
                ast->data.unop.op  = parse_primary_expr(parser);
                return ast_finalize(parser, ast);
            }
        case TOK_LIT_I:
        case TOK_LIT_F:
            return parse_lit(parser);
        case TOK_ID:     return parse_id(parser);
        case TOK_LPAREN: return parse_tuple(parser);
        case TOK_LBRACE: return parse_block(parser);
        default:
            break;
    }
    return parse_err(parser, "primary expression");
}

static ast_t* parse_binary_expr(parser_t* parser, ast_t* left, int max_prec) {
    while (true) {
        uint32_t tag = binop_tag_from_token(parser->ahead.tag);
        if (tag == INVALID_TAG) break;
        int prec = binop_precedence(tag);
        if (prec > max_prec) break;
        next(parser);

        ast_t* right = parse_primary_expr(parser);

        uint32_t next_tag = binop_tag_from_token(parser->ahead.tag);
        if (next_tag != INVALID_TAG) {
            int next_prec = binop_precedence(next_tag);
            if (next_prec < prec)
                right = parse_binary_expr(parser, right, next_prec);
        }

        ast_t* binop = create_ast_with_loc(parser, AST_BINOP, left->loc);
        binop->data.binop.tag   = tag;
        binop->data.binop.left  = left;
        binop->data.binop.right = right;
        left = ast_finalize(parser, binop);
    }
    return left;
}

static ast_t* parse_tuple(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_TUPLE);
    eat(parser, TOK_LPAREN);
    ast_list_t** cur = &ast->data.tuple.args;
    while (parser->ahead.tag != TOK_RPAREN) {
        ast_t* arg = parse_expr(parser);
        cur = ast_list_add(parser, cur, arg);

        eat_nl(parser);
        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    expect(parser, "tuple", TOK_RPAREN);
    return ast_finalize(parser, ast);
}

static ast_t* parse_block(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_BLOCK);
    eat(parser, TOK_LBRACE);
    eat_nl_or_semi(parser);
    ast_list_t** cur = &ast->data.tuple.args;
    while (parser->ahead.tag != TOK_RBRACE) {
        ast_t* stmt = parse_stmt(parser);
        cur = ast_list_add(parser, cur, stmt);

        if (!eat_nl_or_semi(parser))
            break;
    }
    expect(parser, "statement block", TOK_RBRACE);
    return ast_finalize(parser, ast);
}

static ast_t* parse_def(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_DEF);
    eat(parser, TOK_DEF);
    eat_nl(parser);
    ast->data.def.id = parse_id(parser);
    eat_nl(parser);
    if (parser->ahead.tag == TOK_LPAREN) {
        ast->data.def.param = parse_tuple(parser);
        if (ast_is_refutable(ast->data.def.param))
            parse_error(parser, &ast->data.def.param->loc, "invalid function parameter");
        if (parser->ahead.tag == TOK_LBRACE)
            ast->data.def.value = parse_block(parser);
    } else {
        expect(parser, "definition", TOK_EQ);
        eat_nl(parser);
        ast->data.def.value = parse_expr(parser);
    }
    return ast_finalize(parser, ast);
}

static ast_t* parse_varl(parser_t* parser, bool var) {
    ast_t* ast = create_ast(parser, var ? AST_VAR : AST_VAL);
    eat(parser, var ? TOK_VAR : TOK_VAL);
    eat_nl(parser);
    ast->data.varl.ptrn = parse_ptrn(parser);
    eat_nl(parser);
    expect(parser, var ? "variable" : "value", TOK_EQ);
    eat_nl(parser);
    ast->data.varl.value = parse_expr(parser);
    return ast_finalize(parser, ast);
}

static ast_t* parse_mod(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_MOD);
    expect(parser, "module", TOK_MOD);
    eat_nl(parser);
    ast->data.mod.id = parse_id(parser);
    eat_nl(parser);
    expect(parser, "module contents", TOK_LBRACE);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.mod.decls;
    while (parser->ahead.tag != TOK_RBRACE) {
        ast_t* decl = parse_decl(parser);
        cur = ast_list_add(parser, cur, decl);

        if (!eat_nl_or_semi(parser))
            break;
    }
    expect(parser, "module contents", TOK_RBRACE);
    return ast_finalize(parser, ast);
}

ast_t* parse(parser_t* parser) {
    next(parser);
    return parse_mod(parser);
}

void parse_error(parser_t* parser, const loc_t* loc, const char* fmt, ...) {
    char buf[ERR_BUF_SIZE];
    parser->errs++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ERR_BUF_SIZE, fmt, args);
    parser->error_fn(parser, loc, buf);
    va_end(args);
}
