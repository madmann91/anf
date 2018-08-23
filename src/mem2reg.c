#include "anf.h"
#include "adt.h"
#include "scope.h"

typedef struct state_s state_t;

struct state_s {
    node_set_t loads;
    node_set_t stores;
};

HMAP_DEFAULT(fn2state, const fn_t*, state_t*)
VEC(state_vec, state_t*)

static const node_t* find_alloc(const node_set_t* allocs, const node_t* ptr, size_t* depth) {
    // Given a pointer, try to find the original alloc instruction
    if (ptr->tag == NODE_EXTRACT && ptr->ops[0]->tag == NODE_ALLOC) {
        return node_set_lookup(allocs, ptr->ops[0]) ? ptr : NULL;
    } else if (ptr->tag == NODE_OFFSET) {
        (*depth)++;
        return find_alloc(allocs, ptr->ops[0], depth);
    } else {
        return NULL;
    }
}

static bool can_promote_or_remove(const node_t* node, bool only_promote) {
    // Promotion is possible if all uses are (transitively):
    // load, store (as pointer operand), dealloc and offset.
    // Once promotion has been performed, removal is possible
    // if all users that are loads have been replaced.
    use_t* use = node->uses;    
    while (use) {
        switch (use->user->tag) {
            case NODE_DEALLOC:
                return true;
            case NODE_LOAD:
                return only_promote || node->rep != NULL;
            case NODE_STORE:
                return use->index == 1;
            case NODE_OFFSET:
                break;
            default:
                return false;
        }
        if (!can_promote_or_remove(use->user, only_promote))
            return false;
        use = use->next;
    }
    return true;
}

static const node_t* extract_offsets(mod_t* mod, const node_t* value, const node_t* ptr, size_t depth, const dbg_t* dbg) {
    // Recursively extract values from an aggregate to match the offset of a pointer
    // Example:
    // p1 = offset p, 5
    // p2 = offset p1, 3
    // p3 = offset p2, 9
    // extract_offsets with ptr = p2, depth = 3:
    // extract(extract(extract(value, 5), 3), 9)
    return depth == 0 ? value : node_extract(mod, extract_offsets(mod, value, ptr->ops[0], depth - 1, dbg), ptr->ops[1], dbg);
}

static const node_t* insert_offsets(mod_t* mod, const node_t* value, const node_t* elem, const node_t* ptr, size_t depth, const dbg_t* dbg) {
    // Recursively inserts values inside an aggregate to match the offset of a pointer
    // Example:
    // p1 = offset p, 5
    // p2 = offset p1, 3
    // p3 = offset p2, 9
    // insert_offsets with ptr = p2, depth = 3:
    // insert(value, 5, insert(extract(value, 5), 3, insert(extract(extract(value, 5), 3), 9, elem))))
    if (depth == 0)
        return elem;
    const node_t* extracts = extract_offsets(mod, value, ptr->ops[0], depth - 1, dbg);
    return node_insert(mod, extracts, ptr->ops[1], insert_offsets(mod, value, elem, ptr->ops[0], depth - 1, dbg), dbg);
}

static const node_t* try_resolve_load(mod_t* mod, const node_set_t* allocs, const node_t* node, const node_t* load, const node_t* alloc, size_t depth) {
    // Try to find a value for a load instruction by following the thread of memory objects in reverse order
    while (true) {
        if (!node_has_mem(node))
            return NULL;

        const node_t* parent = node_from_mem(node_in_mem(node));
        if (parent->tag == NODE_LOAD || parent->tag == NODE_STORE) {
            size_t parent_depth = 0;
            const node_t* parent_alloc = find_alloc(allocs, parent->ops[1], &parent_depth);

            if (alloc == parent_alloc) {
                if (depth >= parent_depth) {
                    const node_t* value = parent->tag == NODE_LOAD ? node_extract(mod, parent, node_i32(mod, 1), NULL) : parent->ops[2];
                    return extract_offsets(mod, value, load->ops[1], depth - parent_depth, load->dbg);
                } else if (parent->tag == NODE_STORE) {
                    // The value is only partially specified, we need to recurse
                    const node_t* value = try_resolve_load(mod, allocs, parent, load, alloc, depth);
                    if (!value)
                        return NULL;
                    return insert_offsets(mod, value, node->ops[2], parent->ops[1], parent_depth - depth, load->dbg);
                }
            }
        } else if (parent->tag == NODE_ALLOC) {
            assert(alloc->tag == NODE_EXTRACT);
            if (alloc->ops[0] == parent)
                return node_undef(mod, load->type->ops[1]);
        } else if (parent->tag == NODE_DEALLOC) {
            if (parent->ops[1] == alloc)
                return node_undef(mod, load->type->ops[1]);
        }

        node = parent;
    }
}

static size_t find_loads_stores(mod_t* mod, const node_set_t* allocs, const node_t* mem, node_set_t* loads, node_set_t* stores) {
    // Follow the thread of memory objects and record all loads/stores along the way
    assert(mem->type->tag == TYPE_MEM);
    size_t elim_loads = 0;
    const use_t* use = mem->uses;
    while (use) {
        const node_t* node = use->user;
        use = use->next;
        if (!node_has_mem(node))
            continue;

        if (node->tag == NODE_LOAD || node->tag == NODE_STORE) {
            size_t depth = 0;
            const node_t* alloc = find_alloc(allocs, node->ops[1], &depth);
            if (alloc) {
                if (node->tag == NODE_LOAD) {
                    // Try to find a value for the load before adding it to the set
                    const node_t* load_value = try_resolve_load(mod, allocs, node, node, alloc, depth);
                    if (!load_value)
                        node_set_insert(loads, node);
                    else {
                        elim_loads++;
                        node_replace(node, node_tuple_args(mod, 2, node->dbg, node->ops[0], load_value));
                    }
                } else {
                    node_set_insert(stores, node);
                }
            }
        }

        // Do not recurse if there is only one use
        if (!use) {
            mem = node_out_mem(mod, node);
            use = mem->uses;
        } else {
            find_loads_stores(mod, allocs, node_out_mem(mod, node), loads, stores);
        }
    }
    return elim_loads;
}

static void find_mem_params(mod_t* mod, const node_t* param, node_vec_t* mems) {
    // Recursively extract all memory objects from a function parameter
    if (param->type->tag == TYPE_TUPLE) {
        for (size_t i = 0; i < param->type->nops; ++i)
            find_mem_params(mod, node_extract(mod, param, node_i32(mod, i), NULL), mems);
    } else if (param->type->tag == TYPE_MEM) {
        node_vec_push(mems, param);
    }
}

bool mem2reg(mod_t* mod) {
    node_set_t allocs   = node_set_create();
    state_vec_t states  = state_vec_create(); 
    fn2state_t fn2state = fn2state_create();
    mpool_t* pool       = mpool_create();
    size_t elim_loads = 0;

    // Gather all allocations amenable to promotion
    FORALL_HSET(mod->nodes, const node_t*, node, {
        if (node->tag == NODE_ALLOC) {
            const node_t* ptr = node_extract(mod, node, node_i32(mod, 1), NULL);
            if (can_promote_or_remove(ptr, true))
                node_set_insert(&allocs, node);
        }
    })

    // Create (loads/stores) record for every function
    node_vec_t mems = node_vec_create();
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        state_t* state = mpool_alloc(&pool, sizeof(state_t));
        state->loads  = node_set_create();
        state->stores = node_set_create();
        state_vec_push(&states, state);
        fn2state_insert(&fn2state, fn, state);

        mems.nelems = 0;
        find_mem_params(mod, node_param(mod, fn, NULL), &mems);
        FORALL_VEC(mems, const node_t*, mem, {
            elim_loads += find_loads_stores(mod, &allocs, mem, &state->loads, &state->stores);
        })
    })
    node_vec_destroy(&mems);

    FORALL_VEC(states, state_t*, state, {
        node_set_destroy(&state->loads);
        node_set_destroy(&state->stores);
    })

    fn2state_destroy(&fn2state);
    state_vec_destroy(&states);
    node_set_destroy(&allocs);
    mpool_destroy(pool);
    return elim_loads > 0;
}
