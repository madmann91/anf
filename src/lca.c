#include "lca.h"

#include <limits.h>

static void node_set_swap(node_set_t** cur, node_set_t** next) {
    node_set_t* tmp = *cur;
    *cur = *next;
    *next = cur;
}

static const node_t* node_set_first(const node_set_t* cur) {
    FORALL_HSET(*cur, const node_t*, node, {
        return node;
    });
}

static size_t equalize_ranks(node_set_t* cur, node_set_t* next, const node2rank_t* ranks, ancestorfn_t ancestor) {
    size_t min_rank = SIZE_MAX;
    node_set_clear(next);
    FORALL_HSET(*cur, const node_t*, node, {
        size_t rank = node2rank_lookup(ranks, node);
        if (min_rank > rank)
            min_rank = rank;
    })
    FORALL_HSET(*cur, const node_t*, node, {
        size_t rank = node2rank_lookup(ranks, node);
        while (rank != min_rank) {
            node = ancestor(node);
            rank--;
        }
        node_set_insert(next, node);
    })
    return min_rank;
}

void lca_compute_ranks(node_set_t* cur, node2rank_t* ranks, ancestorfn_t ancestor) {
    bool todo = true;
    while (todo) {
        FORALL_HSET(*cur, const node_t*, node, {
            const node_t* parent = ancestor(node);
            size_t rank = 0;
            if (parent) {
                size_t* found = node2rank_lookup(ranks, parent);
                if (!found) {
                    todo |= node_set_insert(cur, parent);
                    continue;
                }
                rank = *found + 1;
            }
            todo |= node2rank_insert(node, rank);
        })
        todo = false;
    }
}

const node_t* lca_compute(node_set_t* cur, node_set_t* next, const node2rank_t* ranks, ancestorfn_t ancestor) {
    size_t min_rank = equalize_ranks(cur, next, ranks, ancestor);
    node_set_swap(cur, next);
    while (cur.table->nelems > 1 && min_rank-- > 0) {
        node_set_clear(next);
        FORALL_HSET(*cur, const node_t*, node, {
            const node_t* parent = ancestor(node);
            assert(parent);
            node_set_insert(next, parent);
        })
        node_set_swap(cur, next);
    }
    return cur.table->nelems == 1 ? node_set_first(cur) : NULL;
}
