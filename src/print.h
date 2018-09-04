#ifndef PRINT_H
#define PRINT_H

#include "anf.h"
#include "ast.h"
#include "util.h"

#define pprintf(printer, ...) (printer)->printf((printer), EXPAND(__VA_ARGS__))

typedef struct printer_s printer_t;
typedef struct file_printer_s file_printer_t;

struct printer_s {
    void (*printf)(printer_t*, const char*, ...);
    size_t indent;
    bool colorize;
};

struct file_printer_s {
    printer_t printer;
    FILE* fp;
};

file_printer_t printer_from_file(FILE*, bool, size_t);

void type_print(const type_t*, printer_t*);
void type_dump(const type_t*);

void node_print(const node_t*, printer_t*);
void node_dump(const node_t*);

void ast_print(const ast_t*, printer_t*);
void ast_dump(const ast_t*);

#endif // PRINT_H
