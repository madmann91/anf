#ifndef BIND_H
#define BIND_H

#include "adt.h"
#include "hash.h"
#include "ast.h"
#include "log.h"

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
    ast_t* fn;
    ast_t* loop;
    env_t* env;
    log_t* log;
};

void bind(binder_t*, ast_t*);

#endif // BIND_H
