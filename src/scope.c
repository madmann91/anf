#include "scope.h"

scope_t scope_create(mod_t* mod, const node_t* fn) {
    assert(fn->tag == NODE_FN);
    scope_t scope = {
        .nodes = node_set_create(64),
        .fn = fn
    };

    node_vec_t worklist = node_vec_create(64);
    node_set_insert(&scope.nodes, fn);
    node_vec_push(&worklist, node_param(mod, fn, NULL));
    while (worklist.nelems > 0) {
        const node_t* node = worklist.elems[--worklist.nelems];
        if (node_set_insert(&scope.nodes, node)) {
            const use_t* use = node->uses;
            while (use) {
                node_vec_push(&worklist, use->user);
                use = use->next;
            }
            if (node->tag == NODE_FN)
                node_vec_push(&worklist, node_param(mod, node, NULL));
        }
    }

    return scope;
}

void scope_destroy(scope_t* scope) {
    node_set_destroy(&scope->nodes);
}

void scope_dump(scope_t* scope) {
    node_dump(scope->fn);
    FORALL_HSET(scope->nodes, const node_t*, node, {
        if (node != scope->fn)
            node_dump(node);
    })
}
