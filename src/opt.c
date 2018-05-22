#include "anf.h"

void mod_import(const mod_t* from, mod_t* to) {
    node2node_t new_nodes = node2node_create(from->nodes.table->cap / 2);
    type2type_t new_types = type2type_create(from->types.table->cap / 2);
    
    FORALL_VEC(from->fns, const fn_t*, fn, {
        if (!fn->is_exported)
            continue;
        node_rewrite(to, &fn->node, &new_nodes, &new_types);
    })

    node2node_destroy(&new_nodes);
    type2type_destroy(&new_types);
}

void mod_opt(mod_t** mod) {
    bool todo = true;
    while (todo) {
        todo = false;
        // TODO: Implement other optimizations here
        // todo |= mem2reg(*mod);

        // Import the old module into the new one.
        // This will trigger local folding rules and partial evaluation.
        mod_t* new_mod = mod_create();
        mod_import(*mod, new_mod);
        todo |= (*mod)->nodes.table->nelems != new_mod->nodes.table->nelems;
        mod_destroy(*mod);
        *mod = new_mod;
    }
}
