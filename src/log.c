#include <stdio.h>
#include <stdarg.h>

#include "log.h"
#include "util.h"

#define MSG_BUF_SIZE 256

typedef struct default_log_s default_log_t;

struct default_log_s {
    const char* file;
    bool colorize;
};

static void log_default(uint32_t type, void* data, const loc_t* loc, const char* msg) {
    FILE* out = type == LOG_ERR ? stderr : stdout;
    const char* file = ((default_log_t*)data)->file;
    bool colorize    = ((default_log_t*)data)->colorize;
    switch (type) {
        case LOG_ERR:  fputs(COLORIZE(colorize, COLOR_ERR("error")), out); break;
        case LOG_WARN: fputs(COLORIZE(colorize, COLOR_WARN("warn")), out); break;
        case LOG_NOTE: fputs(COLORIZE(colorize, COLOR_NOTE("note")), out); break;
        default: break;
    }
    if (loc && file) {
        if (loc->brow != loc->erow || loc->bcol != loc->ecol) {
            const char* fmt = COLORIZE(colorize, " in ", COLOR_LOC("%s(%zu, %zu - %zu, %zu)"), ": %s\n");
            fprintf(out, fmt, file, loc->brow, loc->bcol, loc->erow, loc->ecol, msg);
        } else {
            const char* fmt = COLORIZE(colorize, " in ", COLOR_LOC("%s(%zu, %zu)"), ": %s\n");
            fprintf(out, fmt, file, loc->brow, loc->bcol, msg);
        }
    } else {
        fprintf(out, ": %s\n", msg);
    }
}

log_t log_create_default(const char* file, bool colorize) {
    default_log_t* data = malloc(sizeof(default_log_t));
    data->file = file;
    data->colorize = colorize;
    return (log_t) {
        .data   = data,
        .log_fn = log_default,
        .errs   = 0,
        .warns  = 0
    };
}

static void log_silent(uint32_t type, void* data, const loc_t* loc, const char* msg) {
    // Do nothing (avoid "unused variable" warnings)
    (void)type,(void)data,(void)loc,(void)msg;
}

log_t log_create_silent(void) {
    return (log_t) {
        .data   = NULL,
        .log_fn = log_silent,
        .errs   = 0,
        .warns  = 0
    };
}

void log_destroy(log_t* log) {
    if (log->data)
        free(log->data);
}

#define log_msg(log, type, loc, fmt) \
    { \
        char buf[MSG_BUF_SIZE]; \
        va_list args; \
        va_start(args, fmt); \
        vsnprintf(buf, MSG_BUF_SIZE, fmt, args); \
        log->log_fn(type, log->data, loc, buf); \
        va_end(args); \
    }

void log_error(log_t* log, const loc_t* loc, const char* fmt, ...) {
    log->errs++;
    log_msg(log, LOG_ERR, loc, fmt);
}

void log_warn(log_t* log, const loc_t* loc, const char* fmt, ...) {
    log->warns++;
    log_msg(log, LOG_WARN, loc, fmt);
}

void log_note(log_t* log, const loc_t* loc, const char* fmt, ...) {
    log_msg(log, LOG_NOTE, loc, fmt);
}
