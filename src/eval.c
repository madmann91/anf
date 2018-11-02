#include "anf.h"
#include "scope.h"

static bool is_from_extract(const node_t* node, const node_t* base) {
    // Nodes of the form extract(...extract(base, ...)...)
    return node == base || (node->tag == NODE_EXTRACT && is_from_extract(node->ops[0], base));
}

static bool is_tuple_shuffle(const node_t* node, const node_t* base) {
    // Nodes of the form tuple(extract(base, ...), tuple(..., extract(base, ...), ...), ...)
    if (is_from_extract(node, base))
        return true;
    if (node->tag != NODE_TUPLE)
        return false;
    for (size_t i = 0; i < node->nops; ++i) {
        if (!is_tuple_shuffle(node->ops[i], base))
            return false;
    }
    return true;
}

static inline bool is_eta_convertible(mod_t* mod, const fn_t* fn, const scope_t* scope) {
    // Functions whose bodies are only calling another function
    // with a permutation of their parameters are all eta-convertible
    if (fn->node.ops[0]->tag != NODE_APP)
        return false;
    const node_t* app = fn->node.ops[0];
    const node_t* param = node_param(mod, fn, NULL);
    // The argument must be a shuffled version of the parameter
    if (!is_tuple_shuffle(app->ops[1], param))
        return false;
    // The callee must be a known function or an extract from the function parameter
    if (is_from_extract(app->ops[0], param))
        return true;
    if (app->ops[0]->tag != NODE_FN)
        return false;
    // If the callee is a function, it must not be inside the scope
    // Consider the following example:
    // f(a, b, c) {
    //   g(d) {
    //     a + b + h(c)
    //   }
    //   h(e) {
    //     e + a
    //   }
    //   g(a)
    // }
    // Here, replacing f with g moves h inside the scope of the caller
    return !node_set_lookup(&scope->nodes, app->ops[0]);
}

static inline bool should_always_inline(const fn_t* fn, const scope_t* scope) {
    use_t* use = fn->node.uses;
    size_t n = 0;
    while (use) {
        // If the use is a parameter, it should be ignored
        if (use->user->tag != NODE_PARAM && n++ > 1)
            return false;
        use = use->next;
    }
    // Should not inline functions that create computation
    // (i.e functions calling something that depend on their parameters)
    return !node_set_lookup(&scope->nodes, fn->node.ops[0]);
}

bool partial_eval(mod_t* mod) {
    node_vec_t apps = node_vec_create();
    node2node_t new_nodes = node2node_create();
    type2type_t new_types = type2type_create();

    // Gather all the application nodes that need evaluation
    scope_t scope = { .nodes = node_set_create() };
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        scope.entry = fn;
        node_set_clear(&scope.nodes);
        scope_compute(mod, &scope);

        bool always_inline  = node_is_one(fn->node.ops[1]) ||
                              should_always_inline(fn, &scope) ||
                              is_eta_convertible(mod, fn, &scope);
        bool zero_cond      = node_is_zero(fn->node.ops[1]);
        const node_t* param = node_param(mod, fn, NULL);
        use_t* use = fn->node.uses;
        while (use) {
            const node_t* user = use->user;
            if (use->index == 0 && !user->rep && user->tag == NODE_APP) {
                bool run = always_inline;
                if (!run && !zero_cond) {
                    node2node_clear(&new_nodes);
                    type2type_clear(&new_types);
                    node2node_insert(&new_nodes, param, user->ops[1]);
                    const node_t* cond = node_rewrite(mod, fn->node.ops[1], &new_nodes, &new_types, 0);
                    run = (cond->tag == NODE_LITERAL && cond->box.i1);
                }
                if (run)
                    node_vec_push(&apps, user);
            }
            use = use->next;
        }
    })

    // Generate a specialized version for each call
    node_set_t fvs = node_set_create();
    const fn_t* prev_fn = NULL;
    FORALL_VEC(apps, const node_t*, app, {
        const fn_t* fn = fn_cast(app->ops[0]);
        node2node_clear(&new_nodes);
        type2type_clear(&new_types);

        // Recompute the function scope only when needed
        if (fn != prev_fn) {
            scope.entry = fn;
            node_set_clear(&scope.nodes);
            node_set_clear(&fvs);
            scope_compute(mod, &scope);
            scope_compute_fvs(&scope, &fvs);
            prev_fn = fn;
        }
        // Keep free variables intact
        FORALL_HSET(fvs, const node_t*, node, {
            node2node_insert(&new_nodes, node, node);
        })
        // Replace parameter with argument but keep original function intact
        node2node_insert(&new_nodes, &fn->node, &fn->node);
        node2node_insert(&new_nodes, node_param(mod, fn, NULL), app->ops[1]);

        node_replace(app, node_rewrite(mod, fn->node.ops[0], &new_nodes, &new_types, REWRITE_FNS));
    })
    node_set_destroy(&scope.nodes);
    node_set_destroy(&fvs);

    bool todo = apps.nelems > 0;
    node_vec_destroy(&apps);
    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);
    return todo;
}
