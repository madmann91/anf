#include <float.h>
#include <stdint.h>

#include "emit.h"

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

static const type_t* convert(emitter_t* emitter, const type_t* type) {
    const type_t** found = type2type_lookup(emitter->types, type);
    if (found)
        return *found;

    const type_t* converted = type;
    if (type->tag == TYPE_FN) {
        converted = continuation_type(emitter, convert(emitter, type->ops[0]), convert(emitter, type->ops[1]));
    } else if (type->tag == TYPE_STRUCT) {
        type2type_insert(emitter->types, type, type);
        type->data.struct_def->members = convert(emitter, type->data.struct_def->members);
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

static inline void jump_from(emitter_t* emitter, const node_t* from, const node_t* fn, const node_t* arg, const dbg_t* dbg) {
    bool cond = fn->tag == NODE_SELECT; // Always inline selections
    node_bind(emitter->mod, from, 0, node_app(emitter->mod, fn, arg, node_bool(emitter->mod, cond), dbg));
}

static inline void jump(emitter_t* emitter, const node_t* fn, const node_t* arg, const dbg_t* dbg) {
    assert(emitter->cur);
    jump_from(emitter, emitter->cur, fn, arg, dbg);
}

static inline void return_from(emitter_t* emitter, const node_t* ret, const node_t* value) {
    if (emitter->cur)
        jump(emitter, ret, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, value), ret->dbg);
}

static inline void always_inline(emitter_t* emitter, const node_t* fn) {
    assert(fn->tag == NODE_FN);
    node_bind(emitter->mod, fn, 1, node_bool(emitter->mod, true));
}

static inline const node_t* mem_param(emitter_t* emitter, const node_t* fn) {
    return node_extract(emitter->mod, node_param(emitter->mod, fn, NULL), node_i32(emitter->mod, 0), NULL);
}

static inline void check_int_lit(emitter_t* emitter, ast_t* ast, uint64_t max) {
    assert(ast->tag == AST_LIT && ast->data.lit.tag == LIT_INT);
    if (ast->data.lit.value.ival > max) {
        log_error(emitter->log, &ast->loc, "constant '{0:u64}' cannot be represented with type '{1:t}'",
            { .u64 = ast->data.lit.value.ival },
            { .t = ast->type });
    }
}

static inline void check_flt_lit(emitter_t* emitter, ast_t* ast, double min, double max) {
    assert(ast->tag == AST_LIT && ast->data.lit.tag == LIT_FLT);
    if (ast->data.lit.value.fval > max || ast->data.lit.value.fval < min) {
        log_error(emitter->log, &ast->loc, "constant '{0:f64}' cannot be represented with type '{1:t}'",
            { .u64 = ast->data.lit.value.fval },
            { .t = ast->type });
    }
}

static void emit_ptrn(emitter_t* emitter, ast_t* ast, const node_t* node, bool var) {
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
                const node_t* alloc = node_alloc(emitter->mod, emitter->mem, node->type, NULL);
                emitter->mem = node_extract(emitter->mod, alloc, node_i32(emitter->mod, 0), NULL);
                ast->node = node_extract(emitter->mod, alloc, node_i32(emitter->mod, 1), NULL);
                emitter->mem = node_store(emitter->mod, emitter->mem, ast->node, node, NULL);
            } else {
                ast->node = node;
            }
            if (!ast->node->dbg)
                ((node_t*)ast->node)->dbg = make_dbg(emitter, ast->data.id.str, ast->loc);
            break;
        case AST_ANNOT:
            emit_ptrn(emitter, ast->data.annot.ast, node, var);
            break;
        default:
            assert(false);
            break;
    }
}

static inline const node_t* emit_fn(emitter_t* emitter, const type_t* fn_type, ast_t* ast_param, const char* name, loc_t loc) {
    const node_t* fn = node_fn(emitter->mod, fn_type, 0, make_dbg(emitter, name, loc));
    const node_t* param = node_param(emitter->mod, fn, NULL);
    const node_t* mem  = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), NULL);
    const node_t* ptrn = node_extract(emitter->mod, param, node_i32(emitter->mod, 1), make_dbg(emitter, NULL, ast_param->loc));
    const node_t* ret  = node_extract(emitter->mod, param, node_i32(emitter->mod, 2), NULL);
    emit_ptrn(emitter, ast_param, ptrn, false);
    emitter->cur = fn;
    emitter->return_ = ret;
    emitter->mem = mem;
    return fn;
}

static const node_t* emit_curried_fn(emitter_t* emitter, const type_t* fn_type, ast_list_t* params, const char* name, loc_t loc) {
    const node_t* fn = emit_fn(emitter, fn_type, params->ast, name, loc);
    if (params->next) {
        const type_t* child_type = type_cn_to(fn_type);
        const node_t* ret = emitter->return_;
        const node_t* mem = emitter->mem;
        const node_t* child = emit_curried_fn(emitter, child_type, params->next, name, loc);
        jump_from(emitter, fn, ret, node_tuple_from_args(emitter->mod, 2, NULL, mem, child), make_dbg(emitter, NULL, loc));
    }
    return fn;
}

static const node_t* emit_call(emitter_t* emitter, const node_t* callee, const node_t* arg, const node_t** cont_ptr, loc_t loc) {
    if (type_is_cn(callee->type)) {
        const type_t* from = callee->type->ops[0];
        assert(from->tag == TYPE_TUPLE);
        if (from->nops == 2) {
            // Call to a continuation (like break, continue, or return)
            jump(emitter, callee, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, arg), make_dbg(emitter, NULL, loc));
            emitter->cur = NULL;
            return node_bottom(emitter->mod, type_bottom(emitter->mod));
        } else {
            // Standard call
            const node_t* cont = node_fn(emitter->mod, from->ops[2], 0, make_dbg(emitter, "call_cont", loc));
            jump(emitter, callee, node_tuple_from_args(emitter->mod, 3, NULL, emitter->mem, arg, cont), make_dbg(emitter, NULL, loc));

            const node_t* param = node_param(emitter->mod, cont, NULL);
            emitter->mem = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), NULL);
            emitter->cur = cont;
            if (cont_ptr)
                *cont_ptr = cont;
            return node_extract(emitter->mod, param, node_i32(emitter->mod, 1), NULL);
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
                const type_t* constr_type = continuation_type(emitter, ast->type->data.struct_def->members, ast->type);
                const node_t* constr = node_fn(emitter->mod, constr_type, 0, make_dbg(emitter, ast->data.struct_.id->data.id.str, ast->loc));
                const node_t* param = node_param(emitter->mod, constr, NULL);
                const node_t* mem = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), NULL);
                const node_t* arg = node_extract(emitter->mod, param, node_i32(emitter->mod, 1), make_dbg(emitter, NULL, ast->data.struct_.members->loc));
                const node_t* ret = node_extract(emitter->mod, param, node_i32(emitter->mod, 2), NULL);
                always_inline(emitter, constr);
                jump_from(emitter, constr, ret, node_tuple_from_args(emitter->mod, 2, NULL, mem, node_struct(emitter->mod, arg, ast->type, constr->dbg)), NULL);
                return constr;
            }
        case AST_ID:
            {
                assert(ast->data.id.to);
                if (!ast->data.id.to->node) {
                    // If the identifier is referring to a function, we need to save the current state
                    const node_t* cur = emitter->cur;
                    const node_t* mem = emitter->mem;
                    const node_t* break_ = emitter->break_;
                    const node_t* continue_ = emitter->continue_;
                    const node_t* return_ = emitter->return_;
                    emit(emitter, (ast_t*)ast->data.id.to);
                    emitter->cur = cur;
                    emitter->mem = mem;
                    emitter->break_ = break_;
                    emitter->continue_ = continue_;
                    emitter->return_ = return_;
                }
                const node_t* node = ast->data.id.to->node;
                if (node->type->tag == TYPE_PTR) {
                    const node_t* load = node_load(emitter->mod, emitter->mem, node, NULL);
                    emitter->mem = node_extract(emitter->mod, load, node_i32(emitter->mod, 0), NULL);
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
            return emit(emitter, ast->data.annot.ast);
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
                const node_t* mem = emitter->mem;
                const node_t* cur = emitter->cur;
                const node_t* ret = emitter->return_;
                const node_t* fn = emit_fn(emitter, ast->type, ast->data.fn.param, NULL, ast->loc);
                const node_t* value = emit(emitter, ast->data.fn.body);
                return_from(emitter, emitter->return_, value);
                emitter->mem = mem;
                emitter->cur = cur;
                emitter->return_ = ret;
                return fn;
            }
        case AST_IF:
            {
                const type_t* branch_type = type_cn(emitter->mod, type_unit(emitter->mod));
                const type_t* join_type = type_cn(emitter->mod, type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), ast->type));
                const node_t* if_true = node_fn(emitter->mod, branch_type, 0, make_dbg(emitter, "if_true", ast->data.if_.if_true->loc));
                const node_t* if_false = node_fn(emitter->mod, branch_type, 0, make_dbg(emitter, "if_false", ast->data.if_.if_false ? ast->data.if_.if_false->loc : ast->loc));
                const node_t* if_join = node_fn(emitter->mod, join_type, 0, make_dbg(emitter, "if_join", ast->loc));

                // Enter the conditional
                const node_t* cond = emit(emitter, ast->data.if_.cond);
                const node_t* next = node_select(emitter->mod, cond, if_true, if_false, NULL);
                jump(emitter, next, node_unit(emitter->mod), NULL);

                // Emit true branch
                const node_t* mem = emitter->mem;
                emitter->cur  = if_true;
                const node_t* true_val = emit(emitter, ast->data.if_.if_true);
                jump(emitter, if_join, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, true_val), NULL);

                // Emit false branch
                emitter->mem = mem;
                emitter->cur  = if_false;
                const node_t* false_val = ast->data.if_.if_false ? emit(emitter, ast->data.if_.if_false) : node_unit(emitter->mod);
                jump(emitter, if_join, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, false_val), NULL);

                // Emit join block
                const node_t* param = node_param(emitter->mod, if_join, NULL);
                emitter->cur = if_join;
                emitter->mem = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), NULL);
                return node_extract(emitter->mod, param, node_i32(emitter->mod, 1), make_dbg(emitter, NULL, ast->loc));
            }
        case AST_WHILE:
            {
                const type_t* branch_type = type_cn(emitter->mod, type_unit(emitter->mod));
                const type_t* join_type = type_cn(emitter->mod, type_mem(emitter->mod));
                const type_t* cont_type = type_cn(emitter->mod, type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), type_unit(emitter->mod)));
                const node_t* while_head = node_fn(emitter->mod, join_type, 0, make_dbg(emitter, "while_head", ast->data.while_.cond->loc));
                const node_t* while_exit = node_fn(emitter->mod, branch_type, 0, make_dbg(emitter, "while_exit", ast->loc));
                const node_t* while_body = node_fn(emitter->mod, branch_type, 0, make_dbg(emitter, "while_body", ast->data.while_.body->loc));
                const node_t* while_break = node_fn(emitter->mod, cont_type, 0, make_dbg(emitter, "while_break", ast->loc));
                const node_t* while_continue = node_fn(emitter->mod, cont_type, 0, make_dbg(emitter, "while_continue", ast->loc));
                const node_t* while_join = node_fn(emitter->mod, join_type, 0, make_dbg(emitter, "while_join", ast->loc));

                // Enter the loop
                jump(emitter, while_head, emitter->mem, NULL);

                // Link break/continue to join/head
                const node_t* break_mem = node_extract(emitter->mod, node_param(emitter->mod, while_break, NULL), node_i32(emitter->mod, 0), NULL);
                const node_t* continue_mem = node_extract(emitter->mod, node_param(emitter->mod, while_continue, NULL), node_i32(emitter->mod, 0), NULL);
                jump_from(emitter, while_break, while_join, break_mem, NULL);
                jump_from(emitter, while_continue, while_head, continue_mem, NULL);
                always_inline(emitter, while_break);
                always_inline(emitter, while_continue);

                // Emit while head
                emitter->mem = node_param(emitter->mod, while_head, NULL);
                emitter->cur = while_head;
                const node_t* cond = emit(emitter, ast->data.while_.cond);
                const node_t* next = node_select(emitter->mod, cond, while_body, while_exit, NULL);
                jump(emitter, next, node_unit(emitter->mod), NULL);

                // Emit while_exit
                jump_from(emitter, while_exit, while_join, emitter->mem, NULL);

                // Emit while_body
                emitter->cur = while_body;
                const node_t* break_ = emitter->break_;
                const node_t* continue_ = emitter->continue_;
                emitter->break_ = while_break;
                emitter->continue_ = while_continue;
                emit(emitter, ast->data.while_.body);
                if (emitter->cur)
                    jump(emitter, while_head, emitter->mem, NULL);
                emitter->break_ = break_;
                emitter->continue_ = continue_;

                // Emit while_join
                emitter->cur = while_join;
                emitter->mem = node_param(emitter->mod, while_join, NULL);
                return node_unit(emitter->mod);
            }
        case AST_FOR:
            {
                const node_t* callee = emit(emitter, ast->data.for_.call->data.call.callee);
                ast_list_t* args = ast->data.for_.call->data.call.args;
                ast_t* fn = args->ast->data.tuple.args->ast;

                // Emit a function for the loop body
                const node_t* cur = emitter->cur;
                const node_t* mem = emitter->mem;
                const node_t* return_ = emitter->return_;
                fn->type = convert(emitter, fn->type);
                fn->node = emit_fn(emitter, fn->type, fn->data.fn.param, "for_body", fn->loc);
                const node_t* fn_cur = emitter->cur;
                const node_t* fn_mem = emitter->mem;
                const node_t* for_continue = emitter->return_;
                emitter->cur = cur;
                emitter->mem = mem;
                emitter->return_ = return_;

                // Emit call to iteration function
                const node_t* for_break = NULL;
                FORALL_AST(args, arg, {
                    const node_t* arg_node = emit(emitter, arg);
                    callee = emit_call(emitter, callee, arg_node, &for_break, ast->loc);
                })
                cur = emitter->cur;
                mem = emitter->mem;
                const node_t* continue_ = emitter->continue_;
                const node_t* break_ = emitter->break_;

                // Emit the body of the function
                emitter->cur = fn_cur;
                emitter->mem = fn_mem;
                emitter->break_ = for_break;
                emitter->continue_ = for_continue;
                const node_t* value = emit(emitter, fn->data.fn.body);
                return_from(emitter, for_continue, value);
                ((node_t*)for_break)->dbg = make_dbg(emitter, "for_break", ast->loc);
                ((node_t*)for_continue)->dbg = make_dbg(emitter, "for_continue", ast->loc);

                emitter->cur = cur;
                emitter->mem = mem;
                emitter->break_ = break_;
                emitter->continue_ = continue_;
                return callee;
            }
        case AST_VAL:
        case AST_VAR:
            emit_ptrn(emitter, ast->data.varl.ptrn, emit(emitter, ast->data.varl.value), ast->tag == AST_VAR);
            return node_unit(emitter->mod);
        case AST_DEF:
            {
                const node_t* mem = emitter->mem;
                const node_t* cur = emitter->cur;
                const node_t* ret = emitter->return_;
                const node_t* fn = emit_curried_fn(emitter, ast->type, ast->data.def.params, ast->data.def.id->data.id.str, ast->loc);
                ast->node = fn; // Allow recursive calls
                const node_t* value = emit(emitter, ast->data.def.value);
                return_from(emitter, emitter->return_, value);
                emitter->mem = mem;
                emitter->cur = cur;
                emitter->return_ = ret;
                return fn;
            }
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_INT:
                case LIT_FLT:
                case LIT_BOOL:
                    switch (ast->type->tag) {
                        case TYPE_I8:   check_int_lit(emitter, ast, INT8_MAX);         return node_i8(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_I16:  check_int_lit(emitter, ast, INT16_MAX);        return node_i16(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_I32:  check_int_lit(emitter, ast, INT32_MAX);        return node_i32(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_I64:  check_int_lit(emitter, ast, INT64_MAX);        return node_i64(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_U8:   check_int_lit(emitter, ast, UINT8_MAX);        return node_u8(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_U16:  check_int_lit(emitter, ast, UINT16_MAX);       return node_u16(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_U32:  check_int_lit(emitter, ast, UINT32_MAX);       return node_u32(emitter->mod, ast->data.lit.value.ival);
                        case TYPE_F32:  check_flt_lit(emitter, ast, FLT_MIN, FLT_MAX); return node_f32(emitter->mod, ast->data.lit.value.fval, ast->type->data.fp_flags);
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
                case CONT_BREAK:    return emitter->break_;
                case CONT_CONTINUE: return emitter->continue_;
                case CONT_RETURN:   return emitter->return_;
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
