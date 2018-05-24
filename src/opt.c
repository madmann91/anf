#include "anf.h"
#include "scope.h"

static inline bool should_inline(const fn_t* fn) {
    use_t* use = fn->node.uses;
    size_t n = 0;
    while (use) {
        if (use->user->tag != NODE_PARAM) n++;
        use = use->next;
    }
    return n <= 1;
}

static inline bool eval(mod_t* mod) {
    node_vec_t apps = node_vec_create(64);
    node2node_t new_nodes = node2node_create(16);
    type2type_t new_types = type2type_create(16);
    // Gather all the application nodes that need evaluation
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        if (node_is_zero(fn->node.ops[1]))
            continue;
        bool always_inline = should_inline(fn);
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
    // Generate a specialized version of the function for each call
    FORALL_VEC(apps, const node_t*, app, {
        const fn_t* fn = fn_cast(app->ops[0]);
        node2node_clear(&new_nodes);
        type2type_clear(&new_types);
        node2node_insert(&new_nodes, node_param(mod, fn, NULL), app->ops[1]);
        scope_t scope = { .entry = fn, .nodes = node_set_create(64) };
        scope_compute(mod, &scope);
        scope_rewrite(mod, &scope, &new_nodes, &new_types);
        node_set_destroy(&scope.nodes);
        node_replace(app, node_rewrite(mod, fn->node.ops[0], &new_nodes, &new_types));
    })
    node_vec_destroy(&apps);
    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);
    return apps.nelems > 0;
}

void mod_import(mod_t* from, mod_t* to) {
    node2node_t new_nodes = node2node_create(from->nodes.table->cap / 2);
    type2type_t new_types = type2type_create(from->types.table->cap / 2);

    scope_t scope = { .entry = NULL, .nodes = node_set_create(64) };
    fn_vec_t fn_vec = fn_vec_create(64);
    FORALL_VEC(from->fns, const fn_t*, fn, {
        if (!fn->is_exported)
            continue;
        scope.entry = fn;
        scope_compute(from, &scope); 
    })
    // TODO: Only import the relevant functions
    FORALL_VEC(from->fns, fn_t*, fn, {
        /*if (node->tag != NODE_FN)
            continue;
        fn_t* fn = fn_cast(node);*/
        fn_t* new_fn = node_fn(to, type_rewrite(to, fn->node.type, &new_types), fn->node.dbg);
        new_fn->is_exported  = fn->is_exported;
        new_fn->is_imported  = fn->is_imported;
        new_fn->is_intrinsic = fn->is_intrinsic;
        fn_vec_push(&fn_vec, fn);
        node2node_insert(&new_nodes, &fn->node, &new_fn->node);
    })
    FORALL_VEC(fn_vec, const fn_t*, fn, {
        fn_t* new_fn = fn_cast(*node2node_lookup(&new_nodes, &fn->node));
        fn_bind  (to, new_fn, node_rewrite(to, fn->node.ops[0], &new_nodes, &new_types));
        fn_run_if(to, new_fn, node_rewrite(to, fn->node.ops[1], &new_nodes, &new_types));
    })

    fn_vec_destroy(&fn_vec);
    node_set_destroy(&scope.nodes);
    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);
}

void mod_opt(mod_t** mod) {
    bool todo = true; int n = 0;
    while (todo || n++ < 10) {
        // Import the old module into the new one.
        // This will trigger local folding rules.
        mod_t* new_mod = mod_create();
        mod_import(*mod, new_mod);
        mod_destroy(*mod);
        *mod = new_mod;

        todo = false;
        /*todo |=*/ eval(*mod);
        // todo |= mem2reg(*mod);
    }
}
