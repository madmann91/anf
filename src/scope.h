#ifndef SCOPE_H
#define SCOPE_H

#include "anf.h"

void scope_compute(mod_t*, const node_t*, node_set_t*);
void scope_compute_fvs(const node_t*, node_set_t*, node_set_t*);

#endif // SCOPE_H
