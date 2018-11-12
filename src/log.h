#ifndef LOG_H
#define LOG_H

#include "anf.h"

typedef struct log_s log_t;
typedef void (*logfn_t) (uint32_t, void*, const loc_t*, const char*);

enum log_tag_e {
    LOG_ERR,
    LOG_WARN,
    LOG_NOTE
};

struct log_s {
    size_t  errs;
    size_t  warns;
    void*   data;
    logfn_t log_fn;
};

log_t log_create_default(const char*, bool);
log_t log_create_silent(void);
void log_destroy(log_t* log);

void log_error(log_t*, const loc_t*, const char*, ...);
void log_warn(log_t*, const loc_t*, const char*, ...);
void log_note(log_t*, const loc_t*, const char*, ...);

#endif // LOG_H
