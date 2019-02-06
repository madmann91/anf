#include "check.h"

static inline uint32_t default_fp_flags() {
    return FP_RELAXED_MATH;
}

static inline const type_t* expect(checker_t* checker, ast_t* ast, const char* msg, const type_t* type, const type_t* expected) {
    assert(expected);
    if (type == expected || expected->tag == TYPE_TOP || (type && type->tag == TYPE_BOTTOM))
        return type;
    // When a type contains top, this is an error that has already been logged before
    if (type && type_contains(type, type_top(checker->mod)))
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

static inline bool expect_args(checker_t* checker, ast_t* ast, const char* msg, size_t nargs, size_t nparams) {
    if (nargs != nparams) {
        log_error(checker->log, &ast->loc, "expected {0:u32} argument{1:s} in {2:s}, but got {3:u32}",
            { .u32 = nparams },
            { .s   = nparams >= 2 ? "s" : "" },
            { .s   = msg },
            { .u32 = nargs });
        return false;
    }
    return true;
}

static const type_t* curried_type(checker_t* checker, ast_list_t* params, const type_t* ret_type) {
    if (params) {
        assert(params->ast->type);
        return type_fn(checker->mod, params->ast->type, curried_type(checker, params->next, ret_type));
    }
    return ret_type;
}

static inline const type_t* find_member_or_param(ast_list_t* params, const char* name, size_t* index) {
    size_t i = 0;
    FORALL_AST(params, param, {
        assert(param->tag == AST_ANNOT);
        assert(param->data.annot.arg->tag == AST_ID);
        if (!strcmp(name, param->data.annot.arg->data.id.str)) {
            *index = i;
            return param->type;
        }
        i++;
    })
    return NULL;
}

static inline void invalid_member_or_param(checker_t* checker, ast_t* ast, const char* name, const char* member_name, bool is_struct) {
    log_error(checker->log, &ast->loc, is_struct
        ? "structure '{0:s}' has no member named '{1:s}'"
        : "function '{0:s}' has no parameter named '{1:s}'",
        { .s = name },
        { .s = member_name });
}

static inline void unreachable_code(checker_t* checker, ast_t* ast) {
    log_error(checker->log, &ast->loc, "unreachable code after this statement");
}

static const type_t* infer_ptrn(checker_t* checker, ast_t* ptrn, ast_t* value) {
    switch (ptrn->tag) {
        case AST_TUPLE:
            {
                size_t nargs = ast_list_length(ptrn->data.tuple.args);
                if (value->tag != AST_TUPLE || nargs != ast_list_length(value->data.tuple.args))
                    break;
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
        case AST_ANNOT:
            {
                const type_t* type = infer(checker, ptrn->data.annot.type);
                check(checker, ptrn->data.annot.arg, type);
                return check(checker, value, type);
            }
        default:
            break;
    }
    return check(checker, ptrn, infer(checker, value));
}

static bool infer_names(checker_t* checker, const char* callee_name, ast_list_t* args, ast_list_t* params, bool is_struct) {
    size_t nparams = ast_list_length(params);
    TMP_BUF_ALLOC(param_seen, ast_t*, nparams)
    for (size_t i = 0; i < nparams; ++i)
        param_seen[i] = NULL;

    // Mark every positional argument as seen
    size_t param_index = 0;
    ast_list_t* cur_param = params;
    ast_list_t* cur_arg   = args;
    while (cur_arg && cur_param) {
        ast_t* arg = cur_arg->ast;
        if (arg->tag == AST_FIELD && arg->data.field.name)
            break;
        param_seen[param_index] = arg;
        param_index++;
        cur_param = cur_param->next;
        cur_arg   = cur_arg->next;
    }

    // For every named argument, find the index into the tuple
    bool status = true;
    FORALL_AST(cur_arg, name, {
        const char* param_name = name->data.field.id->data.id.str;
        if (!find_member_or_param(params, param_name, &param_index)) {
            invalid_member_or_param(checker, name, callee_name, param_name, is_struct);
            status = false;
        } else if (param_seen[param_index]) {
            log_error(checker->log, &name->loc, "{0:s} '{1:s}' has already been specified",
                { .s = is_struct ? "member" : "parameter" },
                { .s = param_name });
            log_note(checker->log, &param_seen[param_index]->loc, "previous specification was here");
            status = false;
        } else {
            param_seen[param_index] = name;
            name->data.field.index = param_index;
        }
    })

    TMP_BUF_FREE(param_seen)
    return status;
}

static bool infer_names_from_call(checker_t* checker, ast_t* callee, ast_list_t* args) {
    bool any_named = false;
    FORALL_AST(args, arg, { any_named |= arg->data.tuple.named; })
    if (!any_named)
        return true;

    ast_list_t* cur_arg = args;
    if (callee->tag == AST_ID) {
        const ast_t* to = callee->data.id.to;
        const char* callee_name = callee->data.id.str;
        ast_list_t* cur_param = NULL;
        switch (to->tag) {
            case AST_DEF:
                cur_param = to->data.def.params;
                while (cur_arg && cur_param) {
                    if (cur_arg->ast->data.tuple.named &&
                        !infer_names(checker, callee_name, cur_arg->ast->data.tuple.args, cur_param->ast->data.tuple.args, false))
                        return false;
                    cur_arg = cur_arg->next;
                    cur_param = cur_param->next;
                }
                break;
            case AST_STRUCT:
                if (cur_arg->ast->data.tuple.named &&
                    !infer_names(checker, callee_name, cur_arg->ast->data.tuple.args, to->data.struct_.members->data.tuple.args, true))
                    return false;
                cur_arg = cur_arg->next;
                break;
            default:
                goto error;
        }
        FORALL_AST(cur_arg, arg, {
            if (arg->data.tuple.named)
                goto error;
        })
        return true;
    }

error:
    log_error(checker->log, &cur_arg->ast->loc, "named arguments are only allowed for to top-level functions or structures");
    return false;
}

static inline const type_t* check_tuple(checker_t* checker, ast_t* ast, const char* msg, const type_t* expected) {
    assert(ast->tag == AST_TUPLE);
    size_t nargs = ast_list_length(ast->data.tuple.args);
    // Named tuples must have the same number of arguments as the expected type
    if (nargs == 1 && (!ast->data.tuple.named || expected->tag != TYPE_TUPLE))
        return ast->type = check(checker, ast->data.tuple.args->ast, expected);
    if (!expect_args(checker, ast, msg, nargs, type_member_count(expected)))
        return ast->type = expected;
    size_t nops = 0;
    TMP_BUF_ALLOC(type_ops, const type_t*, nargs)
    FORALL_AST(ast->data.tuple.args, arg, {
        bool name = arg->tag == AST_FIELD && arg->data.field.name;
        size_t index = name ? arg->data.field.index : nops++;
        type_ops[index] = check(checker, name ? arg->data.field.arg : arg, expected->ops[index]);
    });
    ast->type = type_tuple(checker->mod, nops, type_ops);
    TMP_BUF_FREE(type_ops)
    return ast->type;
}

static const type_t* check_index(checker_t* checker, ast_t* index, const type_t* array_type) {
    const type_t* index_type = infer(checker, index);
    if (!type_is_i(index_type) && !type_is_u(index_type)) {
        if (index_type->tag != TYPE_TOP)
            log_error(checker->log, &index->loc, "expected integer type as array index, but got '{0:t}'", { .t = index_type });
        return type_top(checker->mod);
    }
    return array_type->ops[0];
}

static const type_t* infer_call(checker_t* checker, ast_t* arg, const type_t* callee_type) {
    switch (callee_type->tag) {
        case TYPE_ARRAY:
            return check_index(checker, arg, callee_type);
        case TYPE_STRUCT:
            check_tuple(checker, arg, "structure", type_tuple_from_struct(checker->mod, callee_type));
            return callee_type;
        case TYPE_FN:
            check_tuple(checker, arg, "function call", callee_type->ops[0]);
            return callee_type->ops[1];
        default:
            if (callee_type->tag != TYPE_TOP)
                log_error(checker->log, &arg->loc, "function, array, or structure type expected in call expression, but got '{0:t}'", { .t = callee_type });
            return type_top(checker->mod);
    }
}

static const type_t* infer_internal(checker_t* checker, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROG:
            FORALL_AST(ast->data.prog.mods, mod, { infer(checker, mod); })
            return NULL;
        case AST_MOD:
            ast->data.mod.mod = mod_create();
            checker->mod = ast->data.mod.mod;
            FORALL_AST(ast->data.mod.decls, decl, { infer(checker, decl); })
            checker->mod = NULL;
            return NULL;
        case AST_STRUCT:
            {
                const char* name = ast->data.struct_.id->data.id.str;
                struct_def_t* struct_def = mpool_alloc(&checker->mod->pool, sizeof(struct_def_t));
                struct_def->byref = ast->data.struct_.byref;
                struct_def->name = mpool_alloc(&checker->mod->pool, strlen(name) + 1);
                strcpy((char*)struct_def->name, name);
                size_t nmembers = ast_list_length(ast->data.struct_.members->data.tuple.args);
                struct_def->members = mpool_alloc(&checker->mod->pool, sizeof(const char**) * nmembers);
                nmembers = 0;
                FORALL_AST(ast->data.struct_.members->data.tuple.args, arg, {
                    const char* member = arg->data.annot.arg->data.id.str;
                    struct_def->members[nmembers] = mpool_alloc(&checker->mod->pool, strlen(member));
                    strcpy((char*)struct_def->members[nmembers], member);
                    nmembers++;
                })
                ast->type = type_struct(checker->mod, struct_def, 0, NULL);
                struct_def->type = infer(checker, ast->data.struct_.members);
                return ast->type;
            }
        case AST_ID:
            if (ast->data.id.to) {
                // This identifier is refering to some other, previously declared identifier
                ast_t* to = (ast_t*)ast->data.id.to;
                return to->type ? to->type : infer(checker, to);
            }
            log_error(checker->log, &ast->loc, "cannot infer type for identifier '{0:s}'", { .s = ast->data.id.str });
            return type_top(checker->mod);
        case AST_PRIM:
            switch (ast->data.prim.tag) {
                case TYPE_F32:
                case TYPE_F64:
                    // TODO: Use proper FP flags
                    return type_prim_fp(checker->mod, ast->data.prim.tag, default_fp_flags());
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
                FORALL_AST(ast->data.array.elems->next, elem, {
                    check(checker, elem, elem_type);
                });
                return type_array(checker->mod, elem_type);
            }
        case AST_FIELD:
            if (ast->data.field.name) {
                return infer(checker, ast->data.field.arg);
            } else {
                const type_t* struct_type = infer(checker, ast->data.field.arg);
                if (struct_type->tag != TYPE_STRUCT) {
                    if (struct_type->tag != TYPE_TOP)
                        log_error(checker->log, &ast->loc, "structure type expected in field expression, but got '{0:t}'", { .t = struct_type });
                    return type_top(checker->mod);
                }
                const char* member_name = ast->data.field.id->data.id.str;
                ast->data.field.index = type_find_member(struct_type, member_name);
                if (ast->data.field.index == INVALID_INDEX) {
                    invalid_member_or_param(checker, ast, struct_type->data.struct_def->name, member_name, true);
                    return type_top(checker->mod);
                }
                return type_member(checker->mod, struct_type, ast->data.field.index);
            }
        case AST_CALL:
            {
                if (!infer_names_from_call(checker, ast->data.call.callee, ast->data.call.args))
                    return type_top(checker->mod);
                const type_t* callee_type = infer(checker, ast->data.call.callee);
                FORALL_AST(ast->data.call.args, arg, {
                    callee_type = infer_call(checker, arg, callee_type);
                })
                return callee_type;
            }
        case AST_ANNOT:
            {
                const type_t* type = infer(checker, ast->data.annot.type);
                check(checker, ast->data.annot.arg, type);
                return type;
            }
        case AST_VAR:
        case AST_VAL:
            infer_ptrn(checker, ast->data.varl.ptrn, ast->data.varl.value);
            return type_unit(checker->mod);
        case AST_DEF:
            {
                FORALL_AST(ast->data.def.params, param, {
                    infer(checker, param);
                })
                const type_t* ret_type = NULL;
                if (ast->data.def.ret) {
                    // Set the type immediately to allow checking recursive calls
                    ret_type = infer(checker, ast->data.def.ret);
                    ast->type = curried_type(checker, ast->data.def.params, ret_type);
                    check(checker, ast->data.def.value, ret_type);
                } else if (ast_set_insert(checker->defs, ast)) {
                    ret_type = infer(checker, ast->data.def.value);
                    ast->type = curried_type(checker, ast->data.def.params, ret_type);
                    ast_set_remove(checker->defs, ast);
                }
                if (!ret_type) {
                    log_error(checker->log, &ast->loc, "cannot infer return type for recursive function '{0:s}'",
                        { .s = ast->data.def.id->data.id.str });
                    ast->type = type_top(checker->mod);
                }
                return ast->type;
            }
        case AST_BLOCK:
            {
                const type_t* last = type_unit(checker->mod);
                for (ast_list_t* cur = ast->data.block.stmts; cur; cur = cur->next) {
                    last = infer(checker, cur->ast);
                    if (cur->next && last->tag == TYPE_BOTTOM)
                        unreachable_code(checker, cur->ast);
                }
                return last;
            }
        case AST_FN:
            {
                const type_t* param_type = infer(checker, ast->data.fn.param);
                const type_t* body_type  = infer(checker, ast->data.fn.body);
                return type_fn(checker->mod, param_type, body_type);
            }
        case AST_IF:
            {
                check(checker, ast->data.if_.cond, type_bool(checker->mod));
                if (ast->data.if_.if_false) {
                    const type_t* type = infer(checker, ast->data.if_.if_true);
                    return check(checker, ast->data.if_.if_false, type);
                } else {
                    return check(checker, ast->data.if_.if_true, type_unit(checker->mod));
                }
            }
        case AST_MATCH:
            {
                const type_t* arg_type   = infer(checker, ast->data.match.arg);
                const type_t* value_type = NULL;
                FORALL_AST(ast->data.match.cases, case_, {
                    check(checker, case_->data.case_.ptrn, arg_type);
                    if (value_type)
                        check(checker, case_->data.case_.value, value_type);
                    else
                        value_type = infer(checker, case_->data.case_.value);
                })
                return value_type ? value_type : type_unit(checker->mod);
            }
        case AST_WHILE:
            check(checker, ast->data.while_.cond, type_bool(checker->mod));
            check(checker, ast->data.while_.body, type_unit(checker->mod));
            return type_unit(checker->mod);
        case AST_FOR:
            check(checker, ast->data.for_.call, type_unit(checker->mod));
            return type_unit(checker->mod);
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
                switch (ast->data.cont.tag) {
                    case CONT_RETURN:
                        assert(ast->data.cont.parent);
                        const type_t* parent_type = ast->data.cont.parent->type;
                        if (!parent_type) {
                            log_error(checker->log, &ast->loc, "cannot infer type for '{$key}return{$}'");
                            return type_top(checker->mod);
                        }
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
            assert(!ast->data.id.to || ast->data.id.to->type);
            return ast->data.id.to ? expect(checker, ast, "identifier", ast->data.id.to->type, expected) : expected;
        case AST_BLOCK:
            for (ast_list_t* cur = ast->data.block.stmts; cur; cur = cur->next) {
                // Check last element, infer every other one
                if (cur->next) {
                    if (infer(checker, cur->ast)->tag == TYPE_BOTTOM)
                        unreachable_code(checker, cur->ast);
                } else {
                    return check(checker, cur->ast, expected);
                }
            }
            return expect(checker, ast, "block", type_unit(checker->mod), expected);
        case AST_FN:
            {
                assert(ast->data.fn.lambda);
                if (expected->tag != TYPE_FN)
                    return expect(checker, ast, "anonymous function", NULL, expected);
                check(checker, ast->data.fn.param, expected->ops[0]);
                check(checker, ast->data.fn.body, expected->ops[1]);
                return expected;
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
            return check_tuple(checker, ast, "tuple", expected);
        case AST_ARRAY:
            {
                if (expected->tag != TYPE_ARRAY)
                    return expect(checker, ast, "array", NULL, expected);
                const type_t* elem_type = expected->ops[0];
                FORALL_AST(ast->data.array.elems, elem, {
                    check(checker, elem, elem_type);
                })
                return type_array(checker->mod, elem_type);
            }
        case AST_FIELD:
            if (ast->data.field.name)
                return check(checker, ast->data.field.arg, expected);
            return expect(checker, ast, "field expression", infer(checker, ast), expected);
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
                    return NULL;
            }
        default:
            return expect(checker, ast, NULL, infer(checker, ast), expected);
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
