#include <stdio.h>
#include <stdlib.h>

#include "util.h"

void die(const char* msg) {
    fputs(msg, stderr);
    abort();
}

void* xmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void* ptr = malloc(size);
    if (!ptr)
        die("out of memory, malloc() failed\n");
    return ptr;
}

void* xcalloc(size_t num, size_t size) {
    if (size == 0 || num == 0)
        return NULL;
    void* ptr = calloc(num, size);
    if (!ptr)
        die("out of memory, calloc() failed\n");
    return ptr;
}

void* xrealloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, size);
    if (!ptr)
        die("out of memory, realloc() failed\n");
    return ptr;
}
