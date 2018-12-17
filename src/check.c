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
            break;
        case AST_MOD:
            FORALL_AST(ast->data.mod.decls, decl, {
                check(checker, decl, NULL /*TODO*/);
            })
            break;
    }
    ast->type = type;
    return type;
}

const type_t* infer_head(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_MOD:
            FORALL_AST(ast->data.mod.decls, decl, {
                assert(!decl->type);
                if (decl->tag == AST_STRUCT) {
                    // Create structure definition
                    const char* name = decl->data.struct_.id->data.id.str;
                    struct_def_t* struct_def = mpool_alloc(&checker->mod->pool, sizeof(struct_def_t));
                    char* buf = mpool_alloc(&checker->mod->pool, strlen(name) + 1);
                    strcpy(buf, name);
                    *struct_def = (struct_def_t) {
                        .name    = buf,
                        .byref   = decl->data.struct_.byref,
                        .members = NULL
                    };
                    decl->type = type_struct(checker->mod, struct_def, 0, NULL);
                }
            })
            break;
    }
    return NULL;
} 

const type_t* infer(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROGRAM:
            FORALL_AST(ast->data.program.mods, mod, { infer(checker, mod); })
            return NULL;
        case AST_MOD:
            infer_head(checker, ast);
            FORALL_AST(ast->data.mod.decls, decl, { infer(checker, decl); })
            return NULL;
        case AST_STRUCT:
            assert(ast->type && ast->type->tag == TYPE_STRUCT && ast->type->data.struct_def);
            ast->type->data.struct_def->members = infer(checker, ast->data.struct_.members);
            return ast->type;
        case AST_ID:
            if (ast->data.id.to) {
                // This identifier is refering to some other, previously declared identifier
                return ast->type = ast->data.id.to->type;
            }
            return ast->type;
        case AST_PRIM:
            switch (ast->data.prim.tag) {
                case TYPE_F32:
                case TYPE_F64:
                    // TODO: Use proper FP flags
                    return type_prim_fp(checker->mod, ast->data.prim.tag, fp_flags_relaxed());
                default:
                    return type_prim(checker->mod, ast->data.prim.tag);
            }
        case AST_TUPLE:
            {
                TMP_BUF_ALLOC(type_ops, const type_t*, ast_list_length(ast->data.tuple.args))
                size_t nops = 0;
                FORALL_AST(ast->data.tuple.args, arg, { type_ops[nops++] = infer(checker, arg); })
                const type_t* type = type_tuple(checker->mod, nops, type_ops);
                TMP_BUF_FREE(type_ops)
                return ast->type = type;
            }
            break;
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
