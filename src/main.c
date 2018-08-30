#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "anf.h"
#include "lex.h"
#include "parse.h"
#include "util.h"
#include "mpool.h"

#ifdef _WIN32
    #include <io.h>
    #define isatty _isatty
    #define fileno _fileno
#else
    #include <unistd.h>
#endif

static bool colorize = false;

static void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, COLORIZE(colorize, COLOR_ERROR("error"), ": "));
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

static void error_with_loc(const char* file, const loc_t* loc, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (loc->brow != loc->erow || loc->bcol != loc->ecol) {
        fprintf(stderr,
            COLORIZE(colorize, COLOR_ERROR("error"), " in ", COLOR_LOCATION("%s(%zu, %zu - %zu, %zu)"), ": "),
            file, loc->brow, loc->bcol, loc->erow, loc->ecol
        );
    } else {
        fprintf(stderr,
            COLORIZE(colorize, COLOR_ERROR("error"), " in ", COLOR_LOCATION("%s(%zu, %zu)"), ": "),
            file, loc->brow, loc->bcol
        );
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

static void lex_error_fn(lex_t* lex, const loc_t* loc, const char* str) {
    error_with_loc(lex->file, loc, "%s", str);
}

static void parser_error_fn(parser_t* parser, const loc_t* loc, const char* str) {
    error_with_loc(parser->lex->file, loc, "%s", str);
}

void usage(void) {
    static const char* usage_str =
        "usage: anf [options] file...\n"
        "options:\n"
        "  --help     Displays this information\n";
    printf("%s", usage_str);
}

char* read_file(FILE* fp, size_t* size) {
    size_t cap = 1024, i = 0;
    char* buf = malloc(cap);
    int c;
    while ((c = getc(fp)) != EOF) {
        if (i >= cap)
            buf = realloc(buf, cap * 2);
        buf[i++] = c;
    }
    *size = i;
    return buf;
}

bool process_file(const char* file) {
    FILE* fp = fopen(file, "r");
    if (!file)
        return false;
    size_t file_size = 0;
    char* file_data = read_file(fp, &file_size);
    fclose(fp);

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
    if (!parser.errs && !lex.errs) {
        ast_print(ast, 0, colorize);
        printf("\n");
    }

    free(file_data);
    mpool_destroy(pool);
    return true;
}

int main(int argc, char** argv) {
    // Detect if the standard output is a tty or not
    colorize = isatty(fileno(stdout)) && isatty(fileno(stderr));

    if (argc <= 1)
        error("no input files");

    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "--help")) {
                usage();
                return 0;
            } else {
                error("unknown option '%s'", argv[i]);
            }
        } else {
            ok &= process_file(argv[i]);
        }
    }
    return ok ? 0 : 1;
}
