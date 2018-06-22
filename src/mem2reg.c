#include "anf.h"
#include "adt.h"

typedef struct state_s state_t;

struct state_s {
    const node_t* alloc;
    const node_t* val;
};

HMAP_DEFAULT(node2state, const node_t*, const state_t*)
VEC(state_vec, state_t*)

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
        mem = node_in_mem(node_from_mem(mem));
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

static bool can_promote(const node_t* node, node_set_t* replaced_loads) {
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
                return replaced_loads ? node_set_lookup(replaced_loads, node) != NULL : true;
            case NODE_STORE:
                return use->index == 1;
            case NODE_OFFSET:
                break;
            default:
                return false;
        }
        if (!can_promote(use->user, replaced_loads))
            return false;
        use = use->next;
    }
    return true;
}

bool mem2reg(mod_t* mod) {
    node2state_t node2state = node2state_create(64);
    node_set_t replaced_loads = node_set_create(64);
    mpool_t* state_pool = mpool_create(4096);

    // Scan nodes to find allocs
    FORALL_HSET(mod->nodes, const node_t*, node, {
        if (node->tag != NODE_ALLOC)
            continue;
        const node_t* mem = node_extract(mod, node, node_i32(mod, 0), NULL);
        const node_t* ptr = node_extract(mod, node, node_i32(mod, 1), NULL);
        if (!can_promote(ptr, NULL))
            continue;
        state_t* state = mpool_alloc(&state_pool, sizeof(state_t));
        state->alloc = ptr;
        state->val = node_undef(mod, ptr->type->ops[0]);
        node2state_insert(&node2state, mem, state);
    });

    // Remove loads from known pointers
    bool todo = true;
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
            // If there is no state information for the source memory object, wait for the next iteration
            const state_t** found = node2state_lookup(&node2state, in_mem);
            if (!found)
                continue;
            const state_t* from = *found, *to = NULL;
            switch (node->tag) {
                case NODE_LOAD:
                    {
                        const node_t* alloc  = find_alloc(node->ops[1]);
                        const state_t* state = alloc ? find_state(&node2state, in_mem, alloc) : NULL;
                        if (state) {
                            const node_t* val = load_value(mod, state->val, node->ops[1], node->dbg);
                            const node_t* ops[] = { node->ops[0], val };
                            node_replace(node, node_tuple(mod, 2, ops, node->dbg));
                            node_set_insert(&replaced_loads, node);
                        }
                        to = from;
                    }
                    break;
                case NODE_STORE:
                    {
                        const node_t* alloc  = find_alloc(node->ops[1]);
                        const state_t* state = alloc ? find_state(&node2state, in_mem, alloc) : NULL;
                        if (state) {
                            const node_t* val = store_value(mod, state->val, node->ops[1], node->ops[2], node->dbg);
                            state_t* store = mpool_alloc(&state_pool, sizeof(state_t));
                            store->alloc = alloc;
                            store->val = val;
                            to = store;
                        } else {
                            to = from;
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
        if (can_promote(ptr, &replaced_loads)) {
            const node_t* ops[] = { node->ops[0], node_undef(mod, node->type->ops[1]) };
            node_replace(node, node_tuple(mod, 2, ops, node->dbg));
        }
    })

    mpool_destroy(state_pool);
    node2state_destroy(&node2state);
    node_set_destroy(&replaced_loads);
    return replaced_loads.table->nelems > 0;
}
