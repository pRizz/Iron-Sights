/* The room cache's part key (gfx_roomcache.inl, [display] room_cache = 2):
 * a part list's address and its vertex base, mixed into the one 64-bit word
 * that is the cache's whole identity for that part. Its own header so a
 * ROM-free test can check it at both widths (scripts/port/test_roomcache_key.c). */
#ifndef GFX_ROOMCACHE_KEY_H
#define GFX_ROOMCACHE_KEY_H

#include <stdint.h>
#include "platform_info.h"

static inline uint64_t roc_part_key(const void* dl, const void* vtx) {
#if IS_64_BIT
    /* PORT (LP64-005): rotated, not shifted: a shift by 20 drops a 64-bit
     * address's bits 44 and up, so two lists 16 TiB apart with one vertex
     * base shared a key and one replayed the other's capture */
    const uint64_t list = (uint64_t)(uintptr_t)dl;
    return ((list << 20) | (list >> 44)) ^ (uint64_t)(uintptr_t)vtx;
#else
    return ((uint64_t)(uintptr_t)dl << 20) ^ (uint64_t)(uintptr_t)vtx;
#endif
}

#endif
