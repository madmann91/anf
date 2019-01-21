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
    const type_t* bottom = type_bottom(emitter->mod);
    const type_t* mem    = type_mem(emitter->mod);
    const type_t* ret    = type_tuple_from_args(emitter->mod, 2, mem, fn_type->ops[1]);
    const type_t* param  = type_tuple_from_args(emitter->mod, 3,
        type_mem(emitter->mod),
        fn_type->ops[0],
        type_fn(emitter->mod, ret, bottom));
    return type_fn(emitter->mod, param, bottom);
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
                node_bind(emitter->mod, fn, 0, node_app(emitter->mod, ret, node_tuple_from_args(emitter->mod, 2, NULL, emitter->mem, value), node_bool(emitter->mod, false), NULL));
                return fn;
            }
        default:
            assert(false);
            return NULL;
    }
}

const node_t* emit(emitter_t* emitter, ast_t* ast) {
    return ast->node = emit_internal(emitter, ast);
}
