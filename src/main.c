#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "htable.h"

bool cmp(const void* a, const void* b) { return *(uint32_t*)a == *(uint32_t*)b; }
uint32_t hash(const void* elem) { return *(uint32_t*)elem % 23; }

int main(int argc, char** argv) {
    size_t N = 4000;
    size_t inc[3] = {3, 5, 7};
    uint32_t* values = malloc(sizeof(uint32_t) * N);
    for (size_t i = 0, j = 0; i < N; ++i, j += inc[j%3]) {
        values[i] = j;
    }
    htable_t* table1 = htable_create(sizeof(uint32_t), 16, cmp, hash);
    htable_t* table2 = htable_create(sizeof(uint32_t), 16, cmp, hash);
    for (size_t i = 0; i < N; ++i) {
        assert(htable_insert(table1, &values[i]));
    }
    for (size_t i = N - 1; i >= N / 2; --i) {
        assert(htable_remove(table1, &values[i]));
    }
    for (size_t i = N / 2; i < N; ++i) {
        assert(htable_lookup(table1, &values[i]) == INVALID_INDEX);
    }
    for (size_t i = 0; i < N / 2; ++i) {
        assert(htable_lookup(table1, &values[i]) != INVALID_INDEX);
    }
    for (size_t i = 0; i < table1->cap; ++i) {
        if (!(table1->hashes[i] & OCCUPIED_HASH_MASK))
            continue;
        assert(htable_insert(table2, ((uint32_t*)table1->elems) + i));
    }
    for (size_t i = 0; i < N / 2; ++i) {
        assert(htable_lookup(table2, &values[i]) != INVALID_INDEX);
    }
    assert(table1->nelems == N / 2);
    assert(table2->nelems == N / 2);
    httable_destroy(table1);
    httable_destroy(table2);
    free(values);
    return 0;
}