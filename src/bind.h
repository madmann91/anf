#ifndef BIND_H
#define BIND_H

#include "adt.h"
#include "hash.h"
#include "ast.h"

typedef struct binder_s binder_t;

static inline uint32_t hash_id(const void* ptr) {
    return hash_str(hash_init(), *(const char**)ptr);
}

static inline bool cmp_id(const void* ptr1, const void* ptr2) {
    return !strcmp(*(const char**)ptr1, *(const char**)ptr2);
}

HMAP(id2ast, const char*, ast_t*, cmp_id, hash_id)

struct binder_s {
    id2ast_t* id2ast;
    size_t    errs;
    void      (*error_fn)(binder_t*, const loc_t*, const char*);
};

void bind(binder_t*, ast_t*);
void bind_error(binder_t*, const loc_t*, const char*, ...);

#endif // BIND_H
