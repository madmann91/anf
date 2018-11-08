#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "anf.h"
#include "lex.h"
#include "parse.h"
#include "bind.h"
#include "util.h"
#include "mpool.h"
#include "print.h"
#include "util.h"

#if defined(_WIN32) && !defined (__CYGWIN__)
    #define WIN32_LEAN_AND_MEAN 1
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

typedef struct binder_with_file_s binder_with_file_t;

struct binder_with_file_s {
    binder_t binder;
    const char* file;
};

static bool colorize = false;

static void error(const char* file, const loc_t* loc, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (loc && file) {
        if (loc->brow != loc->erow || loc->bcol != loc->ecol) {
            const char* prologue = COLORIZE(colorize, COLOR_ERR("error"), " in ", COLOR_LOC("%s(%zu, %zu - %zu, %zu)"), ": ");
            fprintf(stderr, prologue, file, loc->brow, loc->bcol, loc->erow, loc->ecol);
        } else {
            const char* prologue = COLORIZE(colorize, COLOR_ERR("error"), " in ", COLOR_LOC("%s(%zu, %zu)"), ": ");
            fprintf(stderr, prologue, file, loc->brow, loc->bcol);
        }
    } else {
        fprintf(stderr, COLORIZE(colorize, COLOR_ERR("error"), ": "));
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static void lexer_error_fn(lexer_t* lexer, const loc_t* loc, const char* str) {
    error(lexer->file, loc, "%s", str);
}

static void parser_error_fn(parser_t* parser, const loc_t* loc, const char* str) {
    error(parser->lexer->file, loc, "%s", str);
}

static void binder_error_fn(binder_t* binder, const loc_t* loc, const char* str) {
    error(((binder_with_file_t*)binder)->file, loc, "%s", str);
}

void usage(void) {
    static const char* usage_str =
        "usage: anf [options] file...\n"
        "options:\n"
        "  --help       display this information\n"
        "  --must-fail  invert the return code\n";
    printf("%s", usage_str);
}

char* read_file(const char* file, size_t* size) {
    FILE* fp = fopen(file, "r");
    if (!fp)
        return NULL;
    size_t cap = 1024, i = 0;
    char* buf = xmalloc(cap);
    while (true) {
        size_t read = fread(buf + i, sizeof(char), cap - i, fp);
        i += read;
        if (read != cap - i) {
            if (feof(fp)) break;
            if (ferror(fp)) {
                // Notify caller on read error
                free(buf);
                fclose(fp);
                return NULL;
            }
            cap *= 2;
            buf = xrealloc(buf, cap);
        }
    }
    *size = i;
    fclose(fp);
    return buf;
}

bool process_file(const char* file) {
    size_t file_size = 0;
    char* file_data = read_file(file, &file_size);
    if (!file_data) {
        error(NULL, NULL, "cannot read file '%s'", file);
        return false;
    }

    mpool_t* pool = mpool_create();
    lexer_t lexer = {
        .tmp      = 0,
        .str      = file_data,
        .size     = file_size,
        .file     = file,
        .row      = 1,
        .col      = 1,
        .error_fn = lexer_error_fn
    };
    parser_t parser = {
        .lexer    = &lexer,
        .pool     = &pool,
        .error_fn = parser_error_fn
    };

    // Parse program
    ast_t* ast = parse(&parser);
    bool ok = !parser.errs && !lexer.errs;
    if (ok) {
        // Bind identifiers to AST nodes
        id2ast_t id2ast = id2ast_create();
        binder_with_file_t binder_with_file = {
            .binder = (binder_t) {
                .id2ast   = &id2ast,
                .error_fn = binder_error_fn
            },
            .file = file
        };
        id2ast_destroy(&id2ast);
        bind(&binder_with_file.binder, ast);
        ok &= !binder_with_file.binder.errs;
    }

    // Display program on success
    if (ok) {
        file_printer_t file_printer = printer_from_file(stdout, colorize, 0);
        ast_print(ast, &file_printer.printer);
        pprintf(&file_printer.printer, "\n");
    }

    free(file_data);
    mpool_destroy(pool);
    return ok;
}

int main(int argc, char** argv) {
    // Detect if the standard output is a tty
    colorize = isatty(fileno(stdout)) && isatty(fileno(stderr));

    if (argc <= 1) {
        error(NULL, NULL, "no input files");
        return 1;
    }

    bool must_fail = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "--help")) {
                usage();
                return 0;
            } else if (!strcmp(argv[i], "--must-fail")) {
                must_fail = true;
            } else {
                error(NULL, NULL, "unknown option '%s'", argv[i]);
                return 1;
            }
        }
    }

    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-')
            ok &= process_file(argv[i]);
    }

    return must_fail ^ ok ? 0 : 1;
}
