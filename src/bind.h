#ifndef BIND_H
#define BIND_H

#include "adt.h"
#include "hash.h"
#include "ast.h"

typedef struct env_s    env_t;
typedef struct binder_s binder_t;

static inline uint32_t hash_id(const void* ptr) {
    return hash_str(hash_init(), *(const char**)ptr);
}

static inline bool cmp_id(const void* ptr1, const void* ptr2) {
    return !strcmp(*(const char**)ptr1, *(const char**)ptr2);
}

HMAP(id2ast, const char*, ast_t*, cmp_id, hash_id)

struct env_s {
    id2ast_t  id2ast;
    env_t*    next;
    env_t*    prev;
};

struct binder_s {
    ast_t*    fn;
    ast_t*    loop;
    env_t*    env;
    size_t    errs;
    size_t    warns;
    void      (*error_fn)(binder_t*, const loc_t*, const char*);
    void      (*warn_fn)(binder_t*, const loc_t*, const char*);
    void      (*note_fn)(binder_t*, const loc_t*, const char*);
};

void bind(binder_t*, ast_t*);
void bind_error(binder_t*, const loc_t*, const char*, ...);
void bind_warn(binder_t*, const loc_t*, const char*, ...);
void bind_note(binder_t*, const loc_t*, const char*, ...);

#endif // BIND_H
