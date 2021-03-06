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
    if (!accept(parser, tag)) {
        char buf1[TOK2STR_BUF_SIZE + 2];
        char buf2[TOK2STR_BUF_SIZE + 2];
        const char* str1 = tok2str_with_quotes(tag, buf1);
        const char* str2 = tok2str_with_quotes(parser->ahead.tag, buf2);
        log_error(parser->log, &parser->ahead.loc, "expected {0:s} in {1:s}, but got {2:s}", { .s = str1 }, { .s = msg }, { .s = str2 });
        next(parser);
        return false;
    }
    return true;
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
static ast_t* parse_id(parser_t*, bool);
static ast_t* parse_lit(parser_t*);

// Expressions/Patterns
static ast_t* parse_primary(parser_t*);
static ast_t* parse_post_unop(parser_t*, ast_t*, uint32_t);
static ast_t* parse_pre_unop(parser_t*, uint32_t);
static ast_t* parse_binop(parser_t*, ast_t*, int);
static ast_t* parse_call(parser_t*, ast_t*);
static ast_t* parse_tuple(parser_t*, const char*, ast_t* (*) (parser_t*));
static ast_t* parse_name(parser_t*);
static ast_t* parse_args(parser_t*);
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
static ast_t* parse_array_type(parser_t*);

// Declarations
static ast_t* parse_member_or_param(parser_t*, const char* msg);
static ast_t* parse_member(parser_t*);
static ast_t* parse_param(parser_t*);
static ast_t* parse_tvar(parser_t*);
static ast_list_t* parse_tvars(parser_t*);
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
            ast = parse_id(parser, true);
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
        case TOK_ID:       ast = parse_id(parser, true);                        break;
        case TOK_LPAREN:   ast = parse_tuple(parser, "tuple type", parse_type); break;
        case TOK_LBRACKET: ast = parse_array_type(parser);                      break;
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
    log_error(parser->log, &parser->ahead.loc, "expected {0:s}, but got {1:s}", { .s = msg }, { .s = tok2str_with_quotes(parser->ahead.tag, buf) });
    next(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_id(parser_t* parser, bool types) {
    ast_t* ast = ast_create(parser, AST_ID);
    char* str = "";
    if (parser->ahead.tag != TOK_ID) {
        char buf[TOK2STR_BUF_SIZE + 2];
        log_error(parser->log, &parser->ahead.loc, "expected identifier, but got {0:s}", { .s = tok2str_with_quotes(parser->ahead.tag, buf) });
    } else {
        str = mpool_alloc(parser->pool, strlen(parser->ahead.str) + 1);
        strcpy(str, parser->ahead.str);
    }
    ast->data.id.str = str;
    next(parser);
    if (types && accept(parser, TOK_LBRACKET)) {
        ast_list_t** cur = &ast->data.id.types;
        while (parser->ahead.tag != TOK_RBRACKET) {
            cur = ast_list_add(parser, cur, parse_type(parser));

            eat_nl(parser);
            if (!accept(parser, TOK_COMMA))
                break;
            eat_nl(parser);
        }
        expect(parser, "type arguments", TOK_RBRACKET);
    }
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

static ast_t* parse_suffix(parser_t* parser, ast_t* ast) {
    if (parser->ahead.tag == TOK_INC)
        ast = parse_post_unop(parser, ast, UNOP_POST_INC);
    else if (parser->ahead.tag == TOK_DEC)
        ast = parse_post_unop(parser, ast, UNOP_POST_DEC);
    else if (parser->ahead.tag == TOK_RARROW)
        ast = parse_fn(parser, ast);

    while (true) {
        switch (parser->ahead.tag) {
            case TOK_LPAREN:   ast = parse_call(parser, ast);  continue;
            case TOK_MATCH:    ast = parse_match(parser, ast); continue;
            case TOK_DOT:      ast = parse_field(parser, ast); continue;
            default: break;
        }
        break;
    }

    if (parser->ahead.tag == TOK_COLON)
        ast = parse_annot(parser, ast);
    return ast;

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
        case TOK_ID:       ast = parse_id(parser, true);                   break;
        case TOK_LPAREN:   ast = parse_tuple(parser, "tuple", parse_expr); break;
        case TOK_LBRACKET: ast = parse_array(parser);                      break;
        case TOK_LBRACE:   ast = parse_block(parser);                      break;
        default:
            ast = parse_err(parser, "primary expression");
            break;
    }
    assert(ast);
    return parse_suffix(parser, ast);
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
    ast_list_t** cur = &ast->data.call.args;
    while (parser->ahead.tag == TOK_LPAREN) {
        cur = ast_list_add(parser, cur, parse_args(parser));
    }
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
        cur = ast_list_add(parser, cur, parse_elem(parser));

        eat_nl(parser);
        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    expect(parser, msg, TOK_RPAREN);
    return ast_finalize(ast, parser);
}

static ast_t* parse_name(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_FIELD);
    ast->data.field.name = true;
    ast->data.field.id = parse_id(parser, false);
    if (!expect(parser, "named argument", TOK_EQ))
        log_note(parser->log, &parser->ahead.loc, "positional arguments cannot be placed after named arguments");
    ast->data.field.arg = parse_expr(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_args(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_TUPLE);
    eat(parser, TOK_LPAREN);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.tuple.args;
    bool named = false;
    while (parser->ahead.tag != TOK_RPAREN) {
        ast_t* arg = named ? parse_name(parser) : parse_expr(parser);
        if (!named &&
            arg->tag == AST_BINOP &&
            arg->data.binop.tag == BINOP_ASSIGN &&
            arg->data.binop.left->tag == AST_ID) {
            ast_t* left  = arg->data.binop.left;
            ast_t* right = arg->data.binop.right;
            arg->tag = AST_FIELD;
            arg->data.field.name = true;
            arg->data.field.index = 0;
            arg->data.field.id = left;
            arg->data.field.arg = right;
            named = true;
        }
        cur = ast_list_add(parser, cur, arg);

        eat_nl(parser);
        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    ast->data.tuple.named = named;
    expect(parser, "call arguments", TOK_RPAREN);
    return ast_finalize(ast, parser);
}

static ast_t* parse_array(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_ARRAY);
    eat(parser, TOK_LBRACKET);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.array.elems;
    while (parser->ahead.tag != TOK_RBRACKET) {
        cur = ast_list_add(parser, cur, parse_expr(parser));

        eat_nl(parser);
        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    expect(parser, "array", TOK_RBRACKET);
    return ast_finalize(ast, parser);
}

static ast_t* parse_field(parser_t* parser, ast_t* arg) {
    ast_t* ast = ast_create_with_loc(parser, AST_FIELD, arg->loc);
    eat(parser, TOK_DOT);
    ast->data.field.name = false;
    ast->data.field.arg = arg;
    ast->data.field.id  = parse_id(parser, false);
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
    ast_list_t** cur = &ast->data.block.stmts;
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
    expect(parser, "if condition", TOK_LPAREN);
    eat_nl(parser);
    ast->data.if_.cond = parse_expr(parser);
    eat_nl(parser);
    expect(parser, "if condition", TOK_RPAREN);
    eat_nl(parser);
    ast->data.if_.if_true = parse_expr(parser);
    eat_nl(parser);
    if (accept(parser, TOK_ELSE)) {
        eat_nl(parser);
        ast->data.if_.if_false = parse_expr(parser);
    }
    return ast_finalize(ast, parser);
}

static ast_t* parse_while(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_WHILE);
    eat(parser, TOK_WHILE);
    eat_nl(parser);
    expect(parser, "while condition", TOK_LPAREN);
    eat_nl(parser);
    ast->data.while_.cond = parse_expr(parser);
    eat_nl(parser);
    expect(parser, "while condition", TOK_RPAREN);
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
    ast_t* ptrn = parse_ptrn(parser);
    eat_nl(parser);
    expect(parser, "for loop", TOK_LARROW);
    eat_nl(parser);
    ast_t* call = parse_expr(parser);
    eat_nl(parser);
    expect(parser, "for loop", TOK_RPAREN);
    eat_nl(parser);
    ast_t* body = parse_expr(parser);

    ast->data.for_.call = call;
    if (call->tag != AST_CALL) {
        log_error(parser->log, &call->loc, "invalid for loop expression");
    } else {
        if (ast_is_refutable(ptrn))
            log_error(parser->log, &ptrn->loc, "invalid for loop arguments");

        ast_t* arg = ast_create_with_loc(parser, AST_TUPLE, body->loc);
        ast_t* fn = ast_create_with_loc(parser, AST_FN, body->loc);
        ast_list_add(parser, &arg->data.tuple.args, fn);
        fn->data.fn.lambda = true;
        fn->data.fn.param = ptrn;
        fn->data.fn.body = body;
        ast_finalize(fn, parser);
        ast_finalize(arg, parser);
        ast_list_t* front = mpool_alloc(parser->pool, sizeof(ast_list_t));
        front->ast = arg;
        front->next = call->data.call.args;
        call->data.call.args = front;
    }
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
        cur = ast_list_add(parser, cur, parse_case(parser));

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
    eat_nl(parser);
    ast->data.annot.arg = arg;
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
    ast->data.fn.param  = from;
    ast->data.fn.body   = parse_type(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_array_type(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_ARRAY);
    eat(parser, TOK_LBRACKET);
    ast_list_t** cur = &ast->data.array.elems;
    cur = ast_list_add(parser, cur, parse_type(parser));
    expect(parser, "array type", TOK_RBRACKET);
    return ast_finalize(ast, parser);
}

static ast_t* parse_member_or_param(parser_t* parser, const char* msg) {
    ast_t* ast = ast_create(parser, AST_ANNOT);
    ast->data.annot.arg = parse_id(parser, false);
    eat_nl(parser);
    expect(parser, msg, TOK_COLON);
    eat_nl(parser);
    ast->data.annot.type = parse_type(parser);
    return ast_finalize(ast, parser);
}

static ast_t* parse_member(parser_t* parser) {
    return parse_member_or_param(parser, "structure member");
}

static ast_t* parse_param(parser_t* parser) {
    return parse_member_or_param(parser, "function parameter");
}

static ast_t* parse_tvar(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_TVAR);
    ast->data.tvar.id = parse_id(parser, false);
    eat_nl(parser);
    if (accept(parser, TOK_COLON)) {
        eat_nl(parser);
        ast_list_t** cur = &ast->data.tvar.traits;
        while (true) {
            cur = ast_list_add(parser, cur, parse_type(parser));
            eat_nl(parser);
            if (!accept(parser, TOK_ADD))
                break;
            eat_nl(parser);
        }
    }
    return ast_finalize(ast, parser);
}

static ast_list_t* parse_tvars(parser_t* parser) {
    if (!accept(parser, TOK_LBRACKET))
        return NULL;
    ast_list_t* first = NULL;
    ast_list_t** cur = &first;
    while (parser->ahead.tag != TOK_RBRACKET) {
        cur = ast_list_add(parser, cur, parse_tvar(parser));
        eat_nl(parser);
        if (!accept(parser, TOK_COMMA))
            break;
        eat_nl(parser);
    }
    expect(parser, "type parameters", TOK_RBRACKET);
    return first;
}

static ast_t* parse_struct(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_STRUCT);
    eat(parser, TOK_STRUCT);
    if (accept(parser, TOK_BYREF))
        ast->data.struct_.byref = true;
    ast->data.struct_.id = parse_id(parser, false);
    ast->data.struct_.tvars = parse_tvars(parser);
    ast->data.struct_.members = parse_tuple(parser, "structure members", parse_member);
    if (ast_is_refutable(ast->data.struct_.members))
        log_error(parser->log, &ast->data.struct_.members->loc, "invalid structure definition");
    return ast_finalize(ast, parser);
}

static ast_t* parse_def(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_DEF);
    eat(parser, TOK_DEF);
    eat_nl(parser);
    ast->data.def.id = parse_id(parser, false);
    ast->data.def.tvars = parse_tvars(parser);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.def.params;
    while (parser->ahead.tag == TOK_LPAREN) {
        ast_t* param = parse_tuple(parser, "function parameters", parse_param);
        if (ast_is_refutable(param))
            log_error(parser->log, &param->loc, "invalid function parameters");
        eat_nl(parser);
        cur = ast_list_add(parser, cur, param);
    }
    if (!ast->data.def.params)
        log_error(parser->log, &parser->ahead.loc, "missing function parameters");
    if (accept(parser, TOK_COLON)) {
        eat_nl(parser);
        ast->data.def.ret = parse_type(parser);
    }
    eat_nl(parser);
    if (accept(parser, TOK_EQ)) {
        eat_nl(parser);
        ast->data.def.value = parse_expr(parser);
    } else if (parser->ahead.tag == TOK_LBRACE) {
        ast->data.def.value = parse_block(parser);
    } else {
        ast->data.def.value = parse_err(parser, "function body");
    }
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
    ast->data.mod.id = parse_id(parser, false);
    eat_nl(parser);
    expect(parser, "module contents", TOK_LBRACE);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.mod.decls;
    while (parser->ahead.tag != TOK_RBRACE) {
        cur = ast_list_add(parser, cur, parse_decl(parser));

        if (!eat_nl_or_semi(parser))
            break;
    }
    expect(parser, "module contents", TOK_RBRACE);
    return ast_finalize(ast, parser);
}

static ast_t* parse_program(parser_t* parser) {
    ast_t* ast = ast_create(parser, AST_PROG);
    eat_nl(parser);
    ast_list_t** cur = &ast->data.prog.mods;
    while (parser->ahead.tag == TOK_MOD) {
        cur = ast_list_add(parser, cur, parse_mod(parser));
        eat_nl(parser);
    }
    expect(parser, "program", TOK_EOF);
    return ast;
}

ast_t* parse(parser_t* parser) {
    next(parser);
    return parse_program(parser);
}
