#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "anf.h"
#include "lex.h"
#include "parse.h"
#include "util.h"
#include "mpool.h"

#if defined(_WIN32) && !defined (__CYGWIN__)
    #define WIN32_LEAN_AND_MEAN 1
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

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

static void lex_error_fn(lex_t* lex, const loc_t* loc, const char* str) {
    error(lex->file, loc, "%s", str);
}

static void parser_error_fn(parser_t* parser, const loc_t* loc, const char* str) {
    error(parser->lex->file, loc, "%s", str);
}

void usage(void) {
    static const char* usage_str =
        "usage: anf [options] file...\n"
        "options:\n"
        "  --help     Displays this information\n";
    printf("%s", usage_str);
}

char* read_file(const char* file, size_t* size) {
    FILE* fp = fopen(file, "r");
    if (!fp)
        return NULL;
    size_t cap = 1024, i = 0;
    char* buf = malloc(cap);
    while (true) {
        int c = fgetc(fp);
        if (c == EOF)
            break;
        if (i >= cap)
            buf = realloc(buf, cap * 2);
        buf[i++] = c;
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
    lex_t lex = {
        .tmp      = 0,
        .str      = file_data,
        .size     = file_size,
        .file     = file,
        .row      = 1,
        .col      = 1,
        .error_fn = lex_error_fn
    };
    parser_t parser = {
        .lex      = &lex,
        .pool     = &pool,
        .error_fn = parser_error_fn
    };

    ast_t* ast = parse(&parser);
    bool ok = !parser.errs && !lex.errs;
    if (ok) {
        ast_print(ast, 0, colorize);
        printf("\n");
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

    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "--help")) {
                usage();
                return 0;
            } else {
                error(NULL, NULL, "unknown option '%s'", argv[i]);
                return 1;
            }
        } else {
            ok &= process_file(argv[i]);
        }
    }

    return ok ? 0 : 1;
}
