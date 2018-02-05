#include "scope.h"

void scope_compute(mod_t* mod, const node_t* fn, node_set_t* scope) {
    assert(fn->tag == NODE_FN);
    node_vec_t worklist = node_vec_create(64);
    node_set_insert(scope, fn);
    node_vec_push(&worklist, node_param(mod, fn, NULL));
    while (worklist.nelems > 0) {
        const node_t* node = worklist.elems[--worklist.nelems];
        if (node_set_insert(scope, node)) {
            const use_t* use = node->uses;
            while (use) {
                node_vec_push(&worklist, use->user);
                use = use->next;
            }
            if (node->tag == NODE_FN)
                node_vec_push(&worklist, node_param(mod, node, NULL));
        }
    }
    node_vec_destroy(&worklist);
}

void scope_compute_fvs(const node_t* fn, node_set_t* scope, node_set_t* fvs) {
    assert(fn->tag == NODE_FN);
    node_set_t done = node_set_create(64);
    node_vec_t worklist = node_vec_create(64);
    for (size_t i = 0; i < fn->nops; ++i)
        node_vec_push(&worklist, fn->ops[i]);
    while (worklist.nelems > 0) {
        const node_t* node = worklist.elems[--worklist.nelems];
        if (node->tag == NODE_PARAM || node->tag == NODE_FN) {
            if (!node_set_lookup(scope, node))
                node_set_insert(fvs, node);
        } else {
            for (size_t i = 0; i < node->nops; ++i) {
                const node_t* op = node->ops[i];
                if (!node_set_lookup(&done, op))
                    node_vec_push(&worklist, op);
            }
        }
    }
    node_set_destroy(&done);
    node_vec_destroy(&worklist);
}
