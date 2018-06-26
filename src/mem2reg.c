#include "anf.h"
#include "adt.h"
#include "lca.h"
#include "scope.h"

typedef struct state_s state_t;

struct state_s {
    const node_t* alloc;
    const node_t* val;
};

HMAP_DEFAULT(node2state, const node_t*, const state_t*)
VEC(state_vec, state_t*)

static const node_t* ancestor_mem(const node_t* mem) {
    const node_t* node = node_from_mem(mem);
    return node ? node_in_mem(node) : NULL;
}

static const node_t* find_alloc(const node_t* ptr) {
    if (ptr->tag == NODE_EXTRACT && ptr->ops[0]->tag == NODE_ALLOC) {
        return ptr;
    } else if (ptr->tag == NODE_OFFSET) {
        return find_alloc(ptr->ops[0]);
    } else {
        return NULL;
    }
}

static const state_t* find_state(const node2state_t* node2state, const node_t* mem, const node_t* alloc) {
    assert(find_alloc(alloc) == alloc);
    while (true) {
        const state_t** found = node2state_lookup(node2state, mem);
        if (!found)
            return NULL;
        const state_t* state = *found;
        if (state->alloc == alloc)
            return state;
        mem = ancestor_mem(mem);
    }
}

static const node_t* load_value(mod_t* mod, const node_t* base, const node_t* ptr, const dbg_t* dbg) {
    if (ptr->tag == NODE_EXTRACT && ptr->ops[0]->tag == NODE_ALLOC) {
        return base;
    } else if (ptr->tag == NODE_OFFSET) {
        const node_t* val = load_value(mod, base, ptr->ops[0], dbg);
        return node_extract(mod, val, ptr->ops[1], dbg);
    } else {
        assert(false);
        return NULL;
    }
}

static const node_t* store_value(mod_t* mod, const node_t* base, const node_t* ptr, const node_t* val, const dbg_t* dbg) {
    if (ptr->tag == NODE_EXTRACT && ptr->ops[0]->tag == NODE_ALLOC) {
        return val;
    } else if (ptr->tag == NODE_OFFSET) {
        const node_t* sub_base = node_extract(mod, base, ptr->ops[1], dbg);
        const node_t* sub_val  = store_value(mod, sub_base, ptr->ops[0], val, dbg);
        return node_insert(mod, sub_base, ptr->ops[1], sub_val, dbg);
    } else {
        assert(false);
        return NULL;
    }
}

/*static void extract_mems(node_vec_t* mems, const node_t* node) {
    if (node->tag == NODE_TUPLE) {
        for (size_t i = 0; i < node->nops; ++i)
            extract_mems(mems, node->ops[i]);
    } else if (node->type->tag == TYPE_MEM) {
        node_vec_push(mems, node);
    }
}

const node_t* lowest_ancestor(const node_vec_t* mems) {
    node_set_t mem_set = node_set_create();
    FORALL_VEC((*mems), const node_t*, node, {
        node_set_insert(&mem_set, node);
    })
    node_set_t copy_set = node_set_copy(&mem_set);
    node2rank_t node2rank = node2rank_create();
    lca_compute_ranks(&copy_set, &node2rank, ancestor_mem);
    const node_t* lca = lca_compute(&mem_set, &copy_set, &node2rank, ancestor_mem);
    node2rank_destroy(&node2rank);
    node_set_destroy(&mem_set);
    node_set_destroy(&copy_set);
    return lca;
}

static fn_t* transform_fn(mod_t* mod, const fn_t* fn, node2state_t* node2state) {
    if (!type_contains(fn->node.type->ops[0], type_mem(mod)))
        return NULL;

    fn_t* new_fn = NULL;
    node_vec_t in_mems = node_vec_create();

    scope_t scope = { .entry = fn, .nodes = node_set_create() };
    scope_compute(mod, &scope);

    // Extract all the mems from uses outside the scope
    use_t* use = fn->node.uses;
    while (use) {
        if (use->index != 0 || use->user->tag != NODE_APP)
            goto cleanup;
        if (!node_set_lookup(&scope.nodes, use->user))
            extract_mems(&in_mems, use->user->ops[1]);
        use = use->next;
    }

    // Make sure all the input mems have an associated state
    FORALL_VEC(in_mems, const node_t*, mem, {
        if (!node2state_lookup(node2state, mem))
            goto cleanup;
    })

    // Compute the LCA of all these mems
    const node_t* lca = lowest_ancestor(&in_mems);
    if (!lca)
        goto cleanup;

    // Add all allocs that are in between the LCA and the final mem
    // Add all allocs that have been changed inside the scope
    // For each added alloc, create a new parameter
    // Call original function with added parameters and add parameters to state map
    // TODO

cleanup:
    node_vec_destroy(&in_mems);
    node_set_destroy(&scope.nodes);
    return new_fn;
}*/

static bool can_promote(const node_t* node, bool ignore_replaced) {
    // Promotion is possible if all uses are (transitively):
    // load, store (as pointer operand), dealloc and offset
    // Once promotion has been performed, removal is possible
    // if all users that are loads have been replaced.
    use_t* use = node->uses;
    while (use) {
        switch (use->user->tag) {
            case NODE_DEALLOC:
                return true;
            case NODE_LOAD:
                return ignore_replaced || node->rep != NULL;
            case NODE_STORE:
                return use->index == 1;
            case NODE_OFFSET:
                break;
            default:
                return false;
        }
        if (!can_promote(use->user, ignore_replaced))
            return false;
        use = use->next;
    }
    return true;
}

bool mem2reg(mod_t* mod) {
    node2state_t node2state = node2state_create();
    mpool_t* state_pool = mpool_create();

    // Scan nodes to find allocs
    FORALL_HSET(mod->nodes, const node_t*, node, {
        if (node->tag != NODE_ALLOC)
            continue;
        const node_t* mem = node_extract(mod, node, node_i32(mod, 0), NULL);
        const node_t* ptr = node_extract(mod, node, node_i32(mod, 1), NULL);
        if (!can_promote(ptr, true))
            continue;
        state_t* state = mpool_alloc(&state_pool, sizeof(state_t));
        state->alloc = ptr;
        state->val = node_undef(mod, ptr->type->ops[0]);
        node2state_insert(&node2state, mem, state);
    });

    // Remove loads from known pointers
    bool todo = true;
    size_t eliminated_loads = 0;
    while (todo) {
        todo = false;
        FORALL_HSET(mod->nodes, const node_t*, node, {
            // Skip nodes with no memory operand and allocs
            if (!node_has_mem(node) || node->tag == NODE_ALLOC)
                continue;
            // Skip already discovered nodes
            const node_t* out_mem = node_out_mem(mod, node);
            const node_t* in_mem  = node_in_mem(node);
            if (node2state_lookup(&node2state, out_mem))
                continue;
            const state_t** from = node2state_lookup(&node2state, in_mem);
            const state_t* to = NULL;
            switch (node->tag) {
                case NODE_LOAD:
                    {
                        // Information on the previous state is required at this point
                        if (!from)
                            continue;
                        const node_t* alloc  = find_alloc(node->ops[1]);
                        const state_t* state = alloc ? find_state(&node2state, in_mem, alloc) : NULL;
                        if (state) {
                            const node_t* val = load_value(mod, state->val, node->ops[1], node->dbg);
                            const node_t* ops[] = { node->ops[0], val };
                            node_replace(node, node_tuple(mod, 2, ops, node->dbg));
                            eliminated_loads++;
                        }
                        to = *from;
                    }
                    break;
                case NODE_STORE:
                    {
                        const node_t* alloc = find_alloc(node->ops[1]);
                        // The pointer is not an allocation
                        if (!alloc) {
                            // It is safe to say that the allocs are unchanged
                            if (from) {
                                to = *from;
                                break;
                            }
                            // No progress can be made without any previous state
                            continue;
                        }
                        // If the store overrides the alloc's value completely, no need for previous state
                        if (node->ops[1] == alloc) {
                            state_t* store = mpool_alloc(&state_pool, sizeof(state_t));
                            store->alloc = alloc;
                            store->val = node->ops[2];
                            to = store;
                        } else {
                            // Partially update the stored value in the alloc
                            const state_t* state = find_state(&node2state, in_mem, alloc);
                            if (state) {
                                const node_t* val = store_value(mod, state->val, node->ops[1], node->ops[2], node->dbg);
                                state_t* store = mpool_alloc(&state_pool, sizeof(state_t));
                                store->alloc = alloc;
                                store->val = val;
                                to = store;
                            }
                        }
                    }
                    break;
                case NODE_DEALLOC:
                    {
                        state_t* dealloc = mpool_alloc(&state_pool, sizeof(state_t));
                        dealloc->alloc = node->ops[1];
                        dealloc->val = node_undef(mod, node->ops[1]->type->ops[0]);
                        to = dealloc;
                    }
                    break;
                default:
                    assert(false);
                    break;
            }
            assert(to);
            node2state_insert(&node2state, out_mem, to);
            todo = true;
        });
    }

    // Remove allocs that are no longer necessary (only stores)
    FORALL_HSET(mod->nodes, const node_t*, node, {
        if (node->tag != NODE_ALLOC)
            continue;
        const node_t* ptr = node_extract(mod, node, node_i32(mod, 1), NULL);
        if (can_promote(ptr, false)) {
            const node_t* ops[] = { node->ops[0], node_undef(mod, node->type->ops[1]) };
            node_replace(node, node_tuple(mod, 2, ops, node->dbg));
        }
    })

    mpool_destroy(state_pool);
    node2state_destroy(&node2state);
    return eliminated_loads > 0;
}
