#ifndef PRINT_H
#define PRINT_H

#include "anf.h"
#include "ast.h"
#include "util.h"

typedef struct printer_s printer_t;
typedef struct file_printer_s file_printer_t;
typedef struct mem_printer_s  mem_printer_t;

struct printer_s {
    void (*format)(printer_t*, const char*, const fmt_arg_t*);
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
        (printer)->format((printer), fmt, args + 1); \
    } while (false)

file_printer_t printer_from_file(FILE*, bool, const char*, size_t);
mem_printer_t printer_from_buffer(char*, size_t, bool, bool);

void type_print(const type_t*, printer_t*);
void type_dump(const type_t*);

void node_print(const node_t*, printer_t*);
void node_dump(const node_t*);

void ast_print(const ast_t*, printer_t*);
void ast_dump(const ast_t*);

#endif // PRINT_H
