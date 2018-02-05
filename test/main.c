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

    for (size_t i = 0; i < N; ++i)
        CHECK(htable_insert(table1, &values[i]));
    for (size_t i = N - 1; i >= N / 2; --i)
        CHECK(htable_remove(table1, &values[i]));
    for (size_t i = N / 2; i < N; ++i)
        CHECK(htable_lookup(table1, &values[i]) == INVALID_INDEX);
    for (size_t i = 0; i < N / 2; ++i)
        CHECK(htable_lookup(table1, &values[i]) != INVALID_INDEX);
    for (size_t i = 0; i < table1->cap; ++i) {
        if (!(table1->hashes[i] & OCCUPIED_HASH_MASK))
            continue;
        CHECK(htable_insert(table2, ((uint32_t*)table1->elems) + i));
    }
    for (size_t i = 0; i < N / 2; ++i)
        CHECK(htable_lookup(table2, &values[i]) != INVALID_INDEX);

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
    ops1[1] = node_i8(mod, 2);
    ops1[2] = node_f32(mod, 3.0f);
    tuple1 = node_tuple(mod, 3, ops1, NULL);
    ops2[0] = node_i32(mod, 4);
    ops2[1] = node_i8(mod, 5);
    ops2[2] = node_f32(mod, 6.0f);
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

bool test_arrays(void) {
    mod_t* mod = mod_create();
    const node_t* elems1[3];
    const node_t* elems2[3];
    const node_t* array1;
    const node_t* array2;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    elems1[0] = node_i32(mod, 1);
    elems1[1] = node_i32(mod, 2);
    elems1[2] = node_i32(mod, 3);
    array1 = node_array(mod, 3, elems1, NULL);
    elems2[0] = node_i32(mod, 4);
    elems2[1] = node_i32(mod, 5);
    elems2[2] = node_i32(mod, 6);
    array2 = node_array(mod, 3, elems2, NULL);

    CHECK(node_extract(mod, array1, node_i32(mod, 0), NULL) == elems1[0]);
    CHECK(node_extract(mod, array1, node_i32(mod, 1), NULL) == elems1[1]);
    CHECK(node_extract(mod, array1, node_i32(mod, 2), NULL) == elems1[2]);

    CHECK(node_extract(mod, array2, node_i32(mod, 0), NULL) == elems2[0]);
    CHECK(node_extract(mod, array2, node_i32(mod, 1), NULL) == elems2[1]);
    CHECK(node_extract(mod, array2, node_i32(mod, 2), NULL) == elems2[2]);

    CHECK(
        node_insert(mod,
            node_insert(mod,
                node_insert(mod,
                        array1,
                    node_u32(mod, 0), elems2[0], NULL),
                node_u32(mod, 1), elems2[1], NULL),
            node_u32(mod, 2), elems2[2], NULL)
        == array2);

    CHECK(node_extract(mod, node_undef(mod, array1->type), node_i32(mod, 0), NULL) == node_undef(mod, array1->type->ops[0]));
    CHECK(node_insert(mod, node_undef(mod, array1->type), node_i32(mod, 0), node_i32(mod, 0), NULL) == node_undef(mod, array1->type));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_if(void) {
    mod_t* mod = mod_create();

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    CHECK(node_if(mod, node_i1(mod, true),  node_i32(mod, 32), node_i32(mod, 64), NULL) == node_i32(mod, 32));
    CHECK(node_if(mod, node_i1(mod, false), node_i32(mod, 32), node_i32(mod, 64), NULL) == node_i32(mod, 64));
    CHECK(node_if(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 32), NULL) == node_i32(mod, 32));
    CHECK(node_if(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 64), NULL) ==
          node_if(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 64), NULL));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_bitcast(void) {
    mod_t* mod = mod_create();
    const node_t* fn;
    const node_t* param;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    fn = node_fn(mod, type_fn(mod, type_i32(mod), type_i32(mod)), NULL);
    param = node_param(mod, fn, NULL);

    CHECK(
        node_bitcast(mod,
            node_bitcast(mod,
                node_bitcast(mod, param, type_f32(mod), NULL),
                type_u32(mod), NULL),
            type_i32(mod), NULL)
        == param);

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

bool test_binops(void) {
    mod_t* mod = mod_create();
    const node_t* fn;
    const node_t* param;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    fn = node_fn(mod, type_fn(mod, type_i32(mod), type_i32(mod)), NULL);
    param = node_param(mod, fn, NULL);

    CHECK(node_add(mod, param, node_mul(mod, node_i32(mod, 5), param, NULL), NULL) == node_mul(mod, node_i32(mod, 6), param, NULL));
    CHECK(node_add(mod, node_mul(mod, node_i32(mod, 5), param, NULL), param, NULL) == node_mul(mod, node_i32(mod, 6), param, NULL));
    CHECK(
        node_sub(mod,
            node_mul(mod, node_i32(mod, 2), param, NULL),
            node_mul(mod, node_i32(mod, 5), param, NULL), NULL)
        == node_mul(mod, node_i32(mod, -3), param, NULL));

    CHECK(node_cmplt(mod, node_bitcast(mod, param, type_u32(mod), NULL), node_u32(mod, 0), NULL) == node_i1(mod, false));
    CHECK(node_cmpgt(mod, node_u32(mod, 5), node_u32(mod, 0), NULL) == node_i1(mod, true));
    CHECK(node_cmpeq(mod, param, param, NULL) == node_i1(mod, true));

    CHECK(node_rshft(mod, param, node_i32(mod, 0), NULL) == param);
    CHECK(node_lshft(mod, param, node_i32(mod, 0), NULL) == param);

    CHECK(node_mul(mod, param, node_i32(mod, 1), NULL) == param);
    CHECK(node_div(mod, param, node_i32(mod, 1), NULL) == param);
    CHECK(node_add(mod, param, node_i32(mod, 0), NULL) == param);
    CHECK(node_mod(mod, param, node_i32(mod, 1), NULL) == node_i32(mod, 0));
    CHECK(node_sub(mod, param, param, NULL) == node_i32(mod, 0));
    CHECK(node_div(mod, param, param, NULL) == node_i32(mod, 1));
    CHECK(node_mod(mod, param, param, NULL) == node_i32(mod, 0));
    CHECK(node_mul(mod, param, node_i32(mod, 0), NULL) == node_i32(mod, 0));

    CHECK(node_and(mod, param, node_i32(mod, 0), NULL) == node_i32(mod, 0));
    CHECK(node_and(mod, param, node_i32(mod, -1), NULL) == param);
    CHECK(node_or(mod, param, node_i32(mod, -1), NULL) == node_i32(mod, -1));
    CHECK(node_xor(mod, param, node_i32(mod, 0), NULL) == param);
    CHECK(node_and(mod, param, node_or(mod, param, node_i32(mod, 5), NULL), NULL) == param);
    CHECK(node_or(mod, param, node_and(mod, param, node_i32(mod, 5), NULL), NULL) == param);
    CHECK(node_and(mod, node_or(mod, param, node_i32(mod, 5), NULL), param, NULL) == param);
    CHECK(node_or(mod, node_and(mod, param, node_i32(mod, 5), NULL), param, NULL) == param);
    CHECK(node_xor(mod, param, node_xor(mod, param, node_i32(mod, 5), NULL), NULL) == node_i32(mod, 5));
    CHECK(node_xor(mod, node_xor(mod, param, node_i32(mod, 5), NULL), param, NULL) == node_i32(mod, 5));
    CHECK(node_xor(mod, param, param, NULL) == node_i32(mod, 0));

    CHECK(node_add(mod, node_i8 (mod, 1), node_i8 (mod, 1), NULL) == node_i8 (mod, 2));
    CHECK(node_add(mod, node_i16(mod, 1), node_i16(mod, 1), NULL) == node_i16(mod, 2));
    CHECK(node_add(mod, node_i32(mod, 1), node_i32(mod, 1), NULL) == node_i32(mod, 2));
    CHECK(node_add(mod, node_i64(mod, 1), node_i64(mod, 1), NULL) == node_i64(mod, 2));

    CHECK(node_add(mod, node_u8 (mod, 1), node_u8 (mod, 1), NULL) == node_u8 (mod, 2));
    CHECK(node_add(mod, node_u16(mod, 1), node_u16(mod, 1), NULL) == node_u16(mod, 2));
    CHECK(node_add(mod, node_u32(mod, 1), node_u32(mod, 1), NULL) == node_u32(mod, 2));
    CHECK(node_add(mod, node_u64(mod, 1), node_u64(mod, 1), NULL) == node_u64(mod, 2));

    CHECK(node_add(mod, node_f32(mod, 1.0f), node_f32(mod, 1.0f), NULL) == node_f32(mod, 2.0f));
    CHECK(node_add(mod, node_f64(mod, 1.0), node_f64(mod, 1.0), NULL) == node_f64(mod, 2.0));

    CHECK(
        node_bitcast(mod,
            node_mul(mod,
                node_bitcast(mod, node_f32(mod, 1.0f), type_i32(mod), NULL),
                node_i32(mod, 1),
                NULL),
            type_f32(mod),
            NULL)
        == node_f32(mod, 1.0f));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_fn(void) {
    mod_t* mod = mod_create();
    const node_t* fn1;
    const node_t* fn2;
    const node_t* body;
    const node_t* param;
    const type_t* param_types[2];
    const node_t* call_ops[2];
    const node_t* args[2];
    const node_t* x;
    const node_t* n;
    const node_t* res;
    const node_t* ins;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    param_types[0] = type_i32(mod);
    param_types[1] = type_i32(mod);
    fn1 = node_fn(mod, type_fn(mod, type_tuple(mod, 2, param_types), type_i32(mod)), NULL);
    param = node_param(mod, fn1, NULL);
    ins = node_insert(mod, param, node_i32(mod, 0), node_i32(mod, 42), NULL);
    x  = node_extract(mod, param, node_i32(mod, 0), NULL);
    n  = node_extract(mod, param, node_i32(mod, 1), NULL);
    call_ops[0] = x;
    call_ops[1] = node_sub(mod, n, node_i32(mod, 1), NULL);
    body = node_if(mod,
        node_cmpeq(mod, n, node_i32(mod, 0), NULL),
        node_i32(mod, 1),
        node_mul(mod, x, node_app(mod, fn1, node_tuple(mod, 2, call_ops, NULL), NULL), NULL),
        NULL);
    node_bind(mod, fn1, body);
    node_run_if(mod, fn1, node_known(mod, n, NULL));

    CHECK(use_find(param->uses, 0, x) != NULL);
    CHECK(use_find(param->uses, 0, n) != NULL);
    CHECK(use_find(param->uses, 0, ins) != NULL);
    CHECK(node_count_uses(param) == 3);

    args[0] = node_i32(mod, 2);
    args[1] = node_i32(mod, 8);
    //CHECK(node_app(mod, fn1, node_tuple(mod, 2, args, NULL), NULL) == node_i32(mod, 256));

    size_t N = 100;
    fn2 = node_fn(mod, type_fn(mod, type_i32(mod), type_i32(mod)), NULL);
    param = node_param(mod, fn2, NULL);
    args[0] = param;
    args[1] = node_i32(mod, N);
    body = node_app(mod, fn1, node_tuple(mod, 2, args, NULL), NULL);
    res = param;
    for (size_t i = 1; i < N; ++i)
        res = node_mul(mod, param, res, NULL);
    //CHECK(body == res);

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
        {"arrays",   test_arrays},
        {"if",       test_if},
        {"bitcast",  test_bitcast},
        {"binops",   test_binops},
        {"fn",       test_fn}
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
