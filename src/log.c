#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "print.h"

#define LOG_BUF_SIZE 256

static void log_default(log_t* log, uint32_t type, const loc_t* loc, const char* fmt, const fmt_arg_t* args) {
    FILE* out = type == LOG_ERR ? stderr : stdout;
    file_printer_t file_printer = printer_from_file(out);
    file_printer.printer.colorize = log->colorize;
    printer_t* printer = &file_printer.printer;

    switch (type) {
        case LOG_ERR:  print(printer, "{$err}error{$}");   break;
        case LOG_WARN: print(printer, "{$wrn}warning{$}"); break;
        case LOG_NOTE: print(printer, "{$not}note{$}");    break;
        default:
            assert(false);
            break;
    }

    const char* file = ((default_log_t*)log)->file;
    if (loc && file) {
        bool range_loc = loc->brow != loc->erow || loc->bcol != loc->ecol;
        print(printer,
            range_loc ? " in {$loc}{0:s}({1:u32},{2:u32} - {3:u32},{4:u32}){$}: " : " in {$loc}{0:s}({1:u32},{2:u32}){$}: ",
            { .s = file },
            { .u32 = loc->brow },
            { .u32 = loc->bcol },
            { .u32 = loc->erow },
            { .u32 = loc->ecol }
        );
    } else {
        print(printer, ": ");
    }
    printer->format(printer, fmt, args);
    print(printer, "\n");
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

static void log_silent(log_t* data, uint32_t type, const loc_t* loc, const char* fmt, const fmt_arg_t* args) {
    // Do nothing (avoid "unused variable" warnings)
    (void)type,(void)data,(void)loc,(void)fmt,(void)args;
}

log_t log_create_silent(void) {
    return (log_t) {
        .log_fn   = log_silent,
        .colorize = false,
        .errs     = 0,
        .warns    = 0
    };
}
