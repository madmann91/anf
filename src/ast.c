#include "ast.h"

bool ast_is_decl(const ast_t* ast) {
    switch (ast->tag) {
        case AST_DEF:
        case AST_VAR:
        case AST_VAL:
        case AST_MOD:
            return true;
        default:
            return false;
    }
}

bool ast_is_expr(const ast_t* ast) {
    ast_list_t* list;
    switch (ast->tag) {
        case AST_ID:
        case AST_LIT:
            return true;
        case AST_TUPLE:
            list = ast->data.tuple.args;
            while (list) {
                if (!ast_is_expr(list->ast))
                    return false;
                list = list->next;
            }
            return true;
        default:
            return false;
    }
}

bool ast_is_ptrn(const ast_t* ast) {
    ast_list_t* list;
    switch (ast->tag) {
        case AST_ID:
        case AST_LIT:
            return true;
        case AST_TUPLE:
            list = ast->data.tuple.args;
            while (list) {
                if (!ast_is_ptrn(list->ast))
                    return false;
                list = list->next;
            }
            return true;
        default:
            return false;
    }
}

bool ast_is_refutable(const ast_t* ast) {
    assert(ast_is_ptrn(ast));
    ast_list_t* list;
    switch (ast->tag) {
        case AST_ID:  return false;
        case AST_LIT: return true;
        case AST_TUPLE:
            list = ast->data.tuple.args;
            while (list) {
                if (ast_is_refutable(list->ast))
                    return true;
                list = list->next;
            }
            return false;
        default:
            assert(false);
            return false;
    }
}
