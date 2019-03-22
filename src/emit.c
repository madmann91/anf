#include "emit.h"

#define SAVE_STATE(emitter, ...) \
    { \
        emitter_state_t state = emitter->state; \
        { \
            __VA_ARGS__ \
        } \
        emitter->state = state; \
    }

static inline const dbg_t* make_dbg(emitter_t* emitter, const char* name, loc_t loc) {
    dbg_t* dbg = mpool_alloc(&emitter->mod->pool, sizeof(dbg_t));
    dbg->file = emitter->file;
    dbg->name = name;
    dbg->loc  = loc;
    return dbg;
}

static inline const type_t* continuation_type(emitter_t* emitter, const type_t* from, const type_t* to) {
    const type_t* ret   = type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), to);
    const type_t* param = type_tuple_from_args(emitter->mod, 3, type_mem(emitter->mod), from, type_cn(emitter->mod, ret));
    return type_cn(emitter->mod, param);
}

static inline const type_t* continuation_return_type(const type_t* type) {
    assert(type_is_cn(type));
    const type_t* from = type->ops[0];
    assert(from->tag == TYPE_TUPLE && from->nops == 3);
    const type_t* ret = from->ops[2];
    assert(type_is_cn(ret));
    const type_t* ret_from = ret->ops[0];
    assert(ret_from->tag == TYPE_TUPLE && ret_from->nops == 2);
    return ret_from->ops[1];
}

static const type_t* convert(emitter_t* emitter, const type_t* type) {
    // Perform CPS-conversion: Functions with type T => U are converted to (mem, T, (mem, U) => bottom) => bottom
    // This process is recursive and also affects aggregate members.
    const type_t** found = type2type_lookup(emitter->types, type);
    if (found)
        return *found;

    const type_t* converted = type;
    if (type->tag == TYPE_FN) {
        converted = continuation_type(emitter, convert(emitter, type->ops[0]), convert(emitter, type->ops[1]));
    } else if (type->tag == TYPE_STRUCT) {
        type2type_insert(emitter->types, type, type);
        type->data.struct_def->type = convert(emitter, type->data.struct_def->type);
    } else if (type->nops != 0) {
        TMP_BUF_ALLOC(type_ops, const type_t*, type->nops)
        for (size_t i = 0; i < type->nops; ++i)
            type_ops[i] = convert(emitter, type->ops[i]);
        converted = type_rebuild(emitter->mod, type, type_ops);
        TMP_BUF_FREE(type_ops)
    }
    type2type_insert(emitter->types, type, converted);
    return converted;
}

static inline const node_t* enter_bb(emitter_t* emitter, const node_t* bb, bool tuple) {
    const node_t* param = node_param(emitter->mod, bb, NULL);
    emitter->state.cur = bb;
    if (tuple) {
        emitter->state.mem = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), make_dbg(emitter, "mem", bb->dbg->loc));
        return node_extract(emitter->mod, param, node_i32(emitter->mod, 1), make_dbg(emitter, "res", bb->dbg->loc));
    } else {
        emitter->state.mem = param;
        return NULL;
    }
}

static inline const node_t* enter_fn(emitter_t* emitter, const node_t* fn) {
    const node_t* param = node_param(emitter->mod, fn, NULL);
    assert(param->type->tag == TYPE_TUPLE);
    assert(param->type->nops == 3);
    emitter->state.cur = fn;
    emitter->state.mem = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), make_dbg(emitter, "mem", fn->dbg->loc));
    emitter->state.ret = node_extract(emitter->mod, param, node_i32(emitter->mod, 2), make_dbg(emitter, "ret", fn->dbg->loc));
    return node_extract(emitter->mod, param, node_i32(emitter->mod, 1), make_dbg(emitter, NULL, fn->dbg->loc));
}

static inline void jump_from(emitter_t* emitter, const node_t* from, const node_t* fn, const node_t* arg, const dbg_t* dbg) {
    assert(from);
    bool always_inline = fn->tag == NODE_SELECT;
    node_bind(emitter->mod, from, 0, node_app(emitter->mod, fn, arg, node_bool(emitter->mod, always_inline), dbg));
}

static inline void jump(emitter_t* emitter, const node_t* fn, const node_t* arg, const dbg_t* dbg) {
    if (emitter->state.cur)
        jump_from(emitter, emitter->state.cur, fn, arg, dbg);
}

static inline void branch(emitter_t* emitter, const node_t* cond, const node_t* if_true, const node_t* if_false, const node_t* mem, const dbg_t* dbg) {
    const node_t* select = node_select(emitter->mod, cond, if_true, if_false, dbg);
    jump(emitter, select, mem, dbg);
}

static inline void return_(emitter_t* emitter, const node_t* value, const dbg_t* dbg) {
    jump(emitter, emitter->state.ret, node_tuple_from_args(emitter->mod, 2, NULL, emitter->state.mem, value), dbg);
}

static inline void always_inline(emitter_t* emitter, const node_t* fn) {
    assert(fn->tag == NODE_FN);
    node_bind(emitter->mod, fn, 1, node_bool(emitter->mod, true));
}

static void emit_ptrn(emitter_t* emitter, ast_t* ast, const node_t* node, bool var) {
    assert(!ast->node);
    if (ast->type)
        ast->type = convert(emitter, ast->type);
    switch (ast->tag) {
        case AST_TUPLE:
            // Tuples with one element are not deconstructing patterns
            if (ast_list_is_single(ast->data.tuple.args))
                emit_ptrn(emitter, ast->data.tuple.args->ast, node, var);
            else {
                size_t nops = 0;
                FORALL_AST(ast->data.tuple.args, arg, {
                    const node_t* arg_node = node_extract(emitter->mod, node, node_i32(emitter->mod, nops++), NULL);
                    emit_ptrn(emitter, arg, arg_node, var);
                })
            }
            break;
        case AST_ID:
            if (var) {
                const node_t* alloc = node_alloc(emitter->mod, emitter->state.mem, node->type, NULL);
                emitter->state.mem = node_extract(emitter->mod, alloc, node_i32(emitter->mod, 0), NULL);
                ast->node = node_extract(emitter->mod, alloc, node_i32(emitter->mod, 1), NULL);
                emitter->state.mem = node_store(emitter->mod, emitter->state.mem, ast->node, node, NULL);
            } else {
                ast->node = node;
            }
            if (!ast->node->dbg)
                ((node_t*)ast->node)->dbg = make_dbg(emitter, ast->data.id.str, ast->loc);
            break;
        case AST_ANNOT:
            emit_ptrn(emitter, ast->data.annot.arg, node, var);
            break;
        default:
            assert(false);
            break;
    }
}

static inline const node_t* emit_fn(emitter_t* emitter, const type_t* fn_type, ast_t* ast_param, const char* name, loc_t loc) {
    const node_t* fn = node_fn(emitter->mod, fn_type, 0, make_dbg(emitter, name, loc));
    const node_t* param = enter_fn(emitter, fn);
    emit_ptrn(emitter, ast_param, param, false);
    return fn;
}

static const node_t* emit_curried_fn(emitter_t* emitter, const type_t* fn_type, ast_list_t* params, const char* name, loc_t loc) {
    const node_t* fn = emit_fn(emitter, fn_type, params->ast, name, loc);
    if (params->next) {
        const type_t* child_type = continuation_return_type(fn_type);
        const node_t* ret = emitter->state.ret;
        const node_t* mem = emitter->state.mem;
        const node_t* child = emit_curried_fn(emitter, child_type, params->next, name, loc);
        jump_from(emitter, fn, ret, node_tuple_from_args(emitter->mod, 2, NULL, mem, child), make_dbg(emitter, NULL, loc));
    }
    return fn;
}

static const node_t* emit_call(emitter_t* emitter, const node_t* callee, const node_t* arg, const node_t** cont_ptr, loc_t loc) {
    if (type_is_cn(callee->type)) {
        const type_t* from = callee->type->ops[0];
        assert(from->tag == TYPE_TUPLE);
        assert(from->nops == 2 || from->nops == 3);
        if (from->nops == 2) {
            // Call to a continuation (like break, continue, or return)
            jump(emitter, callee, node_tuple_from_args(emitter->mod, 2, NULL, emitter->state.mem, arg), make_dbg(emitter, NULL, loc));
            emitter->state.cur = NULL;
            return node_bottom(emitter->mod, type_bottom(emitter->mod));
        } else {
            // Standard call
            const node_t* cont = node_fn(emitter->mod, from->ops[2], 0, make_dbg(emitter, "call_cont", loc));
            jump(emitter, callee, node_tuple_from_args(emitter->mod, 3, NULL, emitter->state.mem, arg, cont), make_dbg(emitter, NULL, loc));
            const node_t* res = enter_bb(emitter, cont, true);
            if (cont_ptr)
                *cont_ptr = cont;
            return res;
        }
    } else {
        assert(callee->type->tag == TYPE_ARRAY);
        return node_extract(emitter->mod, callee, arg, make_dbg(emitter, NULL, loc));
    }
}

static const node_t* emit_internal(emitter_t* emitter, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROG:
            FORALL_AST(ast->data.prog.mods, mod, { emit(emitter, mod); })
            return NULL;
        case AST_MOD:
            emitter->mod = ast->data.mod.mod;
            FORALL_AST(ast->data.mod.decls, decl, { emit(emitter, decl); })
            emitter->mod = NULL;
            return NULL;
        case AST_STRUCT:
            {
                assert(ast->type->tag == TYPE_STRUCT);
                const type_t* constr_type = continuation_type(emitter, ast->type->data.struct_def->type, ast->type);
                const node_t* constr = node_fn(emitter->mod, constr_type, 0, make_dbg(emitter, ast->data.struct_.id->data.id.str, ast->loc));
                always_inline(emitter, constr);
                const node_t* arg = enter_fn(emitter, constr);
                return_(emitter, node_struct(emitter->mod, arg, ast->type, constr->dbg), make_dbg(emitter, NULL, ast->loc));
                return constr;
            }
        case AST_ID:
            {
                assert(ast->data.id.to);
                if (!ast->data.id.to->node) {
                    // If the identifier is referring to a function, we need to save the current state
                    SAVE_STATE(emitter, {
                        emit(emitter, (ast_t*)ast->data.id.to);
                    })
                }
                const node_t* node = ast->data.id.to->node;
                if (node->type->tag == TYPE_PTR) {
                    const node_t* load = node_load(emitter->mod, emitter->state.mem, node, NULL);
                    emitter->state.mem = node_extract(emitter->mod, load, node_i32(emitter->mod, 0), NULL);
                    return node_extract(emitter->mod, load, node_i32(emitter->mod, 1), make_dbg(emitter, ast->data.id.str, ast->loc));
                }
                return node;
            }
        case AST_TUPLE:
            {
                size_t nargs = ast_list_length(ast->data.tuple.args);
                TMP_BUF_ALLOC(node_ops, const node_t*, nargs)
                size_t nops = 0;
                FORALL_AST(ast->data.tuple.args, arg, {
                    bool name = arg->tag == AST_FIELD && arg->data.field.name;
                    size_t index = name ? arg->data.field.index : nops++;
                    node_ops[index] = emit(emitter, name ? arg->data.field.arg : arg);
                })
                const node_t* node = node_tuple(emitter->mod, nargs, node_ops, make_dbg(emitter, NULL, ast->loc));
                TMP_BUF_FREE(node_ops)
                return node;
            }
        case AST_ARRAY:
            {
                const type_t* elem_type = ast->type->ops[0];
                size_t nelems = ast_list_length(ast->data.array.elems);
                TMP_BUF_ALLOC(node_ops, const node_t*, nelems)
                size_t nops = 0;
                FORALL_AST(ast->data.array.elems, elem, {
                    node_ops[nops++] = emit(emitter, elem);
                })
                const node_t* node = node_array(emitter->mod, nelems, node_ops, elem_type, make_dbg(emitter, NULL, ast->loc));
                TMP_BUF_FREE(node_ops)
                return node;
            }
        case AST_FIELD:
            return node_extract(emitter->mod, emit(emitter, ast->data.field.arg), node_i32(emitter->mod, ast->data.field.index), make_dbg(emitter, NULL, ast->loc));
        case AST_CALL:
            {
                const node_t* callee = emit(emitter, ast->data.call.callee);
                FORALL_AST(ast->data.call.args, arg, {
                    const node_t* arg_node = emit(emitter, arg);
                    callee = emit_call(emitter, callee, arg_node, NULL, ast->loc);
                })
                return callee;
            }
        case AST_ANNOT:
            return emit(emitter, ast->data.annot.arg);
        case AST_BLOCK:
            {
                const node_t* last = node_unit(emitter->mod);
                FORALL_AST(ast->data.block.stmts, stmt, {
                    last = emit(emitter, stmt);
                })
                return last;
            }
        case AST_FN:
            {
                const node_t* fn = NULL;
                SAVE_STATE(emitter, {
                    fn = emit_fn(emitter, ast->type, ast->data.fn.param, NULL, ast->loc);
                    const node_t* value = emit(emitter, ast->data.fn.body);
                    return_(emitter, value, make_dbg(emitter, NULL, ast->data.fn.body->loc));
                })
                return fn;
            }
        case AST_IF:
            {
                const type_t* branch_type = type_cn(emitter->mod, type_mem(emitter->mod));
                const type_t* join_type   = type_cn(emitter->mod, type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), ast->type));
                const node_t* if_true  = node_fn(emitter->mod, branch_type, 0, make_dbg(emitter, "if_true", ast->data.if_.if_true->loc));
                const node_t* if_false = node_fn(emitter->mod, branch_type, 0, make_dbg(emitter, "if_false", ast->data.if_.if_false ? ast->data.if_.if_false->loc : ast->loc));
                const node_t* if_join  = node_fn(emitter->mod, join_type, 0, make_dbg(emitter, "if_join", ast->loc));

                // Enter the conditional
                const node_t* cond = emit(emitter, ast->data.if_.cond);
                branch(emitter, cond, if_true, if_false, emitter->state.mem, make_dbg(emitter, NULL, ast->data.if_.cond->loc));

                // Emit true branch
                enter_bb(emitter, if_true, false);
                const node_t* true_val = emit(emitter, ast->data.if_.if_true);
                jump(emitter, if_join, node_tuple_from_args(emitter->mod, 2, NULL, emitter->state.mem, true_val), NULL);

                // Emit false branch
                enter_bb(emitter, if_false, false);
                const node_t* false_val = ast->data.if_.if_false ? emit(emitter, ast->data.if_.if_false) : node_unit(emitter->mod);
                jump(emitter, if_join, node_tuple_from_args(emitter->mod, 2, NULL, emitter->state.mem, false_val), NULL);

                // Emit join block
                return enter_bb(emitter, if_join, true);
            }
        case AST_WHILE:
            {
                const type_t* bb_type    = type_cn(emitter->mod, type_mem(emitter->mod));
                const type_t* cont_type  = type_cn(emitter->mod, type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), type_unit(emitter->mod)));
                const node_t* while_head = node_fn(emitter->mod, bb_type,   0, make_dbg(emitter, "while_head",     ast->data.while_.cond->loc));
                const node_t* while_exit = node_fn(emitter->mod, bb_type,   0, make_dbg(emitter, "while_exit",     ast->loc));
                const node_t* while_body = node_fn(emitter->mod, bb_type,   0, make_dbg(emitter, "while_body",     ast->data.while_.body->loc));
                const node_t* while_join = node_fn(emitter->mod, bb_type,   0, make_dbg(emitter, "while_join",     ast->loc));
                const node_t* while_brk  = node_fn(emitter->mod, cont_type, 0, make_dbg(emitter, "while_break",    ast->loc));
                const node_t* while_cnt  = node_fn(emitter->mod, cont_type, 0, make_dbg(emitter, "while_continue", ast->loc));

                // Enter the loop
                jump(emitter, while_head, emitter->state.mem, NULL);

                // Link break/continue to join/head
                enter_bb(emitter, while_brk, true);
                jump(emitter, while_join, emitter->state.mem, while_brk->dbg);
                enter_bb(emitter, while_cnt, true);
                jump(emitter, while_head, emitter->state.mem, while_cnt->dbg);
                always_inline(emitter, while_brk);
                always_inline(emitter, while_cnt);

                // Emit while head
                enter_bb(emitter, while_head, false);
                const node_t* cond = emit(emitter, ast->data.while_.cond);
                branch(emitter, cond, while_body, while_exit, emitter->state.mem, make_dbg(emitter, NULL, ast->data.while_.cond->loc));

                // Emit while_exit
                enter_bb(emitter, while_exit, false);
                jump(emitter, while_join, emitter->state.mem, NULL);

                // Emit while_body
                enter_bb(emitter, while_body, false);
                SAVE_STATE(emitter, {
                    emitter->state.brk = while_brk;
                    emitter->state.cnt = while_cnt;
                    emit(emitter, ast->data.while_.body);
                    jump(emitter, while_head, emitter->state.mem, NULL);
                })

                // Emit while_join
                enter_bb(emitter, while_join, false);
                return node_unit(emitter->mod);
            }
        case AST_FOR:
            {
                const node_t* callee = emit(emitter, ast->data.for_.call->data.call.callee);
                ast_list_t* args = ast->data.for_.call->data.call.args;
                ast_t* fn = args->ast->data.tuple.args->ast;

                // Emit a function for the loop body
                SAVE_STATE(emitter, {
                    fn->type = convert(emitter, fn->type);
                    fn->node = emit_fn(emitter, fn->type, fn->data.fn.param, "for_body", fn->loc);
                })

                // Emit call to iteration function
                const node_t* for_brk = NULL;
                FORALL_AST(args, arg, {
                    const node_t* arg_node = emit(emitter, arg);
                    callee = emit_call(emitter, callee, arg_node, &for_brk, ast->loc);
                })

                // Emit the body of the function
                const node_t* for_cnt = NULL;
                SAVE_STATE(emitter, {
                    const node_t* ret = emitter->state.ret;
                    enter_fn(emitter, fn->node);
                    for_cnt = emitter->state.ret;
                    emitter->state.ret = ret;
                    emitter->state.brk = for_brk;
                    emitter->state.cnt = for_cnt;
                    const node_t* value = emit(emitter, fn->data.fn.body);
                    return_(emitter, value, make_dbg(emitter, NULL, fn->data.fn.body->loc));
                })

                // Set sebug information on break and continue
                ((node_t*)for_brk)->dbg = make_dbg(emitter, "for_break",    ast->loc);
                ((node_t*)for_cnt)->dbg = make_dbg(emitter, "for_continue", ast->loc);

                return callee;
            }
        case AST_VAL:
        case AST_VAR:
            emit_ptrn(emitter, ast->data.varl.ptrn, emit(emitter, ast->data.varl.value), ast->tag == AST_VAR);
            return node_unit(emitter->mod);
        case AST_DEF:
            {
                SAVE_STATE(emitter, {
                    ast->node = emit_curried_fn(emitter, ast->type, ast->data.def.params, ast->data.def.id->data.id.str, ast->loc);
                    const node_t* value = emit(emitter, ast->data.def.value);
                    return_(emitter, value, make_dbg(emitter, NULL, ast->data.def.value->loc));
                })
                return ast->node;
            }
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_INT:
                case LIT_FLT:
                case LIT_BOOL:
                    switch (ast->type->tag) {
                        case TYPE_I8:   return node_i8(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_I16:  return node_i16(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_I32:  return node_i32(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_I64:  return node_i64(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_U8:   return node_u8(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_U16:  return node_u16(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_U32:  return node_u32(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_F32:  return node_f32(emitter->mod, ast->data.lit.value.fval, ast->type->data.fp_flags);
                        case TYPE_BOOL: return node_bool(emitter->mod, ast->data.lit.value.bval);
                        case TYPE_U64:  return node_u64(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_F64:  return node_f64(emitter->mod, ast->data.lit.value.fval, ast->type->data.fp_flags);
                        default:
                            assert(false);
                            return NULL;
                    }
                case LIT_CHR:
                    return node_u8(emitter->mod, ast->data.lit.str[0]);
                case LIT_STR:
                    return node_string(emitter->mod, ast->data.lit.str, make_dbg(emitter, NULL, ast->loc));
                default:
                    assert(false);
                    return NULL;
            }
        case AST_CONT:
            switch (ast->data.cont.tag) {
                case CONT_BREAK:    return emitter->state.brk;
                case CONT_CONTINUE: return emitter->state.cnt;
                case CONT_RETURN:   return emitter->state.ret;
                default:
                    assert(false);
                    return NULL;
            }
        default:
            assert(false);
            return NULL;
    }
}

const node_t* emit(emitter_t* emitter, ast_t* ast) {
    if (ast->node)
        return ast->node;
    if (ast->type)
        ast->type = convert(emitter, ast->type);
    return ast->node = emit_internal(emitter, ast);
}
