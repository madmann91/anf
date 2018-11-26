#include "bind.h"

static inline env_t* alloc_env(env_t* prev) {
    env_t* env = xmalloc(sizeof(env_t));
    env->id2ast = id2ast_create();
    env->prev = prev;
    env->next = NULL;
    return env;
}

static inline void free_env(env_t* env) {
    assert(env && !env->prev);
    while (env) {
        env_t* next = env->next;
        id2ast_destroy(&env->id2ast);
        free(env);
        env = next;
    }
}

static inline void push_env(binder_t* binder) {
    env_t* env = binder->env;
    assert(env);
    if (!env->next)
        env->next = alloc_env(env);
    id2ast_clear(&env->next->id2ast);
    binder->env = env->next;
}

static inline void pop_env(binder_t* binder) {
    env_t* env = binder->env;
    assert(env && env->prev);
    binder->env = env->prev;
}

static inline const ast_t* lookup_env(binder_t* binder, const char* id) {
    env_t* env = binder->env;
    assert(env);
    while (env) {
        const ast_t** found = id2ast_lookup(&env->id2ast, id);
        if (found)
            return *found;
        env = env->prev;
    }
    return NULL;
}

static inline void insert_env(binder_t* binder, ast_t* id, ast_t* ast) {
    assert(id->tag == AST_ID);
    const ast_t* prev = lookup_env(binder, id->data.id.str);
    if (!id2ast_insert(&binder->env->id2ast, id->data.id.str, ast)) {
        log_error(binder->log, &id->loc, "identifier '{0:s}' has already been declared", { .str = id->data.id.str });
    } else if (prev) {
        log_warn(binder->log, &id->loc, "identifier '{0:s}' shadows previous declaration", { .str = id->data.id.str });
    }

    if (prev)
        log_note(binder->log, &prev->loc, "previous declaration was here");
}

static void bind_ptrn(binder_t* binder, ast_t* ast) {
    // This function put variables contained
    // in patterns into the environment
    switch (ast->tag) {
        case AST_ID: 
            insert_env(binder, ast, ast);
            break;
        case AST_ANNOT:
            bind_ptrn(binder, ast->data.annot.ast);
            bind(binder, ast->data.annot.type);
            break;
        case AST_TUPLE:
            FORALL_AST(ast->data.tuple.args, arg, {
                bind_ptrn(binder, arg);
            })
            break;
        case AST_LIT:
            break;
        default:
            assert(false);
            break;
    }
}

static void bind_head(binder_t* binder, ast_t* ast) {
    // This function makes a preliminary pass over AST nodes
    // in order to put declarations inside the environment.
    switch (ast->tag) {
        case AST_BLOCK:
        case AST_MOD:
            FORALL_AST(ast->tag == AST_MOD ? ast->data.mod.decls : ast->data.block.stmts, decl, {
                if (decl->tag == AST_DEF)
                    insert_env(binder, decl->data.def.id, decl);
                if (decl->tag == AST_STRUCT)
                    insert_env(binder, decl->data.struct_.id, decl);
                if (decl->tag == AST_VAR || decl->tag == AST_VAL)
                    bind_ptrn(binder, decl->data.varl.ptrn);
            })
            break;
        case AST_PROGRAM:
            FORALL_AST(ast->data.program.mods, mod, {
                insert_env(binder, mod->data.mod.id, mod);
            })
            break;
        default:
            assert(false);
            break;
    }
}

void bind(binder_t* binder, ast_t* ast) {
    switch (ast->tag) {
        case AST_PROGRAM:
            assert(binder->env == NULL);
            binder->fn = NULL;
            binder->loop = NULL;
            binder->env = alloc_env(NULL);
            bind_head(binder, ast);
            FORALL_AST(ast->data.program.mods, mod, {
                bind(binder, mod);
            })
            free_env(binder->env);
            binder->env = NULL;
            break;
        case AST_MOD:
            push_env(binder);
            bind_head(binder, ast);
            FORALL_AST(ast->data.mod.decls, decl, {
                bind(binder, decl);
            })
            pop_env(binder);
            break;
        case AST_ID:
            {
                // This is a use of a previously declared identifier
                const ast_t* to = lookup_env(binder, ast->data.id.str);
                if (!to) {
                    log_error(binder->log, &ast->loc, "unknown identifier '{0:s}'", { .str = ast->data.id.str });
                    return;
                }
                ast->data.id.to = to;
            }
            break;
        case AST_STRUCT:
            push_env(binder);
            bind_ptrn(binder, ast->data.struct_.members);
            pop_env(binder);
            break;
        case AST_VAR:
        case AST_VAL:
            bind(binder, ast->data.varl.value);
            break;
        case AST_DEF:
            {
                push_env(binder);
                if (ast->data.def.param)
                    bind_ptrn(binder, ast->data.def.param);
                ast_t* fn = binder->fn;
                binder->fn = ast;
                bind(binder, ast->data.def.value);
                binder->fn = fn;
                pop_env(binder);
            }
            break;
        case AST_TUPLE:
            FORALL_AST(ast->data.tuple.args, arg, {
                bind(binder, arg);
            })
            break;
        case AST_ANNOT:
            bind(binder, ast->data.annot.ast);
            bind(binder, ast->data.annot.type);
            break;
        case AST_ARRAY:
            FORALL_AST(ast->data.array.elems, elem, {
                bind(binder, elem);
            })
            break;
        case AST_FIELD:
            bind(binder, ast->data.field.arg);
            break;
        case AST_BLOCK:
            push_env(binder);
            bind_head(binder, ast);
            FORALL_AST(ast->data.block.stmts, stmt, {
                bind(binder, stmt);
            })
            pop_env(binder);
            break;
        case AST_UNOP:
            bind(binder, ast->data.unop.arg);
            break;
        case AST_BINOP:
            bind(binder, ast->data.binop.left);
            bind(binder, ast->data.binop.right);
            break;
        case AST_LAMBDA:
            {
                push_env(binder);
                bind_ptrn(binder, ast->data.lambda.param);
                ast_t* fn = binder->fn;
                binder->fn = ast;
                bind(binder, ast->data.lambda.body);
                binder->fn = fn;
                pop_env(binder);
            }
            break;
        case AST_CALL:
            bind(binder, ast->data.call.callee);
            bind(binder, ast->data.call.arg);
            break;
        case AST_IF:
            bind(binder, ast->data.if_.cond);
            bind(binder, ast->data.if_.if_true);
            bind(binder, ast->data.if_.if_false);
            break;
        case AST_FOR:
            {
                bind(binder, ast->data.for_.expr);
                push_env(binder);
                bind_ptrn(binder, ast->data.for_.vars);
                ast_t* loop = binder->loop;
                binder->loop = ast;
                bind(binder, ast->data.for_.body);
                binder->loop = loop;
                pop_env(binder);
            }
            break;
        case AST_WHILE:
            {
                bind(binder, ast->data.while_.cond);
                ast_t* loop = binder->loop;
                binder->loop = ast;
                bind(binder, ast->data.while_.body);
                binder->loop = loop;
            }
            break;
        case AST_MATCH:
            bind(binder, ast->data.match.arg);
            FORALL_AST(ast->data.match.cases, case_, {
                bind(binder, case_);
            })
            break;
        case AST_CASE:
            push_env(binder);
            bind_ptrn(binder, ast->data.case_.ptrn);
            bind(binder, ast->data.case_.value);
            pop_env(binder);
            break;
        case AST_CONT:
            switch (ast->data.cont.tag) {
                case CONT_RETURN:
                    if (!binder->fn)
                        log_error(binder->log, &ast->loc, "use of '{$key}return{$}' outside of a function");
                    ast->data.cont.parent = binder->fn;
                    break;
                case CONT_BREAK:
                case CONT_CONTINUE:
                    if (!binder->loop)
                        log_error(binder->log, &ast->loc, "use of '{$key}{0:s}{$}' outside of a loop", { .str = ast->data.cont.tag == CONT_BREAK ? "break" : "continue" });
                    ast->data.cont.parent = binder->loop;
                    break;
            }
            break;
        case AST_LIT:
        case AST_PRIM:
            break;
        default:
            assert(false);
            break;
    }
}
