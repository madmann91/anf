#include "anf.h"
#include "scope.h"

bool flatten_tuples(mod_t*);
bool partial_eval(mod_t*);
bool mem2reg(mod_t*);

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

void mod_cleanup(mod_t** mod) {
    // Import the old module into the new one.
    // This will trigger local folding rules.
    mod_t* new_mod = mod_create();
    mod_import(*mod, new_mod);
    mod_destroy(*mod);
    *mod = new_mod;
}

void mod_opt(mod_t** mod) {
    bool todo = true;
    while (todo) {
        todo = false;
        if (flatten_tuples(*mod))
            mod_cleanup(mod), todo = true;
        if (mem2reg(*mod))
            mod_cleanup(mod), todo = true;
        if (partial_eval(*mod))
            mod_cleanup(mod), todo = true;
    }
}
