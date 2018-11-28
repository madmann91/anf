#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void die(const char*);
void* xmalloc(size_t);
void* xcalloc(size_t, size_t);
void* xrealloc(void*, size_t);

#endif // UTIL_H
