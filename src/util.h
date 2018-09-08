#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defers/Expands expressions
#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()
#define EXPAND(...) __VA_ARGS__

// Concatenates tokens
#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define PROBE(x) x, 1,

// Evaluates to 1 for probes, otherwise 0
#define IS_PROBE(...) IS_PROBE_N(__VA_ARGS__, 0, )
#define IS_PROBE_N(x, n, ...) n

// Evaluates to 1 if the expression is parenthesized, otherwise 0
#define IS_PAREN(x) IS_PROBE(IS_PAREN_PROBE x)
#define IS_PAREN_PROBE(...) PROBE(~)

// Helper macro for compile-time loops over macro arguments
#define FOR_EACH_1(F, X) F(X)
#define FOR_EACH_2(F, X, ...) F(X) FOR_EACH_1(F, __VA_ARGS__)
#define FOR_EACH_3(F, X, ...) F(X) FOR_EACH_2(F, __VA_ARGS__)
#define FOR_EACH_4(F, X, ...) F(X) FOR_EACH_3(F, __VA_ARGS__)
#define FOR_EACH_5(F, X, ...) F(X) FOR_EACH_4(F, __VA_ARGS__)
#define FOR_EACH_6(F, X, ...) F(X) FOR_EACH_5(F, __VA_ARGS__)
#define FOR_EACH_7(F, X, ...) F(X) FOR_EACH_6(F, __VA_ARGS__)
#define FOR_EACH_8(F, X, ...) F(X) FOR_EACH_7(F, __VA_ARGS__)
#define FOR_EACH_9(F, X, ...) F(X) FOR_EACH_8(F, __VA_ARGS__)
#define SELECT(_1, _2, _3, _4, _5, _6, _7, _8, _9, F, ...) F
#define FOR_EACH(F, ...) SELECT(__VA_ARGS__, FOR_EACH_9, FOR_EACH_8, FOR_EACH_7, FOR_EACH_6, FOR_EACH_5, FOR_EACH_4, FOR_EACH_3, FOR_EACH_2, FOR_EACH_1, ) (F, __VA_ARGS__)

// Mini-language to format output using ANSI terminal color codes:
//
// * COLORIZE_N(ENABLED/DISABLED, ...) generates a string
//   with colors either enabled or disabled (statically).
//
// * COLORIZE(flag, ...) generates a dynamic condition
//   which selects between the string with and without
//   colors at run-time.
//
// Usage is as follows:
//
// COLORIZE(flag, COLOR_KEY("x"), "hello", COLOR_LIT("world"))
//
// This example generates the following code:
//
// (flag ? "\33[;34;1m" "x" "\33[0m" "hello" "\33[;36;1m" "world" "\33[0m" : "x" "hello" "world" )
#define COLOR_ID(x)   (x, "\33[33m")
#define COLOR_LOC(x)  (x, "\33[37;1m")
#define COLOR_KEY(x)  (x, "\33[34;1m")
#define COLOR_LIT(x)  (x, "\33[36;1m")
#define COLOR_ERR(x)  (x, "\33[31;1m")

#define COLORIZE_ENABLED_0(...) __VA_ARGS__
#define COLORIZE_ENABLED_1(x) COLORIZE_ENABLED_PAIR x
#define COLORIZE_ENABLED_PAIR(x, y) y x "\33[0m"

#define COLORIZE_DISABLED_0(...) __VA_ARGS__
#define COLORIZE_DISABLED_1(x) COLORIZE_DISABLED_PAIR x
#define COLORIZE_DISABLED_PAIR(x, y) x

#define COLORIZE1_ENABLED(x)  CAT(COLORIZE_ENABLED_,  IS_PAREN(x))(x)
#define COLORIZE1_DISABLED(x) CAT(COLORIZE_DISABLED_, IS_PAREN(x))(x)
#define COLORIZE_N(enabled, ...) FOR_EACH(COLORIZE1_##enabled, __VA_ARGS__)

#define COLORIZE(flag, ...) \
    (flag ? COLORIZE_N(ENABLED, __VA_ARGS__) : COLORIZE_N(DISABLED, __VA_ARGS__))

static inline void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    abort();
}

static inline void* xmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void* ptr = malloc(size);
    if (!ptr)
        die("out of memory, malloc() failed");
#ifndef NDEBUG
    memset(ptr, 0xC4, size);
#endif
    return ptr;
}

static inline void* xcalloc(size_t num, size_t size) {
    if (size == 0 || num == 0)
        return NULL;
    void* ptr = calloc(num, size);
    if (!ptr)
        die("out of memory, calloc() failed");
    return ptr;
}

static inline void* xrealloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, size);
    if (!ptr)
        die("out of memory, realloc() failed");
    return ptr;
}

#endif // UTIL_H
