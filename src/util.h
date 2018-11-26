#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void die(const char*);
void* xmalloc(size_t);
void* xcalloc(size_t, size_t);
void* xrealloc(void*, size_t);

typedef union fmt_arg_u fmt_arg_t;

union fmt_arg_u {
    bool     b;
    char     c;
    int8_t   i8;
    int16_t  i16;
    int32_t  i32;
    int64_t  i64;
    uint8_t  u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float    f32;
    double   f64;
    const void* ptr;
    const char* str;
};

enum fmt_flags_e {
    FMT_COLORIZE = 0x01
};

size_t try_format(char*, size_t, const char*, const fmt_arg_t*, uint32_t);
size_t format(char**, size_t, const char*, const fmt_arg_t*, uint32_t);
size_t format_to_file(FILE*, const char*, const fmt_arg_t*, uint32_t);

#endif // UTIL_H
