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

static inline bool is_eta_convertible(mod_t* mod, const fn_t* fn) {
    // Functions whose bodies are only calling another function
    // with a permutation of their parameters are all eta-convertible
    if (fn->node.ops[0]->tag != NODE_APP)
        return false;
    const node_t* app = fn->node.ops[0];
    const node_t* param = node_param(mod, fn, NULL);
    // The callee must be a known function or an extract from the function parameter
    if (app->ops[0]->tag != NODE_FN && !is_from_extract(app->ops[0], param))
        return false;
    // The argument must be a shuffled version of the parameter
    return is_tuple_shuffle(app->ops[1], param);
}

static inline bool should_always_inline(const fn_t* fn) {
    use_t* use = fn->node.uses;
    size_t n = 0;
    while (use) {
        if (use->user->tag != NODE_PARAM) n++;
        use = use->next;
    }
    return n <= 1;
}

static bool partial_eval(mod_t* mod) {
    node_vec_t apps = node_vec_create(64);
    node2node_t new_nodes = node2node_create(16);
    type2type_t new_types = type2type_create(16);

    // Gather all the application nodes that need evaluation
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        bool always_inline = should_always_inline(fn) || is_eta_convertible(mod, fn);
        if (node_is_zero(fn->node.ops[1]) && !always_inline)
            continue;
        const node_t* param = node_param(mod, fn, NULL);
        use_t* use = fn->node.uses;
        while (use) {
            const node_t* user = use->user;
            if (use->index == 0 && !user->rep && user->tag == NODE_APP) {
                node2node_clear(&new_nodes);
                type2type_clear(&new_types);
                node2node_insert(&new_nodes, param, user->ops[1]);
                const node_t* cond = node_rewrite(mod, fn->node.ops[1], &new_nodes, &new_types);
                if (always_inline || (cond->tag == NODE_LITERAL && cond->box.i1))
                    node_vec_push(&apps, user);
            }
            use = use->next;
        }
    })

    // Generate a specialized version for each call
    scope_t scope = { .entry = NULL, .nodes = node_set_create(64) };
    node_set_t fvs = node_set_create(64);
    const fn_t* prev_fn = NULL;
    FORALL_VEC(apps, const node_t*, app, {
        const fn_t* fn = fn_cast(app->ops[0]);
        node2node_clear(&new_nodes);
        type2type_clear(&new_types);

        // Recompute the function scope when needed
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

        node_replace(app, node_rewrite(mod, fn->node.ops[0], &new_nodes, &new_types, true));
    })
    node_set_destroy(&scope.nodes);
    node_set_destroy(&fvs);

    node_vec_destroy(&apps);
    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);
    return apps.nelems > 0;
}

void mod_import(mod_t* from, mod_t* to) {
    assert(from != to);
    node2node_t new_nodes = node2node_create(from->nodes.table->cap / 2);
    type2type_t new_types = type2type_create(from->types.table->cap / 2);

    FORALL_VEC(from->fns, const fn_t*, fn, {
        if (!fn->exported || node2node_lookup(&new_nodes, &fn->node))
            continue;
        node_rewrite(to, &fn->node, &new_nodes, &new_types, true);
    })

    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);
}

void mod_opt(mod_t** mod) {
    bool todo = true;
    while (todo) {
        // Import the old module into the new one.
        // This will trigger local folding rules.
        mod_t* new_mod = mod_create();
        mod_import(*mod, new_mod);
        mod_destroy(*mod);
        *mod = new_mod;

        todo = false;
        todo |= partial_eval(*mod);
        // todo |= mem2reg(*mod);
    }
}
