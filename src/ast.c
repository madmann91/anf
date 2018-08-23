#include <stdarg.h>
#include <stdio.h>

#include "ast.h"

#define ERR_BUF_SIZE 256

static inline void next(parser_t* parser) {
    parser->prev_loc = parser->ahead.loc;
    parser->ahead    = lex(parser->lex);
}

static inline bool accept(parser_t* parser, uint32_t tag) {
    if (parser->ahead.tag == tag) {
        next(parser);
        return true;
    }
    return false;
}

static inline void expect(parser_t* parser, uint32_t tag) {
    assert(tag != TOK_ID);
    if (parser->ahead.tag != tag) {
        char buf1[TOK2STR_BUF_SIZE];
        char buf2[TOK2STR_BUF_SIZE];
        parse_error(parser, "expected '%s', got '%s'", tok2str(tag, buf1), parser->ahead.tag == TOK_ID ? parser->ahead.str : tok2str(parser->ahead.tag, buf2));
    }
    next(parser);
}

static inline ast_t* create_ast_with_loc(parser_t* parser, loc_t loc) {
    ast_t* ast = mpool_alloc(parser->pool, sizeof(ast_t));
    memset(ast, 0, sizeof(ast_t));
    ast->loc = loc;
    return ast;
}

static inline ast_t* create_ast(parser_t* parser) {
    return create_ast_with_loc(parser, parser->ahead.loc);
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

static ast_t* parse_def(parser_t* parser) {
    next(parser);
    return NULL;
}

static ast_t* parse_mod(parser_t* parser) {
    ast_t* ast = create_ast(parser);
    expect(parser, TOK_MOD);
    while (parser->ahead.tag == TOK_DEF) {
        ast_t* def = parse_def(parser);
        ast->data.mod.asts = ast_list_add(parser, ast->data.mod.asts, def);
    }
    return finalize_ast(parser, ast);
}

ast_t* parse(parser_t* parser) {
    next(parser);
    return parse_mod(parser);
}

void parse_error(parser_t* parser, const char* fmt, ...) {
    char buf[ERR_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ERR_BUF_SIZE, fmt, args);
    parser->error_fn(parser, buf);
    va_end(args);
}
