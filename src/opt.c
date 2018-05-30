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

static const type_t* flatten_type(mod_t* mod, const type_t* type, type2type_t* flat_types) {
    // Flatten a type. The rules are as follows:
    // * Flatten (T1, ..., TN) = (Concat Flatten T1 ... Flatten TN)
    // * Flatten fn T1 T2 = fn Flatten T1 Flatten T2
    // * Flatten T = T for all other cases
    const type_t** found = type2type_lookup(flat_types, type);
    if (found)
        return *found;
    const type_t* new_type = NULL;
    if (type->tag == TYPE_TUPLE) {
        // Compute the number of flattened tuple arguments
        size_t nops = 0;
        const type_t* old_ops[type->nops];
        for (size_t i = 0; i < type->nops; ++i) {
            old_ops[i] = flatten_type(mod, type->ops[i], flat_types);
            nops += old_ops[i]->tag == TYPE_TUPLE ? old_ops[i]->nops : 1;
        }
        // Generate them
        const type_t* new_ops[nops];
        for (size_t i = 0, j = 0; i < type->nops; ++i) {
            if (old_ops[i]->tag == TYPE_TUPLE) {
                for (size_t k = 0; k < old_ops[i]->nops; ++k, ++j)
                    new_ops[j] = old_ops[i]->ops[k];
            } else
                new_ops[j++] = old_ops[j];
        }
        new_type = type_tuple(mod, nops, new_ops);
    } else if (type->tag == TYPE_FN) {
        const type_t* new_arg = flatten_type(mod, type->ops[0], flat_types);
        const type_t* new_ret = flatten_type(mod, type->ops[1], flat_types);
        new_type = type_fn(mod, new_arg, new_ret);
    } else {
        // Only tuples and functions are affected, types such as
        // [((i32, i32), i32)] should be kept intact
        new_type = type;
    }
    type2type_insert(flat_types, type, new_type);
    return new_type;
}

typedef struct flatten_s flatten_t;

struct flatten_s {
    type2type_t flat_types;
    node2node_t flat_nodes;
    node2node_t unflat_nodes;
};

static const node_t* flatten_node(mod_t* mod, const node_t* node, flatten_t* flatten) {
    if (flat_type == node->type)
        return node;
    const node_t* found = node2node_lookup(&flatten->flat_nodes, node);
    if (found)
        return *found;
    const node_t* new_node = NULL;
    const type_t* flat_type = flatten_type(mod, node->type, &flatten->flat_types);
    if (node->type->tag == TYPE_TUPLE) {
        // from (a, (b, c), d) get (a, b, c, d)
        const node_t* ops[flat_type->nops];
        for (size_t i = 0, j = 0; i < node->type->nops; ++i) {
            const node_t* op = node_extract(mod, node, node_i32(mod, i), node->dbg);
            const type_t* type_op = op->type;
            const type_t* flat_type_op = flatten_type(mod, type_op, &flatten->flat_types);
            const node_t* flat_op = flatten_node(mod, op, flat_type_op, flatten);
            if (flat_op->tag == TYPE_TUPLE) {
                for (size_t k = 0; k < flat_op->nops; ++k)
                    ops[j++] = flat_op->ops[k];
            } else
                ops[j++] = flat_op;
        }
        new_node = node_tuple(mod, flat_type->nops, ops, node->dbg);
    } else if (node->type->tag == TYPE_FN) {
        // from fn (a, (b, c), d) get a wrapper with signature fn (a, b, c, d)
        const node_t* unflat_fn = node;
        fn_t* flat_fn = node_fn(mod, flat_type, node->dbg);
        const node_t* flat_param = node_param(mod, flat_fn, flat_fn->node.dbg);
        size_t index = 0;
        const node_t* unflat_arg = unflatten(mod, flat_param, &index, unflat_fn->type->ops[0], flatten);
        fn_bind(mod, flat_fn, node_app(mod, unflat_fn, unflat_arg, flat_fn->node.dbg));
        fn_run_if(mod, flat_fn, node_i1(mod, false));
        new_node = &flat_fn->node;
    } else {
        assert(false);
    }
    assert(new_node);
    node2node_insert(&flatten->flat_nodes, node, new_node);
    return new_node;
}

static const node_t* unflatten_node(mod_t* mod, const node_t* node, size_t* index, const type_t* unflat_type, flatten_t* flatten) {
    if (unflat_type == node->type)
        return node;
    const node_t* found = node2node_lookup(&flatten->unflat_nodes, node);
    if (found)
        return *found;
    const node_t* new_node = NULL;
    if (unflat_type->tag == TYPE_TUPLE) {
        // from (a, b, c, d) get (a, (b, c), d)
        const node_t* ops[unflat_type->nops];
        for (size_t i = 0; i < unflat_type->nops; ++i) {
            const type_t* type_op = unflat_type->ops[i];
            ops[i] = unflatten_node(mod, node, index, type_op, flatten);
        }
        new_node = node_tuple(mod, unflat_type->nops, ops, node->dbg);
    } else if (unflat_type->tag == TYPE_FN) {
        // from fn (a, b, c, d) get a wrapper with signature fn (a, (b, c), d)
        const node_t* flat_fn = node_extract(mod, node, node_i32(mod, *index++), node->dbg);
        assert(flat_fn->type->tag == NODE_FN);
        fn_t* unflat_fn = node_fn(mod, unflat_type, flat_fn->dbg);
        const node_t* unflat_param = node_param(mod, unflat_fn, flat_fn->dbg);
        const node_t* flat_arg = flatten_node(mod, unflat_param, flatten);
        fn_bind(mod, unflat_fn, node_app(mod, flat_fn, flat_arg, unflat_fn->node.dbg));
        fn_run_if(mod, unflat_fn, node_i1(mod, true));
        new_node = &fn->node;
    } else {
        new_node = node_extract(mod, node, node_i32(mod, *index++), node->dbg);
    }
    assert(new_node);
    return new_node;
}

static void flatten_fn(mod_t* mod, fn_t* fn, const type_t* flat_type) {
    fn_t* flat_fn = node_fn(mod, flat_type, NULL);
    const node_t* param = node_param(mod, fn, NULL);
    const node_t* flat_param = node_param(mod, flat_fn, param->dbg);

    node2node_t new_nodes = node2node_create(64);
    type2type_t new_types = type2type_create(64);
    // Rewrite runif of old function into new_function
    node2node_insert(&new_nodes, param, flat_param);
    fn_run_if(mod, flat_fn, node_rewrite(mod, fn->node.ops[1], &new_nodes, &new_types, false));
    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);

    // Always inline old function
    fn_run_if(mod, fn, node_i1(mod, true));

    // Make new function call the old_function

    size_t index = 0;
    const node_t* unflat_param = unflatten_node(mod, flat_param, &index, param->type);
    assert(index == flat_param->type->nops);
    fn_bind(mod, flat_fn, node_app(mod, fn, unflat_param, fn->node.ops[0]->dbg));
}

static void flatten_tuples(mod_t* mod) {
    node2node_t flat_nodes = node2node_create(64);
    type2type_t flat_types = type2type_create(64);

    FORALL_VEC(mod->fns, fn_t*, fn, {
        if (fn->imported || fn->exported || fn->is_intrinsic)
            continue;
        const flat_type = flatten_type(mod, fn->node.type, &flat_types);
        if (flat_type != fn->node.type) {
            // TODO: Flatten
        }
    })

    node2node_destroy(&flat_nodes);
    type2type_destroy(&flat_types);
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
