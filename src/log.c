#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "util.h"

#define LOG_BUF_SIZE 256

static void log_default(uint32_t type, log_t* log, const loc_t* loc, const char* msg) {
    FILE* out = type == LOG_ERR ? stderr : stdout;
    uint32_t fmt_flags = log->colorize ? FMT_COLORIZE : 0;
    const char* file = ((default_log_t*)log)->file;
    switch (type) {
        case LOG_ERR:  format_to_file(out, "{$err}error{$}",   NULL, fmt_flags); break;
        case LOG_WARN: format_to_file(out, "{$wrn}warning{$}", NULL, fmt_flags); break;
        case LOG_NOTE: format_to_file(out, "{$not}note{$}",    NULL, fmt_flags); break;
        default:
            assert(false);
            break;
    }
    if (loc && file) {
        fmt_arg_t args[] = {
            { .str = file },
            { .u32 = loc->brow },
            { .u32 = loc->bcol },
            { .u32 = loc->erow },
            { .u32 = loc->ecol },
            { .str = msg }
        };
        bool range_loc = loc->brow != loc->erow || loc->bcol != loc->ecol;
        format_to_file(out,
            range_loc ? " in {$loc}{0:s}({1:u32},{2:u32} - {3:u32},{4:u32}{$}: {5:s}\n" : " in {$loc}{0:s}({1:u32},{2:u32}){$}: {5:s}\n",
            args, fmt_flags);
    } else {
        fmt_arg_t args = { .str = msg };
        format_to_file(out, ": {0:s}\n", &args, fmt_flags);
    }
}

default_log_t log_create_default(const char* file, bool colorize) {
    return (default_log_t) {
        .log = (log_t) {
            .log_fn   = log_default,
            .colorize = colorize,
            .errs     = 0,
            .warns    = 0
        },
        .file = file
    };
}

static void log_silent(uint32_t type, log_t* data, const loc_t* loc, const char* msg) {
    // Do nothing (avoid "unused variable" warnings)
    (void)type,(void)data,(void)loc,(void)msg;
}

log_t log_create_silent(void) {
    return (log_t) {
        .log_fn   = log_silent,
        .colorize = false,
        .errs     = 0,
        .warns    = 0
    };
}

void log_format(log_t* log, uint32_t type, const loc_t* loc, const char* fmt, const fmt_arg_t* args) {
    char tmp[LOG_BUF_SIZE];
    char* buf = tmp;
    format(&buf, LOG_BUF_SIZE, fmt, args, log->colorize);
    log->log_fn(type, log, loc, buf);
    if (buf != tmp)
        free(buf);
}
