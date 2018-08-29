#include "ast.h"

bool ast_is_refutable(const ast_t* ast) {
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
