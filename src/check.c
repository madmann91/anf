#include "check.h"

static fp_flags_t default_fp_flags() {
    return fp_flags_relaxed();
}

static const type_t* expect(checker_t* checker, ast_t* ast, const char* msg, const type_t* type, const type_t* expected) {
    assert(expected);
    if (!type || !type_is_subtype(type, expected)) {
        // When a type contains top, this is an error that has already been logged before
        if (type_contains(type, type_top(checker->mod)))
            return expected;
        static const char* fmts[] = {
            "expected type '{2:t}', but got {0:s} with type '{1:t}'",
            "expected type '{2:t}', but got {0:s}",
            "expected type '{2:t}', but got type '{1:t}'"
        };
        size_t i = msg && type ? 0 : (msg ? 1 : 2);
        log_error(checker->log, &ast->loc, fmts[i], { .s = msg }, { .t = type }, { .t = expected });
        return expected;
    }
    return type;
}

static const type_t* infer_ptrn(checker_t* checker, ast_t* ptrn, ast_t* value) {
    switch (ptrn->tag) {
        case AST_TUPLE:
            {
                size_t nargs = ast_list_length(ptrn->data.tuple.args);
                if (value->tag == AST_TUPLE && nargs == ast_list_length(value->data.tuple.args)) {
                    TMP_BUF_ALLOC(type_ops, const type_t*, nargs)
                    ast_list_t* cur_ptrn  = ptrn->data.tuple.args;
                    ast_list_t* cur_value = value->data.tuple.args;
                    size_t nops = 0;
                    while (cur_ptrn) {
                        type_ops[nops++] = infer_ptrn(checker, cur_ptrn->ast, cur_value->ast);
                        cur_ptrn = cur_ptrn->next;
                        cur_value = cur_value->next;
                    }
                    const type_t* type = type_tuple(checker->mod, nargs, type_ops);
                    TMP_BUF_FREE(type_ops)
                    return type;
                }
            }
            break;
        case AST_ANNOT:
            {
                const type_t* type = infer(checker, ptrn->data.annot.type);
                check(checker, ptrn->data.annot.ast, type);
                return check(checker, value, type);
            }
        default:
            break;
    }
    return check(checker, ptrn, infer(checker, value));
}

static const type_t* infer_internal(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROGRAM:
            FORALL_AST(ast->data.program.mods, mod, { infer(checker, mod); })
            return NULL;
        case AST_MOD:
            FORALL_AST(ast->data.mod.decls, decl, { infer(checker, decl); })
            return NULL;
        case AST_STRUCT:
            {
                const char* name = ast->data.struct_.id->data.id.str;
                struct_def_t* struct_def = mpool_alloc(&checker->mod->pool, sizeof(struct_def_t));
                char* buf = mpool_alloc(&checker->mod->pool, strlen(name) + 1);
                strcpy(buf, name);
                *struct_def = (struct_def_t) {
                    .name    = buf,
                    .byref   = ast->data.struct_.byref,
                    .members = NULL
                };
                ast->type = type_struct(checker->mod, struct_def, 0, NULL);
                ast->type->data.struct_def->members = infer(checker, ast->data.struct_.members);
                return ast->type;
            }
        case AST_ID:
            if (ast->data.id.to) {
                // This identifier is refering to some other, previously declared identifier
                return infer(checker, (ast_t*)ast->data.id.to);
            }
            log_error(checker->log, &ast->loc, "cannot infer type for identifier '{0:s}'", { .s = ast->data.id.str });
            return type_top(checker->mod);
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
        case AST_FIELD:
            {
                const type_t* struct_type = infer(checker, ast->data.field.arg);
                if (struct_type->tag != TYPE_STRUCT) {
                    if (struct_type->tag != TYPE_TOP)
                        log_error(checker->log, &ast->loc, "structure type expected in field expression, but got '{0:t}'", { .t = struct_type });
                    return type_top(checker->mod);
                }
                // TODO
                assert(false);
                return NULL;
            }
        case AST_CALL:
            {
                const type_t* callee_type = infer(checker, ast->data.call.callee);
                switch (callee_type->tag) {
                    case TYPE_FN:
                        check(checker, ast->data.call.arg, callee_type->ops[0]);
                        return callee_type->ops[1];
                    default:
                        if (callee_type->tag != TYPE_TOP)
                            log_error(checker->log, &ast->loc, "function type expected in call expression, but got '{0:t}'", { .t = callee_type });
                        return type_top(checker->mod);
                }
            }
        case AST_ANNOT:
            {
                const type_t* type = infer(checker, ast->data.annot.type);
                check(checker, ast->data.annot.ast, type);
                return type;
            }
        case AST_NAME:
            return infer(checker, ast->data.name.value);
        case AST_VAR:
        case AST_VAL:
            return infer_ptrn(checker, ast->data.varl.ptrn, ast->data.varl.value);
        case AST_DEF:
            {
                const type_t* param_type = infer(checker, ast->data.def.param);
                const type_t* ret_type   = ast->data.def.ret ? infer(checker, ast->data.def.ret) : type_unit(checker->mod);
                // Set the type immediately to allow checking recursive calls
                ast->type = type_fn(checker->mod, param_type, ret_type);
                check(checker, ast->data.def.value, ret_type);
                return ast->type;
            }
        case AST_BLOCK:
            {
                const type_t* last = type_unit(checker->mod);
                FORALL_AST(ast->data.block.stmts, stmt, { last = infer(checker, stmt); })
                return last;
            }
        case AST_FN:
            {
                const type_t* param_type = infer(checker, ast->data.fn.param);
                const type_t* body_type  = infer(checker, ast->data.fn.body);
                return type_fn(checker->mod, param_type, body_type);
            }
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_INT:  return type_i32(checker->mod);
                case LIT_FLT:  return type_f32(checker->mod, default_fp_flags());
                case LIT_STR:  return type_array(checker->mod, type_u8(checker->mod));
                case LIT_CHR:  return type_u8(checker->mod);
                case LIT_BOOL: return type_bool(checker->mod);
                default:
                    assert(false);
                    return NULL;
            }
        case AST_CONT:
            {
                assert(ast->data.cont.parent && ast->data.cont.parent->type);
                const type_t* parent_type = ast->data.cont.parent->type;
                switch (ast->data.cont.tag) {
                    case CONT_RETURN:
                        assert(parent_type->tag == TYPE_FN);
                        return type_fn(checker->mod, parent_type->ops[1], type_bottom(checker->mod));
                    case CONT_CONTINUE:
                    case CONT_BREAK:
                        return type_fn(checker->mod, type_unit(checker->mod), type_bottom(checker->mod));
                    default:
                        assert(false);
                        return NULL;
                }
            }
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
        case AST_FN:
            {
                if (expected->tag != TYPE_FN)
                    return expect(checker, ast, "anonymous function", NULL, expected);
                const type_t* param_type = check(checker, ast->data.fn.param, expected->ops[0]);
                const type_t* body_type  = check(checker, ast->data.fn.body, expected->ops[1]);
                return type_fn(checker->mod, param_type, body_type);
            }
        case AST_IF:
            check(checker, ast->data.if_.cond, type_bool(checker->mod));
            if (ast->data.if_.if_false) {
                check(checker, ast->data.if_.if_true, expected);
                return check(checker, ast->data.if_.if_false, expected);
            }
            return check(checker, ast->data.if_.if_true, type_unit(checker->mod));
        case AST_MATCH:
            {
                const type_t* arg_type = infer(checker, ast->data.match.arg);
                FORALL_AST(ast->data.match.cases, case_, {
                    assert(case_->tag == AST_CASE);
                    check(checker, case_->data.case_.ptrn, arg_type);
                    case_->type = check(checker, case_->data.case_.value, expected);
                });
                return expected;
            }
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
            {
                const type_t* type = infer(checker, ast);
                return expect(checker, ast, NULL, type, expected);
            }
    }
}

const type_t* check(checker_t* checker, ast_t* ast, const type_t* expected) {
    assert(!ast->type);
    return ast->type = check_internal(checker, ast, expected);
}

const type_t* infer(checker_t* checker, ast_t* ast) {
    if (ast->type)
        return ast->type;
    return ast->type = infer_internal(checker, ast);
}
