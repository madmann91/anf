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

    CHECK(type_i1(mod)  == type_i1(mod));
    CHECK(type_i8(mod)  == type_i8(mod));
    CHECK(type_i16(mod) == type_i16(mod));
    CHECK(type_i32(mod) == type_i32(mod));
    CHECK(type_i64(mod) == type_i64(mod));
    CHECK(type_u8(mod)  == type_u8(mod));
    CHECK(type_u16(mod) == type_u16(mod));
    CHECK(type_u32(mod) == type_u32(mod));
    CHECK(type_u64(mod) == type_u64(mod));
    CHECK(type_f32(mod) == type_f32(mod));
    CHECK(type_f64(mod) == type_f64(mod));
    CHECK(type_fn(mod, type_i32(mod), type_f32(mod)) ==
          type_fn(mod, type_i32(mod), type_f32(mod)));
    ops[0] = type_i64(mod);
    ops[1] = type_i32(mod);
    ops[2] = type_i16(mod);
    CHECK(type_tuple(mod, 1, ops) == ops[0]);
    CHECK(type_tuple(mod, 0, ops) == type_tuple(mod, 0, ops));
    CHECK(type_tuple(mod, 1, ops) == type_tuple(mod, 1, ops));
    CHECK(type_tuple(mod, 2, ops) == type_tuple(mod, 2, ops));
    CHECK(type_tuple(mod, 3, ops) == type_tuple(mod, 3, ops));

    CHECK(type_bitwidth(type_i1(mod))  == 1);
    CHECK(type_bitwidth(type_i8(mod))  == 8);
    CHECK(type_bitwidth(type_i16(mod)) == 16);
    CHECK(type_bitwidth(type_i32(mod)) == 32);
    CHECK(type_bitwidth(type_i64(mod)) == 64);
    CHECK(type_bitwidth(type_u8(mod))  == 8);
    CHECK(type_bitwidth(type_u16(mod)) == 16);
    CHECK(type_bitwidth(type_u32(mod)) == 32);
    CHECK(type_bitwidth(type_u64(mod)) == 64);
    CHECK(type_bitwidth(type_f32(mod)) == 32);
    CHECK(type_bitwidth(type_f64(mod)) == 64);

    CHECK(type_is_prim(type_i1(mod)));
    CHECK(type_is_prim(type_i8(mod)));
    CHECK(type_is_prim(type_i16(mod)));
    CHECK(type_is_prim(type_i32(mod)));
    CHECK(type_is_prim(type_i64(mod)));
    CHECK(type_is_prim(type_u8(mod)));
    CHECK(type_is_prim(type_u16(mod)));
    CHECK(type_is_prim(type_u32(mod)));
    CHECK(type_is_prim(type_u64(mod)));
    CHECK(type_is_prim(type_f32(mod)));
    CHECK(type_is_prim(type_f64(mod)));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_literals(void) {
    mod_t* mod = mod_create();

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    CHECK(node_i1(mod, true)  == node_i1(mod, true));
    CHECK(node_i1(mod, false) == node_i1(mod, false));
    CHECK(node_i1(mod, true)  != node_i1(mod, false));
    for (uint8_t i = 0;; ++i) {
        CHECK(node_i8(mod, i) != node_u8(mod, (uint8_t)i));
        CHECK(node_i8(mod, i) == node_i8(mod, i));
        CHECK(node_u8(mod, (uint8_t)i) == node_u8(mod, (uint8_t)i));
        if (i == 255) break;
    }
    CHECK(node_i16(mod, 0) == node_i16(mod, 0));
    CHECK(node_i32(mod, 0) == node_i32(mod, 0));
    CHECK(node_i64(mod, 0) == node_i64(mod, 0));
    CHECK(node_u16(mod, 0) == node_u16(mod, 0));
    CHECK(node_u32(mod, 0) == node_u32(mod, 0));
    CHECK(node_u64(mod, 0) == node_u64(mod, 0));

    CHECK(node_i32(mod, 0) != node_i64(mod, 0));
    CHECK(node_i16(mod, 0) != node_i32(mod, 0));
    CHECK(node_i64(mod, 0) != node_i16(mod, 0));

    CHECK(node_u32(mod, 0) != node_u64(mod, 0));
    CHECK(node_u16(mod, 0) != node_u32(mod, 0));
    CHECK(node_u64(mod, 0) != node_u16(mod, 0));

    CHECK(node_i16(mod, 0) != node_u16(mod, 0));
    CHECK(node_i32(mod, 0) != node_u32(mod, 0));
    CHECK(node_i64(mod, 0) != node_u64(mod, 0));

    CHECK(node_f32(mod, 1.0f) == node_f32(mod,  1.0f));
    CHECK(node_f32(mod, 0.0f) == node_f32(mod,  0.0f));
    CHECK(node_f32(mod, 0.0f) != node_f32(mod, -0.0f));

    CHECK(node_f64(mod, 1.0) == node_f64(mod,  1.0));
    CHECK(node_f64(mod, 0.0) == node_f64(mod,  0.0));
    CHECK(node_f64(mod, 0.0) != node_f64(mod, -0.0));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_tuples(void) {
    mod_t* mod = mod_create();
    const node_t* ops1[3];
    const node_t* ops2[3];
    const node_t* tuple1;
    const node_t* tuple2;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    ops1[0] = node_i32(mod, 1);
    ops1[1] = node_i32(mod, 2);
    ops1[2] = node_i32(mod, 3);
    tuple1 = node_tuple(mod, 3, ops1, NULL);
    ops2[0] = node_i32(mod, 4);
    ops2[1] = node_i32(mod, 5);
    ops2[2] = node_i32(mod, 6);
    tuple2 = node_tuple(mod, 3, ops2, NULL);

    CHECK(node_tuple(mod, 1, ops1, NULL) == ops1[0]);
    CHECK(node_tuple(mod, 0, ops1, NULL) == node_tuple(mod, 0, NULL, NULL));
    CHECK(node_tuple(mod, 0, ops1, NULL) == node_tuple(mod, 0, ops1, NULL));
    CHECK(node_tuple(mod, 1, ops1, NULL) == node_tuple(mod, 1, ops1, NULL));
    CHECK(node_tuple(mod, 2, ops1, NULL) == node_tuple(mod, 2, ops1, NULL));
    CHECK(node_tuple(mod, 3, ops1, NULL) == node_tuple(mod, 3, ops1, NULL));

    CHECK(node_extract(mod, tuple1, node_u32(mod, 0), NULL) == ops1[0]);
    CHECK(node_extract(mod, tuple1, node_u32(mod, 1), NULL) == ops1[1]);
    CHECK(node_extract(mod, tuple1, node_u32(mod, 2), NULL) == ops1[2]);

    CHECK(node_extract(mod, tuple2, node_u32(mod, 0), NULL) == ops2[0]);
    CHECK(node_extract(mod, tuple2, node_u32(mod, 1), NULL) == ops2[1]);
    CHECK(node_extract(mod, tuple2, node_u32(mod, 2), NULL) == ops2[2]);

    CHECK(
        node_insert(mod,
            node_insert(mod,
                node_insert(mod,
                        tuple1,
                    node_u32(mod, 0), ops2[0], NULL),
                node_u32(mod, 1), ops2[1], NULL),
            node_u32(mod, 2), ops2[2], NULL)
        == tuple2);

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_select(void) {
    mod_t* mod = mod_create();

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    CHECK(node_select(mod, node_i1(mod, true),  node_i32(mod, 32), node_i32(mod, 64), NULL) == node_i32(mod, 32));
    CHECK(node_select(mod, node_i1(mod, false), node_i32(mod, 32), node_i32(mod, 64), NULL) == node_i32(mod, 64));
    CHECK(node_select(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 32), NULL) == node_i32(mod, 32));
    CHECK(node_select(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 64), NULL) ==
          node_select(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 64), NULL));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_bitcast(void) {
    mod_t* mod = mod_create();

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    CHECK(node_bitcast(mod, node_u16(mod, 32), type_i16(mod), NULL) == node_i16(mod, 32));
    CHECK(node_bitcast(mod, node_u32(mod, 32), type_i32(mod), NULL) == node_i32(mod, 32));
    CHECK(node_bitcast(mod, node_u64(mod, 32), type_i64(mod), NULL) == node_i64(mod, 32));

    CHECK(node_bitcast(mod, node_f32(mod,  0.0f), type_i32(mod), NULL) == node_i32(mod, 0));
    CHECK(node_bitcast(mod, node_f32(mod, -0.0f), type_u32(mod), NULL) == node_u32(mod, 0x80000000u));

    CHECK(node_bitcast(mod, node_undef(mod, type_i32(mod)), type_f32(mod), NULL) == node_undef(mod, type_f32(mod)));

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
        {"htable",   test_htable},
        {"mpool",    test_mpool},
        {"types",    test_types},
        {"literals", test_literals},
        {"tuples",   test_tuples},
        {"select",   test_select},
        {"bitcast",  test_bitcast}
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
