#ifndef SCOPE_H
#define SCOPE_H

#include "anf.h"

typedef struct scope_s scope_t;

struct scope_s {
    node_set_t nodes;
    const node_t* fn;
};

scope_t scope_create(mod_t*, const node_t*);
void scope_destroy(scope_t*);
void scope_dump(scope_t*);

#endif // SCOPE_H
