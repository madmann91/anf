#include <float.h>
#include <stdint.h>

#include "emit.h"

static inline const dbg_t* dbg_from_name(emitter_t* emitter, const char* name, loc_t loc) {
    dbg_t* dbg = mpool_alloc(&emitter->mod->pool, sizeof(dbg_t));
    dbg->name = name;
    dbg->file = emitter->file;
    dbg->loc  = loc;
    return dbg;
}

static inline const dbg_t* dbg_from_loc(emitter_t* emitter, loc_t loc) {
    return dbg_from_name(emitter, NULL, loc);
}

static inline const type_t* continuation_type(emitter_t* emitter, const type_t* fn_type) {
    assert(fn_type->tag == TYPE_FN);
    const type_t* ret    = type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), fn_type->ops[1]);
    const type_t* param  = type_tuple_from_args(emitter->mod, 3,
        type_mem(emitter->mod),
        fn_type->ops[0],
        type_fn(emitter->mod, ret, type_bottom(emitter->mod)));
    return type_fn(emitter->mod, param, type_bottom(emitter->mod));
}

static inline const type_t* basic_block_type(emitter_t* emitter, const type_t* arg) {
    return type_fn(emitter->mod, arg, type_bottom(emitter->mod));
}

static inline void jump(emitter_t* emitter, const node_t* fn, const node_t* arg, bool cond, const dbg_t* dbg) {
    node_bind(emitter->mod, emitter->bb, 0, node_app(emitter->mod, fn, arg, node_bool(emitter->mod, cond), dbg));
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

static void emit_ptrn(emitter_t* emitter, ast_t* ast, const node_t* node) {
    switch (ast->tag) {
        case AST_TUPLE:
            {
                size_t nops = 0;
                FORALL_AST(ast->data.tuple.args, arg, {
                    const dbg_t*  arg_dbg  = dbg_from_loc(emitter, arg->loc);
                    const node_t* arg_node = node_extract(emitter->mod, node, node_i32(emitter->mod, nops++), arg_dbg);
                    emit_ptrn(emitter, arg, arg_node);
                })
            }
            break;
        case AST_ID:
            ((dbg_t*)node->dbg)->name = ast->data.id.str;
            ast->node = node;
            break;
        case AST_ANNOT:
            emit_ptrn(emitter, ast->data.annot.ast, node);
            break;
        default:
            assert(false);
            break;
    }
}

static const node_t* emit_internal(emitter_t* emitter, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROG:
            FORALL_AST(ast->data.prog.mods, mod, { emit(emitter, mod); })
            return NULL;
        case AST_MOD:
            FORALL_AST(ast->data.mod.decls, decl, { emit(emitter, decl); })
            return NULL;
        case AST_ID:
            assert(ast->data.id.to && ast->data.id.to->node);
            return ast->data.id.to->node;
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
                const node_t* node = node_tuple(emitter->mod, nargs, node_ops, dbg_from_loc(emitter, ast->loc));
                TMP_BUF_FREE(node_ops)
                return node;
            }
            break;
        case AST_BLOCK:
            {
                const node_t* last = node_unit(emitter->mod);
                FORALL_AST(ast->data.block.stmts, stmt, {
                    last = emit(emitter, stmt);
                })
                return last;
            }
        case AST_IF:
            {
                const type_t* bb_type = basic_block_type(emitter, type_unit(emitter->mod));
                const type_t* join_type = basic_block_type(emitter, type_tuple_from_args(emitter->mod, 2, type_mem(emitter->mod), ast->type));
                const node_t* if_true = node_fn(emitter->mod, bb_type, 0, dbg_from_name(emitter, "if_true", ast->data.if_.if_true->loc));
                const node_t* if_false = node_fn(emitter->mod, bb_type, 0, dbg_from_name(emitter, "if_false", ast->data.if_.if_false ? ast->data.if_.if_false->loc : ast->loc));
                const node_t* join = node_fn(emitter->mod, join_type, 0, dbg_from_name(emitter, "if_join", ast->loc));

                const node_t* cond = emit(emitter, ast->data.if_.cond);
                const node_t* next = node_select(emitter->mod, cond, if_true, if_false, NULL);
                jump(emitter, next, node_unit(emitter->mod), true, NULL);

                const node_t* mem = emitter->mem;
                emitter->bb  = if_true;
                const node_t* true_val = emit(emitter, ast->data.if_.if_true);
                jump(emitter, join, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, true_val), false, NULL);

                emitter->mem = mem;
                emitter->bb  = if_false;
                const node_t* false_val = ast->data.if_.if_false ? emit(emitter, ast->data.if_.if_false) : node_unit(emitter->mod);
                jump(emitter, join, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, false_val), false, NULL);

                const node_t* param = node_param(emitter->mod, join, NULL);
                emitter->bb = join;
                emitter->mem = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), NULL);
                return node_extract(emitter->mod, param, node_i32(emitter->mod, 1), dbg_from_loc(emitter, ast->loc));
            }
        case AST_WHILE:
            {
                const type_t* bb_type = basic_block_type(emitter, type_unit(emitter->mod));
                const type_t* join_type = basic_block_type(emitter, type_mem(emitter->mod));
                const node_t* while_head = node_fn(emitter->mod, join_type, 0, dbg_from_name(emitter, "while_head", ast->data.while_.cond->loc));
                const node_t* while_exit = node_fn(emitter->mod, bb_type, 0, dbg_from_name(emitter, "while_exit", ast->loc));
                const node_t* while_body = node_fn(emitter->mod, bb_type, 0, dbg_from_name(emitter, "while_body", ast->data.while_.body->loc));
                const node_t* join = node_fn(emitter->mod, join_type, 0, dbg_from_name(emitter, "while_join", ast->loc));

                jump(emitter, while_head, emitter->mem, false, NULL);

                emitter->mem = node_param(emitter->mod, while_head, NULL);
                emitter->bb  = while_head;
                const node_t* cond = emit(emitter, ast->data.while_.cond);
                const node_t* next = node_select(emitter->mod, cond, while_body, while_exit, NULL);
                jump(emitter, next, node_unit(emitter->mod), true, NULL);

                emitter->bb = while_exit;
                jump(emitter, join, emitter->mem, false, NULL);

                emitter->bb = while_body;
                emit(emitter, ast->data.while_.body);
                jump(emitter, while_head, emitter->mem, false, NULL);

                emitter->bb = join;
                emitter->mem = node_param(emitter->mod, join, NULL);
                return node_unit(emitter->mod);
            }
        case AST_DEF:
            {
                const dbg_t* dbg = dbg_from_name(emitter, ast->data.def.id->data.id.str, ast->loc);
                const node_t* fn = node_fn(emitter->mod, continuation_type(emitter, ast->type), 0, dbg);
                const node_t* param = node_param(emitter->mod, fn, dbg_from_loc(emitter, ast->data.def.param->loc));
                const node_t* mem  = node_extract(emitter->mod, param, node_i32(emitter->mod, 0), NULL);
                const node_t* ptrn = node_extract(emitter->mod, param, node_i32(emitter->mod, 1), dbg_from_loc(emitter, ast->data.def.param->loc));
                const node_t* ret  = node_extract(emitter->mod, param, node_i32(emitter->mod, 2), NULL);

                emit_ptrn(emitter, ast->data.def.param, ptrn);

                emitter->mem = mem;
                emitter->bb  = fn;
                const node_t* value = emit(emitter, ast->data.def.value);
                jump(emitter, ret, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, value), false, NULL);
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
                    return node_string(emitter->mod, ast->data.lit.str, dbg_from_loc(emitter, ast->loc));
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
    return ast->node = emit_internal(emitter, ast);
}
