#include "check.h"

static inline fp_flags_t default_fp_flags() {
    return fp_flags_relaxed();
}

static inline const type_t* expect(checker_t* checker, ast_t* ast, const char* msg, const type_t* type, const type_t* expected) {
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

static inline bool expect_args(checker_t* checker, ast_t* args, const char* msg, size_t nargs, size_t nparams) {
    if (nargs != nparams) {
        log_error(checker->log, &args->loc, "expected {0:u32} argument{1:s} in {2:s}, but got {3:u32}",
            { .u32 = nparams },
            { .s = nparams >= 2 ? "s" : "" },
            { .s = msg },
            { .u32 = nargs });
        return false;
    }
    return true;
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

static const type_t* find_param(ast_list_t* params, const char* name, size_t* index) {
    size_t i = 0;
    FORALL_AST(params, param, {
        assert(param->tag == AST_ANNOT);
        assert(param->data.annot.ast->tag == AST_ID);
        if (!strcmp(name, param->data.annot.ast->data.id.str)) {
            *index = i;
            return param->type;
        }
        i++;
    })
    return NULL;
}

static bool infer_names(checker_t* checker, ast_t* ast) {
    assert(ast->tag == AST_CALL);
    if (!ast->data.call.arg->data.tuple.named)
        return true;

    ast_list_t* args = ast->data.call.arg->data.tuple.args;
    ast_list_t* params = NULL;

    bool is_struct = false;
    ast_t* callee = ast->data.call.callee;
    if (callee->tag == AST_ID) {
        const ast_t* to = callee->data.id.to;
        if (to->tag == AST_DEF)
            params = to->data.def.param->data.tuple.args;
        else if (to->tag == AST_STRUCT) {
            is_struct = true;
            params = to->data.struct_.members->data.tuple.args;
        }
    }

    if (!params) {
        log_error(checker->log, &ast->loc, "named arguments are only allowed for calls to top-level functions or structure constructors");
        return false;
    }

    size_t nparams = ast_list_length(params);
    TMP_BUF_ALLOC(param_seen, bool, nparams)
    for (size_t i = 0; i < nparams; ++i)
        param_seen[i] = false;

    // Mark every positional argument as seen
    size_t param_index = 0;
    ast_list_t* cur_param = params;
    ast_list_t* cur_arg   = args;
    while (cur_arg && cur_param) {
        ast_t* arg = cur_arg->ast;
        if (arg->tag == AST_FIELD && arg->data.field.name)
            break;
        param_seen[param_index] = true;
        param_index++;
        cur_param = cur_param->next;
        cur_arg   = cur_arg->next;
    }

    // For every named argument, find the index into the tuple
    bool status = true;
    const char* param_msg  = is_struct ? "member" : "parameter";
    const char* callee_msg = is_struct ? "structure" : "function";
    FORALL_AST(cur_arg, name, {
        const char* param_name = name->data.field.id->data.id.str;
        const type_t* param_type = find_param(params, param_name, &param_index);
        if (!param_type) {
            log_error(checker->log, &name->loc, "cannot find {0:s} named '{1:s}' in {2:s} '{3:s}'",
                { .s = param_msg },
                { .s = param_name },
                { .s = callee_msg },
                { .s = callee->data.id.str });
            status = false;
        } else if (param_seen[param_index]) {
            log_error(checker->log, &name->loc, "{0:s} '{1:s}' can only be mentioned once",
                { .s = param_msg },
                { .s = param_name });
            status = false;
        } else {
            param_seen[param_index] = true;
            name->data.field.index = param_index;
        }
    })

    TMP_BUF_FREE(param_seen)
    return status;
}

static const type_t* infer_call(checker_t* checker, ast_t* ast) {
    if (!infer_names(checker, ast))
        return type_top(checker->mod);
    const type_t* callee_type = infer(checker, ast->data.call.callee);
    switch (callee_type->tag) {
        case TYPE_ARRAY:
            {
                ast_list_t* args = ast->data.call.arg->data.tuple.args;
                size_t nargs = ast_list_length(args);
                if (!expect_args(checker, ast->data.call.arg, "array indexing expression", nargs, callee_type->data.dim))
                    return type_top(checker->mod);
                FORALL_AST(args, arg, {
                    const type_t* arg_type = infer(checker, arg);
                    if (!type_is_i(arg_type) && !type_is_u(arg_type)) {
                        log_error(checker->log, &arg->loc, "expected integer type as array index, but got '{0:t}'",
                            { .t = arg_type });
                        return type_top(checker->mod);
                    }
                })
                return callee_type->ops[0];
            }
        case TYPE_STRUCT:
            check(checker, ast->data.call.arg, type_members(checker->mod, callee_type));
            return callee_type;
        case TYPE_FN:
            check(checker, ast->data.call.arg, callee_type->ops[0]);
            return callee_type->ops[1];
        default:
            if (callee_type->tag != TYPE_TOP)
                log_error(checker->log, &ast->loc, "function, array, or structure type expected in call expression, but got '{0:t}'", { .t = callee_type });
            return type_top(checker->mod);
    }
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
                    .members = NULL,
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
                    bool name = arg->tag == AST_FIELD && arg->data.field.name;
                    size_t index = name ? arg->data.field.index : nops++;
                    type_ops[index] = infer(checker, name ? arg->data.field.arg : arg);
                })
                const type_t* type = type_tuple(checker->mod, nops, type_ops);
                TMP_BUF_FREE(type_ops)
                return type;
            }
        case AST_ARRAY:
            if (!ast->data.array.elems) {
                log_error(checker->log, &ast->loc, "cannot infer type for empty array");
                return type_top(checker->mod);
            } else {
                const type_t* elem_type = infer(checker, ast->data.array.elems->ast);
                if (!ast->data.array.dim) {
                    FORALL_AST(ast->data.array.elems->next, elem, {
                        elem_type = check(checker, elem, elem_type);
                    });
                }
                if (ast->data.array.regular) {
                    if (ast->data.array.dim) {
                        ast_t* dim = ast->data.array.elems->next->ast;
                        return type_array(checker->mod, dim->data.lit.value.ival, elem_type);
                    }
                    if (elem_type->tag == TYPE_ARRAY)
                        return type_array(checker->mod, elem_type->data.dim + 1, elem_type->ops[0]);
                    log_error(checker->log, &ast->loc, "array type expected in regular array expression, but got '{0:t}'", { .t = elem_type });
                    return type_top(checker->mod);
                }
                return type_array(checker->mod, 1, elem_type);
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
            return infer_call(checker, ast);
        case AST_ANNOT:
            {
                const type_t* type = infer(checker, ast->data.annot.type);
                check(checker, ast->data.annot.ast, type);
                return type;
            }
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
                case LIT_STR:  return type_array(checker->mod, 1, type_u8(checker->mod));
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
                assert(ast->data.fn.lambda);
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
                    bool name = arg->tag == AST_FIELD && arg->data.field.name;
                    size_t index = name ? arg->data.field.index : nops++;
                    type_ops[index] = check(checker, name ? arg->data.field.arg : arg, expected->ops[index]);
                });
                const type_t* type = type_tuple(checker->mod, nops, type_ops);
                TMP_BUF_FREE(type_ops)
                return type;
            }
        case AST_ARRAY:
            {
                assert(!ast->data.array.dim);
                if (expected->tag != TYPE_ARRAY)
                    return expect(checker, ast, "array", NULL, expected);
                if (ast->data.array.regular) {
                    if (expected->data.dim <= 1) {
                        log_error(checker->log, &ast->loc,
                            "expected array of type '{0:t}' with one dimension, but got regular array with more than one dimension",
                            { .t = expected });
                        return expected;
                    }
                    const type_t* array_type = type_array(checker->mod, expected->data.dim - 1, expected->ops[0]);
                    FORALL_AST(ast->data.array.elems, elem, {
                        array_type = check(checker, elem, array_type);
                    })
                    return type_array(checker->mod, array_type->data.dim + 1, array_type->ops[0]);
                } else {
                    if (expected->data.dim != 1) {
                        log_error(checker->log, &ast->loc,
                            "expected array of type '{0:t}' with {1:u32} dimensions, but got array with only one dimension",
                            { .t = expected },
                            { .u32 = expected->data.dim });
                        return expected;
                    }
                    const type_t* elem_type = expected->ops[0];
                    FORALL_AST(ast->data.array.elems, elem, {
                        elem_type = check(checker, elem, expected->ops[0]);
                    })
                    return type_array(checker->mod, 1, elem_type);
                }
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
                    return expect(checker, ast, "string literal", type_array(checker->mod, 1, type_u8(checker->mod)), expected);
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
