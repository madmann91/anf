#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "anf.h"

typedef struct io_s      io_t;
typedef struct file_io_s file_io_t;
typedef struct mem_io_s  mem_io_t;

struct io_s {
    size_t (*read)(io_t*, void*, size_t);
    size_t (*write)(io_t*, const void*, size_t);
    void   (*seek)(io_t*, long, int);
    long   (*tell)(io_t*);
};

struct file_io_s {
    io_t io;
    FILE* fp;
};

struct mem_io_s {
    io_t io;
    void* buf;
    size_t size;
    long off;
};

file_io_t io_from_file(FILE*);
mem_io_t  io_from_buffer(void*, size_t);

bool mod_save(const mod_t* mod, io_t*);
mod_t* mod_load(io_t*, mpool_t**);

#endif // IO_H
