#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stddef.h>

#include "anf.h"
#include "print.h"

typedef struct log_s log_t;
typedef struct default_log_s default_log_t;
typedef void (*logfn_t) (log_t*, uint32_t, const loc_t*, const char*, const fmt_arg_t*);

enum log_tag_e {
    LOG_ERR,
    LOG_WARN,
    LOG_NOTE
};

struct log_s {
    size_t  errs;
    size_t  warns;
    bool    colorize;
    logfn_t log_fn;
};

struct default_log_s {
    log_t log;
    const char* file;
};

default_log_t log_create_default(const char*, bool);
log_t log_create_silent(void);

void log_format(log_t*, uint32_t, const loc_t*, const char*, const fmt_arg_t*);

#define log_format(log, type, loc, fmt, ...) \
    do { \
        fmt_arg_t args[] = { \
            { .u64 = 0 }, \
            __VA_ARGS__ \
        }; \
        (log)->log_fn(log, type, loc, fmt, sizeof(args) > sizeof(args[0]) ? args + 1 : NULL); \
    } while (false)

#define log_error(log, loc, fmt, ...) do { (log)->errs++;  log_format(log, LOG_ERR,  loc, fmt, __VA_ARGS__); } while (false)
#define log_warn(log, loc, fmt, ...)  do { (log)->warns++; log_format(log, LOG_WARN, loc, fmt, __VA_ARGS__); } while (false)
#define log_note(log, loc, fmt, ...)  log_format(log, LOG_NOTE, loc, fmt, __VA_ARGS__)

#endif // LOG_H
