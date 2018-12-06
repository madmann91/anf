#include "check.h"

static inline bool subtype(const ast_type_t* src, const ast_type_t* dst) {
    return false;
}

static const ast_type_t* unify(checker_t* checker, const ast_type_t* src, const ast_type_t* dst) {
    if (!src) return dst;
    if (subtype(src, dst)) return dst;
    return NULL;
}

const ast_type_t* check(checker_t* checker, ast_t* ast, const ast_type_t* type) {
    switch (ast->tag) {
        case AST_ID:
            type = unify(checker, ast->type, type);
            if (!type) {
                log_error(checker->log, &ast->loc, "conflicting types for '{0:s}', got '{1:at}' and '{2:at}'",
                    { .s = ast->data.id.str}, { .at = ast->type }, { .at = type });
            }
            ast->type = type;
            break;
        case AST_MOD:
            FORALL_AST(ast->data.mod.decls, decl, {
                check(checker, decl, NULL /*TODO*/);
            })
            break;
    }
    return type;
}

const ast_type_t* infer(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_ID:
            if (ast->data.id.to) {
                // This identifier is refering to some other, previously declared identifier
                ast->type = ast->data.id.to->type;
                return ast->type;
            }
            return ast->type;
        case AST_ANNOT:
            return check(checker, ast->data.annot.ast, infer(checker, ast->data.annot.type));
        case AST_VAR:
        case AST_VAL:
            return check(checker, ast->data.varl.ptrn, infer(checker, ast->data.varl.value));
        case AST_DEF:
            {
                const ast_type_t* type = infer(checker, ast->data.varl.ptrn);
                if (type)
                    return check(checker, ast->data.varl.value, type);
            }
            break;
    }
    return NULL;
}
