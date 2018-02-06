#ifndef LOGIC_H
#define LOGIC_H

#include "anf.h"

void dnf_compute_ors(const node_t*, node_vec_t*);
const node_t* dnf_convert(mod_t*, const node_t*);

void cnf_compute_ands(const node_t*, node_vec_t*);
const node_t* cnf_convert(mod_t*, const node_t*);

#endif // LOGIC_H
