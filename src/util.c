#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>

#include "util.h"

#define FMT_BUF_SIZE 1024

void die(const char* msg) {
    fputs(msg, stderr);
    abort();
}

void* xmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void* ptr = malloc(size);
    if (!ptr)
        die("out of memory, malloc() failed\n");
    return ptr;
}

void* xcalloc(size_t num, size_t size) {
    if (size == 0 || num == 0)
        return NULL;
    void* ptr = calloc(num, size);
    if (!ptr)
        die("out of memory, calloc() failed\n");
    return ptr;
}

void* xrealloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, size);
    if (!ptr)
        die("out of memory, realloc() failed\n");
    return ptr;
}

size_t try_format(char* buf, size_t buf_len, const char* fmt, const fmt_arg_t* args, uint32_t flags) {
    const char* ptr = fmt;
    size_t len = 0;

    // Reserve space for null terminator
    buf_len--;

    while (*ptr && len < buf_len) {
        while (*ptr && *ptr != '{' && len < buf_len) buf[len++] = *(ptr++);
        if (len == buf_len)
            break;
        if (*ptr == '{') {
            ptr++;
            if (*ptr == '{') {
                buf[len++] = '{';
                ptr++;
                continue;
            } else if (*ptr == '$') {
                ptr++;
                const char* color = NULL;
                size_t color_len = 0;
                switch (*ptr) {
                    case 'k':
                        assert(!strncmp(ptr, "key", 3)); ptr += 3;
                        color = "\33[34;1m";
                        color_len = 7;
                        break;
                    case 'e':
                        assert(!strncmp(ptr, "err", 3)); ptr += 3;
                        color = "\33[31;1m";
                        color_len = 7;
                        break;
                    case 'l':
                        if (ptr[1] == 'i') {
                            assert(!strncmp(ptr, "lit", 3)); ptr += 3;
                            color = "\33[36;1m";
                            color_len = 7;
                        } else {
                            assert(!strncmp(ptr, "loc", 3)); ptr += 3;
                            color = "\33[37;1m";
                            color_len = 7;
                        }
                        break;
                    case 'i':
                        assert(!strncmp(ptr, "id", 2)); ptr += 2;
                        color = "\33[33m";
                        color_len = 5;
                        break;
                    case 'w':
                        assert(!strncmp(ptr, "wrn", 3)); ptr += 3;
                        color = "\33[33;1m";
                        color_len = 7;
                        break;
                    case 'n':
                        assert(!strncmp(ptr, "not", 3)); ptr += 3;
                        color = "\33[36;1m";
                        color_len = 7;
                        break;
                    case '}':
                        color = "\33[0m";
                        color_len = 4;
                        break;
                    default:
                        assert(false);
                        break;
                }
                // Do not write color codes partially to avoid problems
                if (len + color_len <= buf_len) {
                    if (flags & FMT_COLORIZE) memcpy(buf + len, color, color_len);
                    len += color_len;
                }
            } else if (isdigit(*ptr)) {
                size_t id = strtoul(ptr, (char**)&ptr, 10);
                int n = 0;
                assert(*ptr == ':');
                ptr++;
                switch (*ptr) {
                    case 'u':
                        ptr++;
                        switch (*ptr) {
                            case '8':
                                n = snprintf(buf + len, buf_len - len, "%"PRIu8, args[id].u8);
                                break;
                            case '1':
                                ptr++; assert(*ptr == '6');
                                n = snprintf(buf + len, buf_len - len, "%"PRIu16, args[id].u16);
                                break;
                            case '3':
                                ptr++; assert(*ptr == '2');
                                n = snprintf(buf + len, buf_len - len, "%"PRIu32, args[id].u32);
                                break;
                            case '6':
                                ptr++; assert(*ptr == '4');
                                n = snprintf(buf + len, buf_len - len, "%"PRIu64, args[id].u64);
                                break;
                        }
                         ptr++;
                        break;
                    case 'i':
                        ptr++;
                        switch (*ptr) {
                            case '8':
                                n = snprintf(buf + len, buf_len - len, "%"PRIi8, args[id].i8); ptr++;
                                break;
                            case '1':
                                ptr++; assert(*ptr == '6');
                                n = snprintf(buf + len, buf_len - len, "%"PRIi16, args[id].i16);
                                break;
                            case '3':
                                ptr++; assert(*ptr == '2');
                                n = snprintf(buf + len, buf_len - len, "%"PRIi32, args[id].i32);
                                break;
                            case '6':
                                ptr++; assert(*ptr == '4');
                                n = snprintf(buf + len, buf_len - len, "%"PRIi64, args[id].i64);
                                break;
                        }
                        ptr++;
                        break;
                    case 'p':
                        n = snprintf(buf + len, buf_len - len, "%"PRIxPTR, (intptr_t)args[id].ptr); ptr++;
                        break;
                    case 'b':
                        n = snprintf(buf + len, buf_len - len, "%s", args[id].b ? "true" : "false"); ptr++;
                        break;
                    case 's':
                        {
                            const char* str = args[id].str;
                            while (*str && len < buf_len)
                                buf[len++] = *(str++);
                            ptr++;
                        }
                        break;
                    case 'c':
                        buf[len++] = args[id].c;
                        ptr++;
                        break;
                    default:
                        assert(false);
                        break;
                }
                assert(n >= 0);
                len += n;
            } else {
                assert(false);
            }
            assert(*ptr == '}');
            ptr++;
        }
    }

    buf[len] = 0;    
    return len;
}

size_t format(char** buf_ptr, size_t buf_len, const char* fmt, const fmt_arg_t* args, uint32_t flags) {
    size_t len = try_format(*buf_ptr, buf_len, fmt, args, flags);
    if (len >= buf_len) {
        char* buf = NULL;
        buf_len++; // Ensure > 0
        do {
            free(buf);
            buf_len = buf_len * 2;
            buf = xmalloc(buf_len);
            len = try_format(buf, buf_len, fmt, args, flags);
        } while (len + 1 >= buf_len);
        *buf_ptr = buf;
    }
    return len;
}

size_t format_to_file(FILE* fp, const char* fmt, const fmt_arg_t* args, uint32_t flags) {
    char tmp[FMT_BUF_SIZE];
    char* buf = tmp;
    size_t len = format(&buf, FMT_BUF_SIZE, fmt, args, flags);
    fwrite(buf, sizeof(char), len, fp);
    if (buf != tmp)
        free(buf);
    return len;
}
