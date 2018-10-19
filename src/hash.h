#ifndef HASH_H
#define HASH_H

#include <stdint.h>

static inline uint32_t hash_init() {
    return 0x811C9DC5;
}

static inline uint32_t hash_uint8(uint32_t h, uint8_t d) {
    return (h * 16777619) ^ d;
}

static inline uint32_t hash_uint16(uint32_t h, uint16_t d) {
    return hash_uint8(hash_uint8(h, d & 0xFF), d >> 8);
}

static inline uint32_t hash_uint32(uint32_t h, uint32_t d) {
    return hash_uint16(hash_uint16(h, d & 0xFFFF), d >> 16);
}

static inline uint32_t hash_uint64(uint32_t h, uint64_t d) {
    return hash_uint32(hash_uint32(h, d & ((uint64_t)0xFFFFFFFF)), d >> 32);
}

static inline uint32_t hash_bytes(uint32_t h, const void* ptr, size_t nbytes) {
    const uint8_t* byte_ptr = ptr;
    for (size_t i = 0; i < nbytes; ++i, byte_ptr++)
        h = hash_uint8(h, *byte_ptr);
    return h;
}

static inline uint32_t hash_ptr(uint32_t h, const void* ptr) {
    return hash_bytes(h, &ptr, sizeof(void*));
}

static inline uint32_t hash_str(uint32_t h, const char* ptr) {
    char c = *ptr;
    while (c != 0) {
        h = hash_uint8(h, c);
        c = *(++ptr);
    }
    return h;
}

#endif // HASH_H
