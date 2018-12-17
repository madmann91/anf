#include "check.h"

static inline bool subtype(const type_t* src, const type_t* dst) {
    // TODO
    (void)src,(void)dst;
    return false;
}

static const type_t* unify(checker_t* checker, const type_t* src, const type_t* dst) {
    // TODO
    (void)checker;

    if (!src) return dst;
    if (subtype(src, dst)) return dst;
    return NULL;
}

const type_t* check(checker_t* checker, ast_t* ast, const type_t* type) {
    switch (ast->tag) {
        case AST_ID:
            type = unify(checker, ast->type, type);
            if (!type) {
                log_error(checker->log, &ast->loc, "conflicting types for '{0:s}', got '{1:t}' and '{2:t}'",
                    { .s = ast->data.id.str}, { .t = ast->type }, { .t = type });
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

const type_t* infer_head(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROGRAM:
            // TODO
            // ast->type = type_module(checker->mod, ast, ast_list_size(ast->data.program.decls));
            break;
        case AST_MOD:
            FORALL_AST(ast->data.mod.decls, decl, {
                if (decl->tag == AST_STRUCT)
                    decl->type = type_struct(checker->mod, 0, 0, NULL); // TODO
            })
            break;
    }
    return NULL;
} 

const type_t* infer(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_ID:
            if (ast->data.id.to) {
                // This identifier is refering to some other, previously declared identifier
                ast->type = ast->data.id.to->type;
                return ast->type;
            }
            return ast->type;
        case AST_PRIM:
            switch (ast->data.prim.tag) {
                case TYPE_F32:
                case TYPE_F64:
                    return type_prim_fp(checker->mod, ast->data.prim.tag, fp_flags_relaxed());
                default:
                    return type_prim(checker->mod, ast->data.prim.tag);
            }
        case AST_ANNOT:
            return check(checker, ast->data.annot.ast, infer(checker, ast->data.annot.type));
        case AST_VAR:
        case AST_VAL:
            return check(checker, ast->data.varl.ptrn, infer(checker, ast->data.varl.value));
        case AST_DEF:
            {
                const type_t* type = infer(checker, ast->data.varl.ptrn);
                if (type)
                    return check(checker, ast->data.varl.value, type);
            }
            break;
    }
    return NULL;
}
