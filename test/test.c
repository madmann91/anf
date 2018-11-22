#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "htable.h"
#include "mpool.h"
#include "anf.h"
#include "scope.h"
#include "io.h"
#include "lex.h"
#include "parse.h"
#include "print.h"
#include "util.h"

#define CHECK(expr) check(env, expr, #expr, __FILE__, __LINE__)
void check(jmp_buf env, bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        fprintf(stderr, "check failed in %s(%d): %s\n", file, line, expr);
        longjmp(env, 1);
    }
}

bool cmp_elem(const void* a, const void* b) { return *(uint32_t*)a == *(uint32_t*)b; }
uint32_t hash_elem(const void* elem) { return *(uint32_t*)elem % 239; }
HSET(elemset, uint32_t, cmp_elem, hash_elem)

bool test_hset(void) {
    size_t N = 4000;
    size_t inc[3] = {3, 5, 7};
    uint32_t* values = xmalloc(sizeof(uint32_t) * N);
    for (size_t i = 0, j = 0; i < N; ++i, j += inc[j%3]) {
        values[i] = j;
    }
    elemset_t set1 = elemset_create();
    elemset_t set2 = elemset_create();

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    for (size_t i = 0; i < N; ++i)
        CHECK(elemset_insert(&set1, values[i]));
    for (size_t i = N - 1; i >= N / 2; --i)
        CHECK(elemset_remove(&set1, values[i]));
    for (size_t i = N / 2; i < N; ++i)
        CHECK(elemset_lookup(&set1, values[i]) == NULL);
    for (size_t i = 0; i < N / 2; ++i)
        CHECK(elemset_lookup(&set1, values[i]) != NULL);
    FORALL_HSET(set1, uint32_t, elem, {
        CHECK(elemset_insert(&set2, elem));
    })
    for (size_t i = 0; i < N / 2; ++i)
        CHECK(elemset_lookup(&set2, values[i]) != NULL);

    CHECK(set1.table->nelems == N / 2);
    CHECK(set2.table->nelems == N / 2);

cleanup:
    elemset_destroy(&set1);
    elemset_destroy(&set2);
    free(values);
    return status == 0;
}

bool test_mpool(void) {
    mpool_t* pool = mpool_create_with_cap(1024 * 1024);

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
    fn_t* fn;
    const node_t* param;

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

    fn = node_fn(mod, type_fn(mod, tuple1->type, type_i32(mod)), NULL);
    param = node_param(mod, fn, NULL);

    ops1[0] = node_extract(mod, param, node_i32(mod, 0), NULL);
    ops1[1] = node_extract(mod, param, node_i32(mod, 1), NULL);
    ops1[2] = node_extract(mod, param, node_i32(mod, 2), NULL);
    CHECK(node_tuple(mod, 3, ops1, NULL) == param);

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
    array1 = node_array(mod, 3, elems1, elems1[0]->type, NULL);
    elems2[0] = node_i32(mod, 4);
    elems2[1] = node_i32(mod, 5);
    elems2[2] = node_i32(mod, 6);
    array2 = node_array(mod, 3, elems2, elems2[0]->type, NULL);

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

bool test_select(void) {
    mod_t* mod = mod_create();

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    CHECK(node_select(mod, node_i1(mod, true),  node_i32(mod, 32), node_i32(mod, 64), NULL) == node_i32(mod, 32));
    CHECK(node_select(mod, node_i1(mod, false), node_i32(mod, 32), node_i32(mod, 64), NULL) == node_i32(mod, 64));
    CHECK(node_select(mod, node_undef(mod, type_i1(mod)), node_i32(mod, 32), node_i32(mod, 32), NULL) == node_i32(mod, 32));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

bool test_bitcast(void) {
    mod_t* mod = mod_create();
    fn_t* fn;
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
    fn_t* fn;
    const node_t* param;
    const node_t* x, *y;
    const node_t* a, *b;

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
    CHECK(node_rem(mod, param, node_i32(mod, 1), NULL) == node_i32(mod, 0));
    CHECK(node_sub(mod, param, param, NULL) == node_i32(mod, 0));
    CHECK(node_div(mod, param, param, NULL) == node_i32(mod, 1));
    CHECK(node_rem(mod, param, param, NULL) == node_i32(mod, 0));
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

    fn = node_fn(mod, type_fn(mod, type_tuple_args(mod, 2, type_i32(mod), type_i32(mod)), type_i32(mod)), NULL);
    param = node_param(mod, fn, NULL);
    x = node_extract(mod, param, node_i32(mod, 0), NULL);
    y = node_extract(mod, param, node_i32(mod, 1), NULL);
    a = node_cmpgt(mod, x, node_i32(mod, 42), NULL);
    b = node_cmpgt(mod, y, node_i32(mod, 42), NULL);

    // (x >= 5) & (x >= 3) <=> (x >= 5)
    CHECK(
        node_and(mod,
            node_cmpge(mod, x, node_i32(mod, 5), NULL),
            node_cmpge(mod, x, node_i32(mod, 3), NULL),
            NULL)
        == node_cmpge(mod, x, node_i32(mod, 5), NULL));

    // (x < 5) & (x < 3) <=> (x < 3)
    CHECK(
        node_and(mod,
            node_cmplt(mod, x, node_i32(mod, 5), NULL),
            node_cmplt(mod, x, node_i32(mod, 3), NULL),
            NULL)
        == node_cmplt(mod, x, node_i32(mod, 3), NULL));

    // (a & b) | ((a | b) & (a | b | (a & b))) <=> (a | b)
    CHECK(
        node_or(mod,
            node_and(mod, a, b, NULL),
            node_and(mod,
                node_or(mod, a, b, NULL),
                node_or(mod,
                    a,
                    node_or(mod, b, node_and(mod, a, b, NULL), NULL),
                    NULL),
                NULL),
            NULL)
        == node_or(mod, a, b, NULL));

    // (x == y) | (x >= y) <=> (x >= y)
    CHECK(
        node_or(mod,
            node_cmpeq(mod, x, y, NULL),
            node_cmpge(mod, x, y, NULL),
            NULL)
        == node_cmpge(mod, x, y, NULL));

cleanup:
    mod_destroy(mod);
    return status == 0;
}

static inline fn_t* make_const_fn(mod_t* mod, const type_t* type) {
    const type_t* inner_type = type_fn(mod, type, type);
    fn_t* inner = node_fn(mod, inner_type, NULL);
    fn_t* outer = node_fn(mod, type_fn(mod, type, inner_type), NULL);
    const node_t* x = node_param(mod, outer, NULL);
    fn_bind(mod, inner, 0, x);
    fn_bind(mod, outer, 0, &inner->node);
    return outer;
}

bool test_scope(void) {
    mod_t* mod = mod_create();
    scope_t scope = { .entry = NULL, .nodes = node_set_create() };
    node_set_t fvs = node_set_create();

    fn_t* inner, *outer;
    const node_t* x, *y;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    outer = make_const_fn(mod, type_i32(mod));
    inner = (fn_t*)outer->node.ops[0];
    x = node_param(mod, outer, NULL);
    y = node_param(mod, inner, NULL);

    scope.entry = outer;
    scope_compute(mod, &scope);
    CHECK(node_set_lookup(&scope.nodes, &inner->node) != NULL);
    CHECK(node_set_lookup(&scope.nodes, &outer->node) != NULL);
    CHECK(node_set_lookup(&scope.nodes, x) != NULL);
    CHECK(node_set_lookup(&scope.nodes, y) != NULL);
    CHECK(scope.nodes.table->nelems == 4);

    scope.entry = inner;
    node_set_clear(&scope.nodes);
    scope_compute(mod, &scope);
    CHECK(node_set_lookup(&scope.nodes, &inner->node) != NULL);
    CHECK(node_set_lookup(&scope.nodes, y) != NULL);
    CHECK(scope.nodes.table->nelems == 2);

    scope_compute_fvs(&scope, &fvs);
    CHECK(node_set_lookup(&fvs, x) != NULL);
    CHECK(fvs.table->nelems == 1);

cleanup:
    node_set_destroy(&scope.nodes);
    node_set_destroy(&fvs);
    mod_destroy(mod);
    return status == 0;
}

bool test_io() {
    mod_t* mod = mod_create();
    mod_t* loaded_mod = NULL;
    mpool_t* pool = mpool_create();
    FILE* out = NULL;
    FILE* in  = NULL;
    file_io_t file_io;

    fn_t* fn1, *fn2;
    const node_t* param1, *param2;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    make_const_fn(mod, type_i32(mod));

    out = fopen("mod.anf", "wb");
    file_io = io_from_file(out);
    CHECK(mod_save(mod, &file_io.io));
    fclose(out);
    out = NULL;

    in = fopen("mod.anf", "rb");
    file_io = io_from_file(in);
    loaded_mod = mod_load(&file_io.io, &pool);
    fclose(in);
    in = NULL;

    CHECK(loaded_mod);
    CHECK(loaded_mod->fns.nelems == 2);
    fn1 = loaded_mod->fns.elems[0];
    fn2 = loaded_mod->fns.elems[1];
    param1 = node_param(loaded_mod, fn1, NULL);
    param2 = node_param(loaded_mod, fn2, NULL);
    if (node_count_uses(param2) != 0) {
        const node_t* param = param2;
        fn_t* fn = fn2;
        param2 = param1;
        param1 = param;
        fn2 = fn1;
        fn1 = fn;
    }
    CHECK(fn1->node.ops[0] == &fn2->node);
    CHECK(fn2->node.ops[0] == param1);

cleanup:
    mod_destroy(mod);
    if (loaded_mod) mod_destroy(loaded_mod);
    if (in)  fclose(in);
    if (out) fclose(out);
    mpool_destroy(pool);
    return status == 0;
}

bool test_opt(void) {
    mod_t* mod = mod_create();

    const fn_t* opt_outer;
    const node_t* opt_y;
    const node_t* pow0, *pow1, *pow2, *pow4, *pow5;

    fn_t* pow, *when_zero, *when_nzero, *when_even, *when_odd, *outer;
    const node_t* node_ops[3];
    const node_t* node_false;
    const node_t* x, *n;
    const node_t* unit;
    const node_t* param;
    const node_t* modulo;
    const node_t* cmp_zero, *cmp_even;
    const node_t* pow_even, *pow_odd, *pow_half;
    const type_t* pow_type, *bb_type;
    dbg_t dbg_x, dbg_n;
    dbg_t dbg_y;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    dbg_x = (dbg_t) { .file = "", .name = "x" };
    dbg_n = (dbg_t) { .file = "", .name = "n" };
    dbg_y = (dbg_t) { .file = "", .name = "y" };

    node_false = node_i1(mod, false);

    pow_type = type_fn(mod, type_tuple_args(mod, 3, type_i32(mod), type_i32(mod), type_tuple(mod, 0, NULL)), type_i32(mod));
    bb_type  = type_fn(mod, type_tuple(mod, 0, NULL), type_i32(mod));
    pow = node_fn(mod, pow_type, NULL);
    when_zero  = node_fn(mod, bb_type, NULL);
    when_nzero = node_fn(mod, bb_type, NULL);
    when_odd   = node_fn(mod, bb_type, NULL);
    when_even  = node_fn(mod, bb_type, NULL);
    param = node_param(mod, pow, NULL);
    x = node_extract(mod, param, node_i32(mod, 0), &dbg_x);
    n = node_extract(mod, param, node_i32(mod, 1), &dbg_n);
    cmp_zero = node_cmpeq(mod, n, node_i32(mod, 0), NULL);
    modulo = node_rem(mod, n, node_i32(mod, 2), NULL);
    cmp_even = node_cmpeq(mod, modulo, node_i32(mod, 0), NULL);
    unit = node_tuple(mod, 0, NULL, NULL);

    fn_bind(mod, pow, 0, node_app(mod, node_select(mod, cmp_zero, &when_zero->node, &when_nzero->node, NULL), unit, node_false, NULL));
    fn_bind(mod, when_zero, 0, node_i32(mod, 1));
    fn_bind(mod, when_nzero, 0, node_app(mod, node_select(mod, cmp_even, &when_even->node, &when_odd->node, NULL), unit, node_false, NULL));
    node_ops[0] = x;
    node_ops[1] = node_sub(mod, n, node_i32(mod, 1), NULL);
    node_ops[2] = node_tuple(mod, 0, NULL, NULL);
    pow_odd = node_mul(mod, x, node_app(mod, &pow->node, node_tuple(mod, 3, node_ops, NULL), node_false, NULL), NULL);
    node_ops[1] = node_div(mod, n, node_i32(mod, 2), NULL);
    pow_half = node_app(mod, &pow->node, node_tuple(mod, 3, node_ops, NULL), node_false, NULL);
    pow_even = node_mul(mod, pow_half, pow_half, NULL);
    fn_bind(mod, when_even, 0, pow_even);
    fn_bind(mod, when_odd,  0, pow_odd);

    outer = node_fn(mod, type_fn(mod, type_i32(mod), type_i32(mod)), NULL);
    outer->exported = true;
    node_ops[0] = node_param(mod, outer, &dbg_y);
    node_ops[1] = node_i32(mod, 5);
    fn_bind(mod, outer, 0, node_app(mod, &pow->node, node_tuple(mod, 3, node_ops, NULL), node_false, NULL));

    fn_bind(mod, pow, 1, node_known(mod, n, NULL));
    mod_opt(&mod);

    CHECK(mod->fns.nelems == 1);
    CHECK(mod->fns.elems[0]->exported);

    opt_outer = mod->fns.elems[0];
    opt_y = node_param(mod, opt_outer, NULL);
    pow0 = node_i32(mod, 1);
    pow1 = node_mul(mod, opt_y, pow0, NULL);
    pow2 = node_mul(mod, pow1, pow1, NULL);
    pow4 = node_mul(mod, pow2, pow2, NULL);
    pow5 = node_mul(mod, opt_y, pow4, NULL);
    CHECK(opt_outer->node.ops[0] == pow5);

cleanup:
    if (status) {
        FORALL_HSET(mod->nodes, const node_t*, node, {
            node_dump(node);
        })
        FORALL_VEC(mod->fns, const fn_t*, fn, {
            node_dump(&fn->node);
            printf("\t");
            if (fn->node.ops[0]) node_dump(fn->node.ops[0]);
        })
    }

    mod_destroy(mod);
    return status == 0;
}

bool test_mem(void) {
    mod_t* mod = mod_create();

    const type_t* fn_type;
    fn_t* fn;
    const node_t* param;
    const node_t* res;
    const node_t* alloc;
    const node_t* mem;
    const node_t* ptr;
    const node_t* val;

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    fn_type = type_fn(mod, type_mem(mod), type_tuple_args(mod, 2, type_mem(mod), type_tuple_args(mod, 2, type_i16(mod), type_u32(mod))));
    fn = node_fn(mod, fn_type, NULL);
    param = node_param(mod, fn, NULL);
    val = node_tuple_args(mod, 2, NULL, node_i32(mod, 5), node_tuple_args(mod, 2, NULL, node_i16(mod, 42), node_u32(mod, 33)));
    res = node_alloc(mod, param, val->type, NULL);
    alloc = node_extract(mod, res, node_i32(mod, 1), NULL);
    mem = node_extract(mod, res, node_i32(mod, 0), NULL);
    mem = node_store(mod, mem, alloc, val, NULL);
    ptr = node_offset(mod, alloc, node_i32(mod, 1), NULL);
    res = node_load(mod, mem, ptr, NULL);
    mem = node_extract(mod, res, node_i32(mod, 0), NULL);
    mem = node_store(mod, mem, node_offset(mod, ptr, node_i32(mod, 1), NULL), node_u32(mod, 34), NULL);
    mem = node_store(mod, mem, node_offset(mod, alloc, node_i32(mod, 0), NULL), node_i32(mod, 6), NULL);
    res = node_load(mod, mem, ptr, NULL);
    mem = node_extract(mod, res, node_i32(mod, 0), NULL);
    res = node_extract(mod, res, node_i32(mod, 1), NULL);
    mem = node_dealloc(mod, mem, alloc, NULL);
    fn_bind(mod, fn, 0, node_tuple_args(mod, 2, NULL, mem, res));

    fn->exported = true;

    mod_opt(&mod);

    CHECK(mod->fns.nelems == 1);
    CHECK(mod->fns.elems[0]->exported);
    CHECK(node_extract(mod, mod->fns.elems[0]->node.ops[0], node_i32(mod, 1), NULL) == node_tuple_args(mod, 2, NULL, node_i16(mod, 42), node_u32(mod, 34)));

cleanup:
    if (status) {
        FORALL_HSET(mod->nodes, const node_t*, node, {
            node_dump(node);
        })
        FORALL_VEC(mod->fns, const fn_t*, fn, {
            node_dump(&fn->node);
            printf("\t");
            if (fn->node.ops[0]) node_dump(fn->node.ops[0]);
        })
    }

    mod_destroy(mod);
    return status == 0;
}

bool test_lex(void) {
    const char* str =
        "hello if\'c\' ^ /* this is a multi-\n"
        " line comment */ else world!  | // another comment \n"
        " (- ), < * \"string\xE2\x82\xAC\" +: var; / def=% >something & 0b010010110 0xFFe45 10.3e+7";
    uint32_t tags[] = {
        TOK_ID, TOK_IF, TOK_CHR, TOK_XOR,
        TOK_ELSE, TOK_ID, TOK_NOT, TOK_OR,
        TOK_LPAREN, TOK_SUB, TOK_RPAREN, TOK_COMMA, TOK_LANGLE, TOK_MUL, TOK_STR,
        TOK_ADD, TOK_COLON, TOK_VAR, TOK_SEMI, TOK_DIV, TOK_DEF, TOK_EQ, TOK_REM,
        TOK_RANGLE, TOK_ID, TOK_AND, TOK_INT, TOK_INT, TOK_FLT, TOK_EOF
    };
    size_t num_tags = sizeof(tags) / sizeof(tags[0]);
    char* buf = xmalloc(strlen(str) + 1);
    log_t log = log_create_silent();
    lexer_t lexer = {
        .tmp  = 0,
        .str  = buf,
        .size = strlen(str),
        .row  = 1,
        .col  = 1,
        .log  = &log
    };
    strcpy(buf, str);

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    size_t n = 0;
    while (true) {
        tok_t tok = lex(&lexer);
        CHECK(n < num_tags && tags[n] == tok.tag);
        n++;
        if (tok.tag == TOK_EOF)
            break;
    }
    CHECK(log.errs == 0);

cleanup:
    free(buf);
    return status == 0;
}

bool test_parse(void) {
    const char* str =
        "mod hello {\n"
        "    var z = 33 * 4 + (2 >> 1)\n"
        "    val (a, b) = 7\n"
        "    def func(x\n) {\n"
        "        z >>= 3\n"
        "        (x, z, \n"
        "        {x ; y})\n"
        "    }\n"
        "}";
    char* buf = xmalloc(strlen(str) + 1);
    mpool_t* pool = mpool_create();
    log_t log = log_create_silent();
    lexer_t lexer = {
        .tmp  = 0,
        .str  = buf,
        .size = strlen(str),
        .row  = 1,
        .col  = 1,
        .log  = &log
    };
    parser_t parser = {
        .lexer = &lexer,
        .pool  = &pool,
        .log   = &log
    };
    strcpy(buf, str);

    jmp_buf env;
    int status = setjmp(env);
    if (status)
        goto cleanup;

    ast_t* ast = parse(&parser);
    CHECK(log.errs == 0);
    CHECK(ast);

cleanup:
    mpool_destroy(pool);
    free(buf);
    return status == 0;
}

typedef struct {
    const char* name;
    bool (*test_fn)(void);
} test_t;

void usage(void) {
    printf("usage: anf_test [options]\n"
           "  --help       display this information\n"
           "  -t [module]  test the given module\n");
}

size_t find_test(const char* name, const test_t* tests, size_t ntests) {
    for (size_t i = 0; i < ntests; ++i) {
        if (!strcmp(tests[i].name, name)) return i;
    }
    return ntests;
}

bool run_test(const test_t* test) {
    bool res = test->test_fn();
    printf("- %s: %s\n", test->name, res ? "\33[;32;1mOK\33[0m" : "\33[;31;1mFAIL\33[0m");
    return res;
}

int main(int argc, char** argv) {
    test_t tests[] = {
        {"hset",     test_hset},
        {"mpool",    test_mpool},
        {"types",    test_types},
        {"literals", test_literals},
        {"tuples",   test_tuples},
        {"arrays",   test_arrays},
        {"select",   test_select},
        {"bitcast",  test_bitcast},
        {"binops",   test_binops},
        {"scope",    test_scope},
        {"io",       test_io},
        {"opt",      test_opt},
        {"mem",      test_mem},
        {"lex",      test_lex},
        {"parse",    test_parse}
    };
    const size_t ntests = sizeof(tests) / sizeof(test_t);
    bool ok = true;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (!strcmp(argv[i], "--help")) {
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
            ok &= run_test(&tests[i]);
    }
    return ok ? 0 : 1;
}
