#include <stdio.h>

#include "mod.h"
#include "node.h"
#include "type.h"
#include "adt.h"
#include "util.h"
#include "mpool.h"
#include "scope.h"

bool node_cmp(const void* ptr1, const void* ptr2) {
    const node_t* node1 = *(const node_t**)ptr1;
    const node_t* node2 = *(const node_t**)ptr2;
    if (node1->tag  != node2->tag  ||
        node1->nops != node2->nops ||
        node1->type != node2->type ||
        memcmp(&node1->data, &node2->data, node1->dsize))
        return false;
    for (size_t i = 0; i < node1->nops; ++i) {
        if (node1->ops[i] != node2->ops[i])
            return false;
    }
    return true;
}

uint32_t node_hash(const void* ptr) {
    const node_t* node = *(const node_t**)ptr;
    uint32_t h = hash_init();
    h = hash_uint32(h, node->tag);
    h = hash_ptr(h, node->type);
    for (size_t i = 0; i < node->nops; ++i)
        h = hash_ptr(h, node->ops[i]);
    for (size_t i = 0; i < node->dsize; ++i)
        h = hash_uint8(h, ((uint8_t*)&node->data)[i]);
    return h;
}

bool type_cmp(const void* ptr1, const void* ptr2) {
    const type_t* type1 = *(const type_t**)ptr1;
    const type_t* type2 = *(const type_t**)ptr2;
    if (type1->tag  != type2->tag ||
        type1->nops != type2->nops ||
        memcmp(&type1->data, &type2->data, type1->dsize))
        return false;
    for (size_t i = 0; i < type1->nops; ++i) {
        if (type1->ops[i] != type2->ops[i])
            return false;
    }
    return true;
}

uint32_t type_hash(const void* ptr) {
    const type_t* type = *(const type_t**)ptr;
    uint32_t h = hash_init();
    h = hash_uint32(h, type->tag);
    for (size_t i = 0; i < type->nops; ++i)
        h = hash_ptr(h, type->ops[i]);
    for (size_t i = 0; i < type->dsize; ++i)
        h = hash_uint8(h, ((uint8_t*)&type->data)[i]);
    return h;
}

mod_t* mod_create(void) {
    mod_t* mod = xmalloc(sizeof(mod_t));
    mod->pool  = mpool_create();
    mod->fns   = node_vec_create();
    mod->nodes = internal_node_set_create();
    mod->types = internal_type_set_create();
    return mod;
}

void mod_destroy(mod_t* mod) {
    mpool_destroy(mod->pool);
    node_vec_destroy(&mod->fns);
    internal_node_set_destroy(&mod->nodes);
    internal_type_set_destroy(&mod->types);
    free(mod);
}

void mod_opt(mod_t** mod) {
    // TODO
    (void)mod;
}

void mod_dump(mod_t* mod) {
    scope_t scope = { .entry = NULL, .nodes = node_set_create() };
    node_set_t seen = node_set_create();
    node_vec_t stack = node_vec_create();
    FORALL_FNS(mod, fn, {
        node_set_clear(&scope.nodes);
        node_set_clear(&seen);
        node_vec_clear(&stack);

        scope.entry = fn;
        scope_compute(mod, &scope);

        node_dump(fn);
        node_vec_push(&stack, fn->ops[0]);
        // Post order walk over the body of the function
        while (stack.nelems > 0) {
            const node_t* node = stack.elems[stack.nelems - 1];
            // Do not print nodes outside the scope except TAPPs
            if (node->tag != NODE_TAPP &&
                (node->nops == 0 || node->tag == NODE_FN || !node_set_lookup(&scope.nodes, node)))
                goto done;
            bool all_seen = true;
            for (size_t i = 0; i < node->nops; ++i) {
                if (!node_set_lookup(&seen, node->ops[i])) {
                    node_vec_push(&stack, node->ops[i]);
                    all_seen = false;
                }
            }
            if (all_seen) {
                printf("    ");
                node_dump(node);
                goto done;
            }
            continue;
        done:
            node_set_insert(&seen, node);
            node_vec_pop(&stack);
        }
        printf("\n");
    });
    node_vec_destroy(&stack);
    node_set_destroy(&seen);
    node_set_destroy(&scope.nodes);
}

static inline void register_use(mod_t* mod, size_t index, const node_t* used, const node_t* user) {
    use_t* use = mpool_alloc(&mod->pool, sizeof(use_t));
    use->index = index;
    use->user  = user;
    use->next  = used->uses;
    ((node_t*)used)->uses = use;
}

static inline void unregister_use(size_t index, const node_t* used, const node_t* user) {
    use_t* use = used->uses;
    use_t** prev = &((node_t*)used)->uses;
    while (use) {
        if (use->index == index && use->user  == user)
            break;
        prev = &use->next;
        use  = use->next;
    }
    assert(use);
    *prev = use->next;
}

void node_bind(mod_t* mod, const node_t* node, size_t i, const node_t* op) {
    assert(i < node->nops && node->ops[i]);
    unregister_use(i, node->ops[i], node);
    node->ops[i] = op;
    register_use(mod, i, node->ops[i], node);
}

const type_t* mod_insert_type(mod_t* mod, const type_t* type) {
    const type_t** lookup = internal_type_set_lookup(&mod->types, type);
    if (lookup)
        return *lookup;

    type_t* type_ptr = mpool_alloc(&mod->pool, sizeof(type_t));
    memcpy(type_ptr, type, sizeof(type_t));
    if (type->nops > 0) {
        const type_t** type_ops = mpool_alloc(&mod->pool, sizeof(type_t*) * type->nops);
        for (size_t i = 0; i < type->nops; ++i) type_ops[i] = type->ops[i];
        type_ptr->ops = type_ops;
    }

    bool success = internal_type_set_insert(&mod->types, type_ptr);
    assert(success), (void)success;
    return type_ptr;
}

const node_t* mod_insert_node(mod_t* mod, const node_t* node) {
    // Functions are not hashed
    if (node->tag != NODE_FN) {
        const node_t** lookup = internal_node_set_lookup(&mod->nodes, node);
        if (lookup) {
            if (node->dbg && !(*lookup)->dbg)
                (*(node_t**)lookup)->dbg = node->dbg;
            return *lookup;
        }
    }

    node_t* node_ptr = mpool_alloc(&mod->pool, sizeof(node_t));
    memcpy(node_ptr, node, sizeof(node_t));
    if (node->nops > 0) {
        const node_t** node_ops = mpool_alloc(&mod->pool, sizeof(node_t*) * node->nops);
        for (size_t i = 0; i < node->nops; ++i) {
            register_use(mod, i, node->ops[i], node_ptr);
            node_ops[i] = node->ops[i];
        }
        node_ptr->ops = node_ops;
    }

    if (node->tag != NODE_FN) {
        bool success = internal_node_set_insert(&mod->nodes, node_ptr);
        assert(success), (void)success;
    } else {
        node_vec_push(&mod->fns, node_ptr);
    }
    return node_ptr;
}
