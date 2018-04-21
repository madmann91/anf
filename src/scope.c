#include "scope.h"

void scope_compute(mod_t* mod, scope_t* scope) {
    node_vec_t worklist = node_vec_create(64);
    node_set_insert(&scope->nodes, &scope->entry->node);
    node_vec_push(&worklist, node_param(mod, scope->entry, NULL));
    // transitively add the uses of the parameter of the entry function to the scope
    while (worklist.nelems > 0) {
        const node_t* node = node_vec_pop(&worklist);
        if (node_set_insert(&scope->nodes, node)) {
            const use_t* use = node->uses;
            while (use) {
                node_vec_push(&worklist, use->user);
                use = use->next;
            }
            if (node->tag == NODE_FN)
                node_vec_push(&worklist, node_param(mod, fn_cast(node), NULL));
        }
    }
    node_vec_destroy(&worklist);
}

void scope_compute_fvs(const scope_t* scope, node_set_t* fvs) {
    node_set_t done = node_set_create(64);
    node_vec_t worklist = node_vec_create(64);
    // look through all the expressions contained in the entry
    // function and extract those that are not in the scope
    const node_t* entry = &scope->entry->node;
    for (size_t i = 0; i < entry->nops; ++i)
        node_vec_push(&worklist, entry->ops[i]);
    while (worklist.nelems > 0) {
        const node_t* node = node_vec_pop(&worklist);
        if (node->tag == NODE_PARAM || node->tag == NODE_FN) {
            if (!node_set_lookup(&scope->nodes, node))
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
