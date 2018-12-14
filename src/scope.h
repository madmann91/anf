#ifndef SCOPE_H
#define SCOPE_H

#include "mod.h"

typedef struct scope_s scope_t;

struct scope_s {
    const node_t* entry;
    node_set_t nodes;
};

void scope_compute(mod_t*, scope_t*);
void scope_compute_fvs(const scope_t*, node_set_t*);

#endif // SCOPE_H
