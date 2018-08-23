#include <stdarg.h>
#include <stdio.h>

#include "ast.h"

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

static inline bool expect(parser_t* parser, uint32_t tag) {
    assert(tag != TOK_ID);
    bool status = true;
    if (parser->ahead.tag != tag) {
        char buf1[TOK2STR_BUF_SIZE];
        char buf2[TOK2STR_BUF_SIZE];
        parse_error(parser, "expected '%s', got '%s'", tok2str(tag, buf1), parser->ahead.tag == TOK_ID ? parser->ahead.str : tok2str(parser->ahead.tag, buf2));
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

static inline ast_t* finalize_ast(parser_t* parser, ast_t* ast) {
    ast->loc.erow = parser->prev_loc.erow;
    ast->loc.ecol = parser->prev_loc.ecol;
    return ast;
}

static inline ast_list_t* ast_list_add(parser_t* parser, ast_list_t* list, ast_t* ast) {
    ast_list_t* prev = mpool_alloc(parser->pool, sizeof(ast_list_t));
    prev->next = list;
    prev->ast = ast;
    return prev;
}

static ast_t* parse_id(parser_t*);
static ast_t* parse_expr(parser_t*);
static ast_t* parse_stmt(parser_t*);
static ast_t* parse_tuple(parser_t*);
static ast_t* parse_block(parser_t*);
static ast_t* parse_def(parser_t*);
static ast_t* parse_mod(parser_t*);

static ast_t* parse_id(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_ID);
    char* str = "";
    if (parser->ahead.tag != TOK_ID) {
        char buf[TOK2STR_BUF_SIZE];
        parse_error(parser, "identifier expected, got '%s'", tok2str(parser->ahead.tag, buf));
    } else {
        str = mpool_alloc(parser->pool, strlen(parser->ahead.str) + 1);
        strcpy(str, parser->ahead.str);
    }
    next(parser);
    ast->data.str = str;
    return finalize_ast(parser, ast);
}

static ast_t* parse_expr(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_ID:     return parse_id(parser);
        case TOK_LPAREN: return parse_tuple(parser);
        case TOK_LBRACE: return parse_block(parser);
        default:
            break;
    }
    ast_t* ast = create_ast(parser, AST_ERR);
    char buf[TOK2STR_BUF_SIZE];
    parse_error(parser, "expected expression, got '%s'", tok2str(parser->ahead.tag, buf));
    next(parser);
    return finalize_ast(parser, ast);
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
    ast_t* ast = create_ast(parser, AST_ERR);
    char buf[TOK2STR_BUF_SIZE];
    parse_error(parser, "expected statement, got '%s'", tok2str(parser->ahead.tag, buf));
    next(parser);
    return finalize_ast(parser, ast);
}

static ast_t* parse_tuple(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_TUPLE);
    eat(parser, TOK_LPAREN);
    while (parser->ahead.tag != TOK_RPAREN) {
        ast_t* arg = parse_expr(parser);
        ast->data.tuple.args = ast_list_add(parser, ast->data.tuple.args, arg);

        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    expect(parser, TOK_RPAREN);
    return finalize_ast(parser, ast);
}

static ast_t* parse_block(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_BLOCK);
    eat(parser, TOK_LBRACE);
    eat_nl_or_semi(parser);
    while (parser->ahead.tag != TOK_RBRACE) {
        ast_t* stmt = parse_stmt(parser);
        ast->data.block.stmts = ast_list_add(parser, ast->data.block.stmts, stmt);

        if (!eat_nl_or_semi(parser))
            break;
    }
    expect(parser, TOK_RBRACE);
    return finalize_ast(parser, ast);
}

static ast_t* parse_def(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_DEF);
    eat(parser, TOK_DEF);
    ast->data.def.id = parse_id(parser);
    if (parser->ahead.tag == TOK_LPAREN)
        ast->data.def.param = parse_tuple(parser);
    if (parser->ahead.tag == TOK_LBRACE)
        ast->data.def.body = parse_block(parser);
    return finalize_ast(parser, ast);
}

static ast_t* parse_mod(parser_t* parser) {
    ast_t* ast = create_ast(parser, AST_MOD);
    expect(parser, TOK_MOD);
    ast->data.mod.id = parse_id(parser);
    eat_nl(parser);
    expect(parser, TOK_LBRACE);
    eat_nl_or_semi(parser);
    while (parser->ahead.tag != TOK_RBRACE) {
        ast_t* def = parse_def(parser);
        ast->data.mod.asts = ast_list_add(parser, ast->data.mod.asts, def);

        if (!eat_nl_or_semi(parser))
            break;
    }
    expect(parser, TOK_RBRACE);
    return finalize_ast(parser, ast);
}

ast_t* parse(parser_t* parser) {
    next(parser);
    return parse_mod(parser);
}

void parse_error(parser_t* parser, const char* fmt, ...) {
    char buf[ERR_BUF_SIZE];
    parser->errs++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ERR_BUF_SIZE, fmt, args);
    parser->error_fn(parser, buf);
    va_end(args);
}
