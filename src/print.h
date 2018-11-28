#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

typedef union  fmt_arg_u      fmt_arg_t;
typedef struct printer_s      printer_t;
typedef struct file_printer_s file_printer_t;
typedef struct mem_printer_s  mem_printer_t;
typedef void (*formatfn_t)(printer_t*, const char*, const fmt_arg_t*);

union fmt_arg_u {
    bool b;
    char c;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    const void* p;
    const char* s;
    const struct ast_s* a;
    const struct ast_type_s* at;
    const struct node_s* n;
    const struct type_s* t;
};

struct printer_s {
    formatfn_t format;
    bool colorize;
    const char* tab;
    size_t indent;
};

struct file_printer_s {
    printer_t printer;
    FILE* fp;
};

struct mem_printer_s {
    printer_t printer;
    char* buf;
    size_t cap;
    long off;
};

#define print(printer, fmt, ...) \
    do { \
        fmt_arg_t args[] = { \
            { .u64 = 0 }, \
            __VA_ARGS__ \
        }; \
        (printer)->format((printer), fmt, sizeof(args) > sizeof(args[0]) ? args + 1 : NULL); \
    } while (false)

file_printer_t printer_from_file(FILE*);
mem_printer_t printer_from_buffer(char*, size_t);

#endif // PRINT_H
