#include "check.h"

static inline bool subtype(const type_t* src, const type_t* dst) {
    return src == dst || src->tag == TYPE_BOTTOM;
}

static const type_t* expect(checker_t* checker, ast_t* ast, const char* msg, const type_t* type, const type_t* expected) {
    assert(expected);
    if (!type || !subtype(type, expected)) {
        const char* fmt = type
            ? "expected expression of type '{2:t}', but got {0:s} with type '{1:t}'"
            : "expected expression of type '{2:t}', but got {0:s}";
        log_error(checker->log, &ast->loc, fmt, { .s = msg }, { .t = type }, { .t = expected });
        return expected;
    }
    return type;
}

static void infer_head(checker_t* checker, ast_t* ast) {
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
}

static const type_t* infer_internal(checker_t* checker, ast_t* ast) {
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
                return ast->data.id.to->type;
            }
            return NULL;
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
                FORALL_AST(ast->data.tuple.args, arg, {
                    type_ops[nops++] = infer(checker, arg);
                })
                const type_t* type = type_tuple(checker->mod, nops, type_ops);
                TMP_BUF_FREE(type_ops)
                return type;
            }
        case AST_ANNOT:
            {
                const type_t* type = infer(checker, ast->data.annot.type);
                check(checker, ast->data.annot.ast, type);
                return type;
            }
        case AST_VAR:
        case AST_VAL:
            {
                const type_t* type = infer(checker, ast->data.varl.value);
                if (!type)
                    return infer(checker, ast->data.varl.ptrn);
                check(checker, ast->data.varl.ptrn, type);
                return type;
            }
        case AST_DEF:
            {
                const type_t* param_type = infer(checker, ast->data.def.param);
                const type_t* ret_type = infer(checker, ast->data.def.ret);
                if (!ret_type)
                    ret_type = type_unit(checker->mod);
                check(checker, ast->data.def.value, ret_type);
                return type_fn(checker->mod, param_type, ret_type);
            }
        case AST_BLOCK:
            FORALL_AST(ast->data.block.stmts, stmt, { infer(checker, stmt); })
            break;
        default:
            assert(false);
            return NULL;
    }
}

static const type_t* check_internal(checker_t* checker, ast_t* ast, const type_t* expected) {
    assert(expected);
    switch (ast->tag) {
        case AST_ID:
            return ast->data.id.to ? expect(checker, ast, "identifier", ast->data.id.to->type, expected) : expected;
        case AST_BLOCK:
            for (ast_list_t* cur = ast->data.block.stmts; cur; cur = cur->next) {
                // Check last element, infer every other one
                if (cur->next)
                    infer(checker, cur->ast);
                else {
                    check(checker, cur->ast, expected);
                    return cur->ast->type;
                }
            }
            return expect(checker, ast, "block", type_unit(checker->mod), expected);
        case AST_IF:
            check(checker, ast->data.if_.cond, type_bool(checker->mod));
            if (ast->data.if_.if_false) {
                check(checker, ast->data.if_.if_true, expected);
                return check(checker, ast->data.if_.if_false, expected);
            }
            return check(checker, ast->data.if_.if_true, type_unit(checker->mod));
        case AST_TUPLE:
            {
                size_t nargs = ast_list_length(ast->data.tuple.args);
                if (nargs == 1)
                    return check(checker, ast->data.tuple.args->ast, expected);
                if (expected->tag != TYPE_TUPLE)
                    return expect(checker, ast, "tuple", NULL, expected);
                if (expected->nops != nargs) {
                    log_error(checker->log, &ast->loc, "expected {0:u32} arguments, but got {1:u32}", { .u32 = expected->nops }, { .u32 = nargs });
                    return expected;
                }
                size_t nops = 0;
                TMP_BUF_ALLOC(type_ops, const type_t*, nargs)
                FORALL_AST(ast->data.tuple.args, arg, {
                    type_ops[nops] = check(checker, arg, expected->ops[nops]);
                    nops++;
                });
                const type_t* type = type_tuple(checker->mod, nops, type_ops);
                TMP_BUF_FREE(type_ops)
                return type;
            }
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_INT:
                    if (type_is_i(expected) || type_is_u(expected) || type_is_f(expected))
                        return expected;
                    return expect(checker, ast, "integer literal", NULL, expected);
                case LIT_FLT:
                    if (type_is_f(expected))
                        return expected;
                    return expect(checker, ast, "floating point literal", NULL, expected);
                case LIT_STR:
                    return expect(checker, ast, "string literal", type_array(checker->mod, type_u8(checker->mod)), expected);
                case LIT_CHR:
                    return expect(checker, ast, "character literal", type_u8(checker->mod), expected);
                case LIT_BOOL:
                    return expect(checker, ast, "boolean literal", type_bool(checker->mod), expected);
                default:
                    assert(false);
                    break;
            }
            break;
        default:
            assert(false);
            return NULL;
    }
}

const type_t* check(checker_t* checker, ast_t* ast, const type_t* expected) {
    return ast->type = check_internal(checker, ast, expected);
}

const type_t* infer(checker_t* checker, ast_t* ast) {
    return ast->type = infer_internal(checker, ast);
}
