#ifndef MPOOL_H
#define MPOOL_H

#include <stddef.h>

typedef struct mpool_s mpool_t;

struct mpool_s {
    void*  begin;
    size_t size;
    size_t cap;
    mpool_t* next;
};

mpool_t* mpool_create_with_cap(size_t);
mpool_t* mpool_create(void);
void mpool_destroy(mpool_t*);
void* mpool_alloc(mpool_t**, size_t);

#endif // MPOOL_H
