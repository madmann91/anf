#include <stdarg.h>

#include "bind.h"

#define ERR_BUF_SIZE 256

void bind(binder_t* binder, ast_t* ast) {
    switch (ast->tag) {
        default: break;
    }
}

void bind_error(binder_t* binder, const loc_t* loc, const char* fmt, ...) {
    char buf[ERR_BUF_SIZE];
    binder->errs++;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ERR_BUF_SIZE, fmt, args);
    binder->error_fn(binder, loc, buf);
    va_end(args);
}
