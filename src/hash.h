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

static inline uint32_t hash_ptr(uint32_t h, const void* ptr) {
    uintptr_t uptr = (uintptr_t)ptr;
    for (size_t i = 0; i < sizeof(const void*); ++i, uptr >>= 8) {
        h = hash_uint8(h, uptr & 0xFF);
    }
    return h;
}

#define hash(h, X) _Generic((X), \
    uint8_t:  hash_uint8, \
    uint16_t: hash_uint16, \
    uint32_t: hash_uint32, \
    uint64_t: hash_uint64, \
    default: hash_ptr)(h, X)

#endif // HASH_H
