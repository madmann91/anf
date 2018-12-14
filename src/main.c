#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "mod.h"
#include "node.h"
#include "type.h"
#include "lex.h"
#include "parse.h"
#include "bind.h"
#include "check.h"
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

static default_log_t global_log;

static void usage(void) {
    static const char* usage_str =
        "usage: anf [options] file...\n"
        "options:\n"
        "  --help       display this information\n"
        "  --must-fail  invert the return code\n";
    fputs(usage_str, stdout);
}

static char* read_file(const char* file, size_t* size) {
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

static bool process_file(const char* file) {
    size_t file_size = 0;
    char* file_data = read_file(file, &file_size);
    if (!file_data) {
        log_error(&global_log.log, NULL, "cannot read file '{0:s}'", { .s = file });
        return false;
    }

    mpool_t* pool = mpool_create();
    default_log_t file_log = log_create_default(file, global_log.log.colorize);
    lexer_t lexer = {
        .tmp  = 0,
        .str  = file_data,
        .size = file_size,
        .row  = 1,
        .col  = 1,
        .log  = &file_log.log
    };
    parser_t parser = {
        .lexer = &lexer,
        .pool  = &pool,
        .log   = &file_log.log
    };

    // Parse program
    ast_t* ast = parse(&parser);
    bool ok = !file_log.log.errs;
    if (ok) {
        // Bind identifiers to AST nodes
        id2ast_t id2ast = id2ast_create();
        binder_t binder = {
            .env = NULL,
            .log = &file_log.log
        };
        id2ast_destroy(&id2ast);
        bind(&binder, ast);
        ok &= !file_log.log.errs;
    }

    mod_t* mod = mod_create();
    if (ok) {
        // Perform type checking
        checker_t checker = {
            .log = &file_log.log,
            .mod = mod
        };
        infer(&checker, ast);
        ok &= !file_log.log.errs;
    }

    // Display program on success
    if (ok) {
        file_printer_t file_printer = printer_from_file(stdout);
        file_printer.printer.colorize = global_log.log.colorize;
        print(&file_printer.printer, "{0:a}\n", { .a = ast });
    }

    free(file_data);
    mpool_destroy(pool);
    mod_destroy(mod);
    return ok;
}

int main(int argc, char** argv) {
    // Detect if the standard output is a tty
    bool colorize = isatty(fileno(stdout)) && isatty(fileno(stderr));
    global_log = log_create_default(NULL, colorize);

    if (argc <= 1) {
        log_error(&global_log.log, NULL, "no input files");
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
                log_error(&global_log.log, NULL, "unknown option '{0:s}'", { .s = argv[i] });
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
