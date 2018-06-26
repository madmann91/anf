#ifndef LCA_H
#define LCA_H

#include "anf.h"
#include "adt.h"

typedef const node_t* (*ancestorfn_t)(const node_t*);

HMAP_DEFAULT(node2rank, const node_t*, size_t)

void lca_compute_ranks(node_set_t*, node2rank_t*, ancestorfn_t);
const node_t* lca_compute(node_set_t*, node_set_t*, const node2rank_t*, ancestorfn_t);

#endif // LCA_H
