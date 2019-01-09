#include <stdio.h>

#include "parse.h"

#define TOK2STR_BUF_SIZE 32

static inline void next(parser_t* parser) {
    parser->prev_loc = parser->ahead.loc;
    parser->ahead    = lex(parser->lexer);
}

static inline void eat(parser_t* parser, uint32_t tag) {
    assert(parser->ahead.tag == tag); (void)tag;
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
    switch (tag) {
        case TOK_INT:
        case TOK_FLT:
        case TOK_CHR:
        case TOK_STR:
        case TOK_BLT:
        case TOK_ID:
        case TOK_NL:
        case TOK_ERR:
        case TOK_EOF:
            return tok2str(tag, buf);
        default: break;
    }

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
        const char* str2 = tok2str_with_quotes(parser->ahead.tag, buf2);
        log_error(parser->log, &parser->ahead.loc, "expected {0:s} in {1:s}, but got {2:s}", { .s = str1 }, { .s = msg }, { .s = str2 });
        status = false;
    }
    next(parser);
    return status;
}

static inline ast_t* ast_create_with_loc(parser_t* parser, uint32_t tag, loc_t loc) {
    ast_t* ast = mpool_alloc(parser->pool, sizeof(ast_t));
    memset(ast, 0, sizeof(ast_t));
    ast->tag = tag;
    ast->loc = loc;
    return ast;
}

static inline ast_t* ast_create(parser_t* parser, uint32_t tag) {
    return ast_create_with_loc(parser, tag, parser->ahead.loc);
}

static inline ast_t* ast_finalize(ast_t* ast, parser_t* parser) {
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
static ast_t* parse_type(parser_t*);

static ast_t* parse_err(parser_t*, const char*);
static ast_t* parse_annot(parser_t*, ast_t*);

// Literals & identifiers
static ast_t* parse_id(parser_t*);
static ast_t* parse_lit(parser_t*);

// Expressions/Patterns
static ast_t* parse_primary(parser_t*);
static ast_t* parse_post_unop(parser_t*, ast_t*, uint32_t);
static ast_t* parse_pre_unop(parser_t*, uint32_t);
static ast_t* parse_binop(parser_t*, ast_t*, int);
static ast_t* parse_call(parser_t*, ast_t*);
static ast_t* parse_tuple(parser_t*, const char*, ast_t* (*) (parser_t*));
static ast_t* parse_tuple_or_err(parser_t*, const char*, ast_t* (*) (parser_t*));
static ast_t* parse_array(parser_t*);
static ast_t* parse_field(parser_t*, ast_t*);
static ast_t* parse_fn(parser_t*, ast_t*);
static ast_t* parse_block(parser_t*);
static ast_t* parse_if(parser_t*);
static ast_t* parse_while(parser_t*);
static ast_t* parse_for(parser_t*);
static ast_t* parse_match(parser_t*, ast_t*);
static ast_t* parse_case(parser_t*);
static ast_t* parse_cont(parser_t*, uint32_t);

// Types
static ast_t* parse_prim(parser_t*, uint32_t);
static ast_t* parse_fn_type(parser_t*, ast_t*);

// Declarations
static ast_t* parse_struct(parser_t*);
static ast_t* parse_def(parser_t*);
static ast_t* parse_var_or_val(parser_t*, bool);
static ast_t* parse_mod(parser_t*);
static ast_t* parse_program(parser_t*);

static ast_t* parse_expr(parser_t* parser) {
    ast_t* ast = parse_primary(parser);
    return parse_binop(parser, ast, MAX_BINOP_PRECEDENCE);
}

static ast_t* parse_ptrn(parser_t* parser) {
    ast_t* ast = NULL;
    switch (parser->ahead.tag) {
        case TOK_INT:
        case TOK_FLT:
        case TOK_CHR:
        case TOK_STR:
        case TOK_BLT:
            ast = parse_lit(parser);
            break;
        case TOK_ID:
            ast = parse_id(parser);
            break;
        case TOK_LPAREN:
            ast = parse_tuple(parser, "tuple pattern", parse_ptrn);
            break;
        default:
            return parse_err(parser, "pattern");
    }
    if (parser->ahead.tag == TOK_COLON)
        ast = parse_annot(parser, ast);
    return ast;
}

static ast_t* parse_stmt(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_DEF:
        case TOK_VAR:
        case TOK_VAL:
            return parse_decl(parser);
        case TOK_INT:
        case TOK_FLT:
        case TOK_CHR:
        case TOK_STR:
        case TOK_BLT:
        case TOK_ID:
        case TOK_LPAREN:
        case TOK_LBRACE:
        case TOK_IF:
        case TOK_BREAK:
        case TOK_CONTINUE:
        case TOK_RETURN:
            return parse_expr(parser);
        case TOK_WHILE: return parse_while(parser);
        case TOK_FOR:   return parse_for(parser);
        default:
            break;
    }
    return parse_err(parser, "statement");
}

static ast_t* parse_decl(parser_t* parser) {
    switch (parser->ahead.tag) {
        case TOK_STRUCT: return parse_struct(parser);
        case TOK_DEF:    return parse_def(parser);
        case TOK_VAR:    return parse_var_or_val(parser, true);
        case TOK_VAL:    return parse_var_or_val(parser, false);
        default:
            break;
    }
    return parse_err(parser, "declaration");
}

static ast_t* parse_type(parser_t* parser) {
    ast_t* ast = NULL; 
    switch (parser->ahead.tag) {
#define PRIM(name, str) case TOK_##name: ast = parse_prim(parser, PRIM_##name); break;
        PRIM_LIST(,PRIM)
#undef PRIM
        case TOK_ID:     ast = parse_id(parser); break;
        case TOK_LPAREN: ast = parse_tuple(parser, "tuple type", parse_type); break;
        default:
            return parse_err(parser, "type");
    }
    if (parser->ahead.tag == TOK_RARROW)
        ast = parse_fn_type(parser, ast);
    return ast;
}

static ast_t* parse_err(parser_t* parser, const char* msg) {
    ast_t* ast = ast_create(parser, AST_ERR);
    char buf[TOK2STR_BUF_SIZE + 2];
    log_error(parser->log, &parser->ahead.loc, "expected {0:s}, got {1:s}", { .s = msg }, { .s = tok2str_with_quotes(parser->ahead.tag, buf) });
    next(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_id(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_ID);
    char* str = "";
    if (parser->ahead.tag != TOK_ID) {
        char buf[TOK2STR_BUF_SIZE + 2];
        log_error(parser->log, &parser->ahead.loc, "identifier expected, got {0:s}", { .s = tok2str_with_quotes(parser->ahead.tag, buf) });
    } else {
        str = mpool_alloc(parser->pool, strlen(parser->ahead.str) + 1);
        strcpy(str, parser->ahead.str);
    }
    next(parser);
    ast->data.id.str = str;
    return ast_finalize(ast, parser);
}

static ast_t* parse_lit(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_LIT);
    assert(parser->ahead.tag == TOK_INT ||
           parser->ahead.tag == TOK_FLT ||
           parser->ahead.tag == TOK_CHR ||
           parser->ahead.tag == TOK_STR ||
           parser->ahead.tag == TOK_BLT);
    if (parser->ahead.str) {
        char* str = mpool_alloc(parser->pool, strlen(parser->ahead.str) + 1);
        strcpy(str, parser->ahead.str);
        ast->data.lit.str = str;
    }
    ast->data.lit.value = parser->ahead.lit;
    ast->data.lit.tag   = parser->ahead.tag;
    next(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_primary(parser_t* parser) {
    ast_t* ast = NULL;
    switch (parser->ahead.tag) {
        case TOK_NOT: ast = parse_pre_unop(parser, UNOP_NOT);     break;
        case TOK_ADD: ast = parse_pre_unop(parser, UNOP_PLUS);    break;
        case TOK_SUB: ast = parse_pre_unop(parser, UNOP_NEG);     break;
        case TOK_DEC: ast = parse_pre_unop(parser, UNOP_PRE_DEC); break;
        case TOK_INC: ast = parse_pre_unop(parser, UNOP_PRE_INC); break;
        case TOK_INT:
        case TOK_FLT:
        case TOK_CHR:
        case TOK_STR:
        case TOK_BLT:
            ast = parse_lit(parser);
            break;
        case TOK_IF:       ast = parse_if(parser);                         break;
        case TOK_BREAK:    ast = parse_cont(parser, CONT_BREAK);           break;
        case TOK_CONTINUE: ast = parse_cont(parser, CONT_CONTINUE);        break;
        case TOK_RETURN:   ast = parse_cont(parser, CONT_RETURN);          break;
        case TOK_ID:       ast = parse_id(parser);                         break;
        case TOK_LPAREN:   ast = parse_tuple(parser, "tuple", parse_expr); break;
        case TOK_LBRACKET: ast = parse_array(parser);                      break;
        case TOK_LBRACE:   ast = parse_block(parser);                      break;
        default:
            ast = parse_err(parser, "primary expression");
            break;
    }
    assert(ast);
    if (parser->ahead.tag == TOK_INC)
        ast = parse_post_unop(parser, ast, UNOP_POST_INC);
    else if (parser->ahead.tag == TOK_DEC)
        ast = parse_post_unop(parser, ast, UNOP_POST_DEC);
    else if (parser->ahead.tag == TOK_RARROW)
        ast = parse_fn(parser, ast);

    while (true) {
        switch (parser->ahead.tag) {
            case TOK_LPAREN: ast = parse_call(parser, ast);  continue;
            case TOK_MATCH:  ast = parse_match(parser, ast); continue;
            case TOK_DOT:    ast = parse_field(parser, ast); continue;
            default: break;
        }
        break;
    }

    if (parser->ahead.tag == TOK_COLON)
        ast = parse_annot(parser, ast);
    return ast;
}

static ast_t* parse_post_unop(parser_t* parser, ast_t* arg, uint32_t tag) {
    ast_t* ast = ast_create_with_loc(parser, AST_UNOP, arg->loc);
    next(parser);
    ast->data.unop.tag = tag;
    ast->data.unop.arg = arg;
    return ast_finalize(ast, parser);
}

static ast_t* parse_pre_unop(parser_t* parser, uint32_t tag) {
    ast_t* ast = ast_create(parser, AST_UNOP);
    next(parser);
    ast->data.unop.tag = tag;
    ast->data.unop.arg = parse_primary(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_call(parser_t* parser, ast_t* callee) {
    ast_t* ast = ast_create_with_loc(parser, AST_CALL, callee->loc);
    ast->data.call.callee = callee;
    ast->data.call.arg    = parse_tuple_or_err(parser, "function call argument", parse_expr);
    return ast_finalize(ast, parser);
}

static ast_t* parse_binop(parser_t* parser, ast_t* left, int max_prec) {
    while (true) {
        uint32_t tag = binop_tag_from_token(parser->ahead.tag);
        if (tag == INVALID_TAG) break;
        int prec = binop_precedence(tag);
        if (prec > max_prec) break;
        next(parser);

        ast_t* right = parse_primary(parser);

        uint32_t next_tag = binop_tag_from_token(parser->ahead.tag);
        if (next_tag != INVALID_TAG) {
            int next_prec = binop_precedence(next_tag);
            if (next_prec < prec)
                right = parse_binop(parser, right, next_prec);
        }

        ast_t* binop = ast_create_with_loc(parser, AST_BINOP, left->loc);
        binop->data.binop.tag   = tag;
        binop->data.binop.left  = left;
        binop->data.binop.right = right;
        left = ast_finalize(binop, parser);
    }
    return left;
}

static ast_t* parse_tuple(parser_t* parser, const char* msg, ast_t* (*parse_elem) (parser_t*)) {
    ast_t* ast = ast_create(parser, AST_TUPLE);
    eat(parser, TOK_LPAREN);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.tuple.args;
    while (parser->ahead.tag != TOK_RPAREN) {
        ast_t* arg = parse_elem(parser);
        cur = ast_list_add(parser, cur, arg);

        eat_nl(parser);
        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    expect(parser, msg, TOK_RPAREN);
    return ast_finalize(ast, parser);
}

static ast_t* parse_tuple_or_err(parser_t* parser, const char* msg, ast_t* (*parse_elem) (parser_t*)) {
    if (parser->ahead.tag == TOK_LPAREN)
        return parse_tuple(parser, msg, parse_elem);
    return parse_err(parser, msg);
}

static ast_t* parse_array(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_ARRAY);
    eat(parser, TOK_LBRACKET);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.array.elems;
    bool regular = false, first = true;
    while (parser->ahead.tag != TOK_RBRACKET) {
        ast_t* elem = parse_expr(parser);
        cur = ast_list_add(parser, cur, elem);

        eat_nl(parser);
        loc_t loc = parser->ahead.loc;
        bool comma = accept(parser, TOK_COMMA);
        bool semi  = !comma && accept(parser, TOK_SEMI);
        if (!comma && !semi)
            break;
        if (!first && regular != semi)
            log_error(parser->log, &loc, "cannot mix regular and irregular arrays");
        first   = false;
        regular = semi;
        eat_nl(parser);
    }
    ast->data.array.regular = regular;
    expect(parser, "array", TOK_RBRACKET);
    return ast_finalize(ast, parser);
}

static ast_t* parse_field(parser_t* parser, ast_t* arg) {
    ast_t* ast = ast_create_with_loc(parser, AST_FIELD, arg->loc);
    eat(parser, TOK_DOT);
    ast->data.field.arg = arg;
    ast->data.field.id  = parse_id(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_fn(parser_t* parser, ast_t* param) {
    ast_t* ast = ast_create_with_loc(parser, AST_FN, param->loc);
    eat(parser, TOK_RARROW);
    ast->data.fn.lambda = true;
    ast->data.fn.param = param;
    if (!ast_is_ptrn(param) || ast_is_refutable(param))
        log_error(parser->log, &param->loc, "invalid function parameter");
    ast->data.fn.body  = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_block(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_BLOCK);
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
    return ast_finalize(ast, parser);
}

static ast_t* parse_if(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_IF);
    eat(parser, TOK_IF);
    eat_nl(parser);
    ast->data.if_.cond = parse_tuple_or_err(parser, "if condition", parse_expr);
    eat_nl(parser);
    ast->data.if_.if_true = parse_expr(parser);
    eat_nl(parser);
    if (parser->ahead.tag == TOK_ELSE) {
        eat(parser, TOK_ELSE);
        eat_nl(parser);
        ast->data.if_.if_false = parse_expr(parser);
    }
    return ast_finalize(ast, parser);
}

static ast_t* parse_while(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_WHILE);
    eat(parser, TOK_WHILE);
    eat_nl(parser);
    ast->data.while_.cond = parse_tuple_or_err(parser, "while condition", parse_expr);
    eat_nl(parser);
    ast->data.while_.body = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_for(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_FOR);
    eat(parser, TOK_FOR);
    eat_nl(parser);
    expect(parser, "for loop", TOK_LPAREN);
    eat_nl(parser);
    ast->data.for_.vars = parse_ptrn(parser);
    eat_nl(parser);
    expect(parser, "for loop", TOK_LARROW);
    eat_nl(parser);
    ast->data.for_.expr = parse_expr(parser);
    eat_nl(parser);
    expect(parser, "for loop", TOK_RPAREN);
    eat_nl(parser);
    ast->data.for_.body = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_match(parser_t* parser, ast_t* arg) {
    ast_t* ast = ast_create(parser, AST_MATCH);
    eat(parser, TOK_MATCH);
    eat_nl(parser);
    expect(parser, "match expression", TOK_LBRACE);
    eat_nl(parser);
    ast->data.match.arg = arg;
    ast_list_t** cur = &ast->data.match.cases;
    while (parser->ahead.tag == TOK_CASE) {
        ast_t* arg = parse_case(parser);
        cur = ast_list_add(parser, cur, arg);

        if (!eat_nl_or_semi(parser))
            break;
    }
    expect(parser, "match expression", TOK_RBRACE);
    return ast_finalize(ast, parser);
}

static ast_t* parse_case(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_CASE);
    eat(parser, TOK_CASE);
    eat_nl(parser);
    ast->data.case_.ptrn = parse_ptrn(parser);
    eat_nl(parser);
    expect(parser, "match case", TOK_RARROW);
    eat_nl(parser);
    ast->data.case_.value = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_cont(parser_t* parser, uint32_t tag) {
    ast_t* ast = ast_create(parser, AST_CONT);
    ast->data.cont.tag = tag;
    next(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_annot(parser_t* parser, ast_t* arg) {
    ast_t* ast = ast_create_with_loc(parser, AST_ANNOT, arg->loc);
    eat(parser, TOK_COLON);
    ast->data.annot.ast = arg;
    ast->data.annot.type = parse_type(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_prim(parser_t* parser, uint32_t tag) {
    ast_t* ast = ast_create(parser, AST_PRIM);
    next(parser);
    ast->data.prim.tag = tag;
    return ast_finalize(ast, parser);
}

static ast_t* parse_fn_type(parser_t* parser, ast_t* from) {
    ast_t* ast = ast_create_with_loc(parser, AST_FN, from->loc);
    eat(parser, TOK_RARROW);
    ast->data.fn.lambda = false;
    ast->data.fn.param = from;
    ast->data.fn.body  = parse_type(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_struct(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_STRUCT);
    eat(parser, TOK_STRUCT);
    if (parser->ahead.tag == TOK_BYREF) {
        eat(parser, TOK_BYREF);
        ast->data.struct_.byref = true;
    }
    ast->data.struct_.id = parse_id(parser);
    ast->data.struct_.members = parse_tuple(parser, "structure definition", parse_ptrn);
    if (ast_is_refutable(ast->data.struct_.members))
        log_error(parser->log, &ast->data.struct_.members->loc, "invalid structure definition");
    return ast_finalize(ast, parser);
}

static ast_t* parse_def(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_DEF);
    eat(parser, TOK_DEF);
    eat_nl(parser);
    ast->data.def.id = parse_id(parser);
    eat_nl(parser);
    if (parser->ahead.tag == TOK_LPAREN) {
        ast->data.def.param = parse_tuple(parser, "function parameter", parse_ptrn);
        if (ast_is_refutable(ast->data.def.param))
            log_error(parser->log, &ast->data.def.param->loc, "invalid function parameter");
    }
    if (parser->ahead.tag == TOK_COLON) {
        eat(parser, TOK_COLON);
        ast->data.def.ret = parse_type(parser);
    }
    expect(parser, "definition", TOK_EQ);
    eat_nl(parser);
    ast->data.def.value = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_var_or_val(parser_t* parser, bool var) {
    ast_t* ast = ast_create(parser, var ? AST_VAR : AST_VAL);
    eat(parser, var ? TOK_VAR : TOK_VAL);
    eat_nl(parser);
    ast->data.varl.ptrn = parse_ptrn(parser);
    if (ast_is_refutable(ast->data.varl.ptrn))
        log_error(parser->log, &ast->data.varl.ptrn->loc, var ? "invalid variable pattern" : "invalid value pattern");
    eat_nl(parser);
    expect(parser, var ? "variable" : "value", TOK_EQ);
    eat_nl(parser);
    ast->data.varl.value = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_mod(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_MOD);
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
    return ast_finalize(ast, parser);
}

static ast_t* parse_program(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_PROGRAM);
    ast_list_t** cur = &ast->data.program.mods;
    while (parser->ahead.tag == TOK_MOD) {
        eat_nl(parser);
        ast_t* mod = parse_mod(parser);
        eat_nl(parser);
        cur = ast_list_add(parser, cur, mod);
    }
    expect(parser, "program", TOK_EOF);
    return ast;
}

ast_t* parse(parser_t* parser) {
    next(parser);
    return parse_program(parser);
}
