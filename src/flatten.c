#include "anf.h"
#include "scope.h"

static const type_t* flatten_type(mod_t*, const type_t*, type2type_t*);
static const node_t* flatten_node(mod_t*, const node_t*, node2node_t*, type2type_t*);
static const node_t* unflatten_node(mod_t*, const node_t*, size_t*, const type_t*, node2node_t*, type2type_t*);

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
                new_ops[j++] = old_ops[i];
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

static const node_t* flatten_node(mod_t* mod, const node_t* node, node2node_t* flat_nodes, type2type_t* flat_types) {
    // Flattens a node.
    const node_t** found = node2node_lookup(flat_nodes, node);
    if (found)
        return *found;
    const type_t* flat_type = flatten_type(mod, node->type, flat_types);
    if (flat_type == node->type)
        return node;
    const node_t* new_node = NULL;
    if (node->type->tag == TYPE_TUPLE) {
        // from (a, (b, c), d) get (a, b, c, d)
        size_t nops = flat_type->tag == TYPE_TUPLE ? flat_type->nops : 1;
        const node_t* ops[flat_type->nops];
        for (size_t i = 0, j = 0; i < node->type->nops; ++i) {
            const node_t* op = node_extract(mod, node, node_i32(mod, i), node->dbg);
            const node_t* flat_op = flatten_node(mod, op, flat_nodes, flat_types);
            if (flat_op->type->tag == TYPE_TUPLE) {
                assert(flat_op->type->nops == 0 || flat_op->tag == NODE_TUPLE);
                for (size_t k = 0; k < flat_op->type->nops; ++k)
                    ops[j++] = flat_op->ops[k];
            } else
                ops[j++] = flat_op;
        }
        new_node = node_tuple(mod, nops, ops, node->dbg);
    } else if (node->type->tag == TYPE_FN) {
        // from fn (a, (b, c), d) get a wrapper with signature fn (a, b, c, d)
        fn_t* flat_fn = node_fn(mod, flat_type, node->dbg);
        new_node = &flat_fn->node;

        const node_t* flat_param = node_param(mod, flat_fn, flat_fn->node.dbg);
        size_t index = 0;
        const node_t* unflat_arg = unflatten_node(mod, flat_param, &index, node->type->ops[0], flat_nodes, flat_types);
        if (node->tag == NODE_FN) {
            // Inline the function body when it is known
            const node_t* body = fn_inline(mod, fn_cast(node), unflat_arg);
            fn_bind(mod, flat_fn, 0, body);
            // Insert the flattened node in the map now, as it may be needed when rewriting
            node2node_insert(flat_nodes, node, new_node);
            fn_bind(mod, flat_fn, 1, node_rewrite(mod, node->ops[1], flat_nodes, flat_types, false));
        } else {
            fn_bind(mod, flat_fn, 0, node_app(mod, node, unflat_arg, node_i1(mod, true), flat_fn->node.dbg));
        }
    } else {
        assert(false);
    }
    assert(new_node);
    node2node_insert(flat_nodes, node, new_node);
    return new_node;
}

static const node_t* unflatten_node(mod_t* mod, const node_t* node, size_t* index, const type_t* unflat_type, node2node_t* flat_nodes, type2type_t* flat_types) {
    if (unflat_type == node->type)
        return node;
    const node_t* new_node = NULL;
    if (unflat_type->tag == TYPE_TUPLE) {
        // from (a, b, c, d) get (a, (b, c), d)
        const node_t* ops[unflat_type->nops];
        for (size_t i = 0; i < unflat_type->nops; ++i) {
            const type_t* type_op = unflat_type->ops[i];
            ops[i] = unflatten_node(mod, node, index, type_op, flat_nodes, flat_types);
        }
        new_node = node_tuple(mod, unflat_type->nops, ops, node->dbg);
    } else if (unflat_type->tag == TYPE_FN) {
        // from fn (a, b, c, d) get a wrapper with signature fn (a, (b, c), d)
        const node_t* flat_fn = node_extract(mod, node, node_i32(mod, *index++), node->dbg);
        assert(flat_fn->type->tag == TYPE_FN);
        fn_t* unflat_fn = node_fn(mod, unflat_type, flat_fn->dbg);
        const node_t* unflat_param = node_param(mod, unflat_fn, flat_fn->dbg);
        const node_t* flat_arg = flatten_node(mod, unflat_param, flat_nodes, flat_types);
        fn_bind(mod, unflat_fn, 0, node_app(mod, flat_fn, flat_arg, node_i1(mod, false), unflat_fn->node.dbg));
        fn_bind(mod, unflat_fn, 1, node_i1(mod, true));
        new_node = &unflat_fn->node;
    } else {
        new_node = node_extract(mod, node, node_i32(mod, (*index)++), node->dbg);
    }
    assert(new_node);
    return new_node;
}

bool flatten_tuples(mod_t* mod) {
    node2node_t flat_nodes = node2node_create(64);
    type2type_t flat_types = type2type_create(64);
    fn_vec_t worklist = fn_vec_create(mod->fns.nelems);

    FORALL_VEC(mod->fns, fn_t*, fn, {
        if (fn->imported || fn->exported || fn->intrinsic)
            continue;
        const type_t* flat_type = flatten_type(mod, fn->node.type, &flat_types);
        if (flat_type != fn->node.type)
            fn_vec_push(&worklist, fn);
    })

    FORALL_VEC(worklist, fn_t*, fn, {
        flatten_node(mod, &fn->node, &flat_nodes, &flat_types);
    })

    node2node_t new_nodes = node2node_create(64);
    type2type_t new_types = type2type_create(64);
    FORALL_VEC(worklist, fn_t*, fn, {
        size_t index = 0;
        const node_t* flat_fn = *node2node_lookup(&flat_nodes, &fn->node);
        const node_t* wrapper = unflatten_node(mod, flat_fn, &index, fn->node.type, &flat_nodes, &flat_types);
        const node_t* unflat_param = node_param(mod, fn, fn->node.dbg);
        node2node_clear(&new_nodes);
        type2type_clear(&new_types);
        node2node_insert(&new_nodes, &fn->node, wrapper);
        node2node_insert(&new_nodes, unflat_param, unflat_param);
        use_t* use = fn->node.uses;
        while (use) {
            if (use->user != flat_fn->ops[0])
                node_replace(use->user, node_rewrite(mod, use->user, &new_nodes, &new_types, false));
            use = use->next;
        }
    })
    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);

    bool todo = worklist.nelems > 0;
    fn_vec_destroy(&worklist);
    node2node_destroy(&flat_nodes);
    type2type_destroy(&flat_types);
    return todo;
}
