#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "htable.h"
#include "mpool.h"
#include "anf.h"

#define CHECK(expr) check(env, expr, #expr, __FILE__, __LINE__)
void check(jmp_buf env, bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        fprintf(stderr, "check failed in %s(%d): %s\n", file, line, expr);
        longjmp(env, 1);
    }
}

bool cmp(const void* a, const void* b) { return *(uint32_t*)a == *(uint32_t*)b; }
uint32_t hash(const void* elem) { return *(uint32_t*)elem % 23; }

bool test_htable(void) {
    size_t N = 4000;
    size_t inc[3] = {3, 5, 7};
    uint32_t* values = malloc(sizeof(uint32_t) * N);
    for (size_t i = 0, j = 0; i < N; ++i, j += inc[j%3]) {
        values[i] = j;
    }
    htable_t* table1 = htable_create(sizeof(uint32_t), 16, cmp, hash);
    htable_t* table2 = htable_create(sizeof(uint32_t), 16, cmp, hash);

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    for (size_t i = 0; i < N; ++i) {
        CHECK(htable_insert(table1, &values[i]));
    }
    for (size_t i = N - 1; i >= N / 2; --i) {
        CHECK(htable_remove(table1, &values[i]));
    }
    for (size_t i = N / 2; i < N; ++i) {
        CHECK(htable_lookup(table1, &values[i]) == INVALID_INDEX);
    }
    for (size_t i = 0; i < N / 2; ++i) {
        CHECK(htable_lookup(table1, &values[i]) != INVALID_INDEX);
    }
    for (size_t i = 0; i < table1->cap; ++i) {
        if (!(table1->hashes[i] & OCCUPIED_HASH_MASK))
            continue;
        CHECK(htable_insert(table2, ((uint32_t*)table1->elems) + i));
    }
    for (size_t i = 0; i < N / 2; ++i) {
        CHECK(htable_lookup(table2, &values[i]) != INVALID_INDEX);
    }
    CHECK(table1->nelems == N / 2);
    CHECK(table2->nelems == N / 2);

cleanup:
    htable_destroy(table1);
    htable_destroy(table2);
    free(values);
    return status == 0;
}

bool test_mpool(void) {
    mpool_t* pool = mpool_create(1024 * 1024);

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    for (size_t i = 0; i < 1024; ++i)
        mpool_alloc(&pool, 1024);
    CHECK(pool->next == NULL);
    CHECK(pool->cap == pool->size);
    mpool_alloc(&pool, 1024 * 1024 * 2);
    CHECK(pool->cap == 1024 * 1024 * 2);
    CHECK(pool->next != NULL);

cleanup:
    mpool_destroy(pool);
    return status == 0;
}

bool test_types(void) {
    mod_t* mod = mod_create();
    const type_t* ops[3];

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    CHECK(type_i(mod, 1)  == type_i(mod, 1));
    CHECK(type_i(mod, 8)  == type_i(mod, 8));
    CHECK(type_i(mod, 16) == type_i(mod, 16));
    CHECK(type_i(mod, 32) == type_i(mod, 32));
    CHECK(type_i(mod, 64) == type_i(mod, 64));
    CHECK(type_u(mod, 8)  == type_u(mod, 8));
    CHECK(type_u(mod, 16) == type_u(mod, 16));
    CHECK(type_u(mod, 32) == type_u(mod, 32));
    CHECK(type_u(mod, 64) == type_u(mod, 64));
    CHECK(type_f(mod, 32) == type_f(mod, 32));
    CHECK(type_f(mod, 64) == type_f(mod, 64));
    CHECK(type_fn(mod, type_i(mod, 32), type_f(mod, 32)) ==
          type_fn(mod, type_i(mod, 32), type_f(mod, 32)));
    ops[0] = type_i(mod, 64);
    ops[1] = type_i(mod, 32);
    ops[2] = type_i(mod, 16);
    CHECK(type_tuple(mod, 1, ops) == ops[0]);
    CHECK(type_tuple(mod, 0, ops) == type_tuple(mod, 0, ops));
    CHECK(type_tuple(mod, 1, ops) == type_tuple(mod, 1, ops));
    CHECK(type_tuple(mod, 2, ops) == type_tuple(mod, 2, ops));
    CHECK(type_tuple(mod, 3, ops) == type_tuple(mod, 3, ops));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

typedef struct {
    const char* name;
    bool (*test_fn)(void);
} test_t;

void usage(void) {
    printf("usage: test [options]\n"
           "  -h           show this message\n"
           "  -t [module]  test the given module\n");
}

size_t find_test(const char* name, const test_t* tests, size_t ntests) {
    for (size_t i = 0; i < ntests; ++i) {
        if (!strcmp(tests[i].name, name)) return i;
    }
    return ntests;
}

void run_test(const test_t* test) {
    bool res = test->test_fn();
    printf("- %s: %s\n", test->name, res ? "OK" : "FAIL");
}

int main(int argc, char** argv) {
    test_t tests[] = {
        {"htable", test_htable},
        {"mpool",  test_mpool},
        {"types",  test_mpool}
    };
    const size_t ntests = sizeof(tests) / sizeof(test_t);
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (!strcmp(argv[i], "-h")) {
                usage();
                return 0;
            } else if (!strcmp(argv[i], "-t")) {
                if (i == argc - 1) {
                    fprintf(stderr, "missing argument for -t\n");
                    return 1;
                }
                const char* name = argv[++i];
                size_t j = find_test(name, tests, ntests);
                if (j == ntests) {
                    fprintf(stderr, "invalid module name: %s\n", name);
                    return 1;
                }
                run_test(&tests[j]);
            } else {
                fprintf(stderr, "unknown option: %s\n", argv[i]);
                return 1;
            }
        }
    } else {
        printf("running all tests\n");
        for (size_t i = 0; i < ntests; ++i)
            run_test(&tests[i]);
    }
    return 0;
}
