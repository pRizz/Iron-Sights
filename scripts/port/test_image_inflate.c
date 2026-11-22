/* ROM-free regression (LP64-006 Task 8): an odd-width 4-bit texture packs each
 * row's last pixel with the value after it, which for the last row is one past
 * what the decoder wrote. A 64-bit build must read zero there, not the stack's
 * leftovers; the 32-bit program keeps its leftovers, so that half only runs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondtypes.h>
#include "game/image.h"

/* The rest of the game image.c links against; the inflater calls none of them. */
u32 decompressdata(u8 *src, u8 *dst, void *table) { (void) src; (void) dst; (void) table; abort(); }
u8 *rzipGetSomething(void) { abort(); }
void ge_dbg_vec(const char *tag, const f32 *v, s32 n) { (void) tag; (void) v; (void) n; }
void osInvalDCache(void *p, s32 n) { (void) p; (void) n; }
void osWritebackDCacheAll(void) {}
uintptr_t osVirtualToPhysical(void *p) { return (uintptr_t) p; }
unsigned long long portProfNow(void) { return 0; }
void portHitchNoteItem(int cat, unsigned long long us, const char *what, unsigned int id) { (void) cat; (void) us; (void) what; (void) id; }
void portHwNoteLoad(u32 bytes, s32 compressed) { (void) bytes; (void) compressed; }
void portHwTexDecoded(s32 num, s32 zlib, s32 lod, s32 explicitlods) { (void) num; (void) zlib; (void) lod; (void) explicitlods; }
void portHwTexInfo(s32 format, s32 width, s32 height, s32 method, s32 colours, s32 images) { (void) format; (void) width; (void) height; (void) method; (void) colours; (void) images; }
void portLp64PoolExhausted(const char *pool, u32 wanted) { (void) pool; (void) wanted; }
void portLp64PoolUse(const char *pool, u32 used, u32 size) { (void) pool; (void) used; (void) size; }
void portLp64RequireConverter(s32 cls) { (void) cls; }
void *portRootFopen(const char *name, const char *mode) { (void) name; (void) mode; return NULL; }
void portTexCrosshairForget(void *tex) { (void) tex; }
void portTexCrosshairLoaded(void *tex) { (void) tex; }
s32 portTexReplaceRom(void *tex) { (void) tex; return 0; }
void romCopy(void *dst, void *src, u32 len) { (void) dst; (void) src; (void) len; abort(); }
void gfx_texture_cache_clear_c(void) {}
void *mempAllocBytesInBank(u32 bytes, u8 bank) { (void) bytes; (void) bank; abort(); }
u8 _GlobalimagetableSegmentStart[16], _GlobalimagetableSegmentEnd[16], _GlobalimagetableSegmentRomStart[16];
u8 port_imagesSegmentRomStart[16];

/* MSB first, as texReadBits reads them. */
static void put(u8 *buf, s32 *bit, u32 value, s32 count)
{
    while (count-- > 0) {
        if ((value >> count) & 1) buf[*bit >> 3] |= (u8) (0x80 >> (*bit & 7));
        (*bit)++;
    }
}

/* Leaves 0xFF where the inflater's frame will be. */
static __attribute__((noinline)) void dirty_stack(void)
{
    volatile u8 junk[0x8000];
    memset((void *) junk, 0xFF, sizeof(junk));
}

int main(void)
{
    /* Arrange: one I4 pixel of value 0xA, RLE-coded (one literal block). */
    u8 stream[16] = {0};
    s32 bit = 0;
    u8 pixels[64];
    struct tex record;
    struct texpool pool;
    s32 bytes;
    put(stream, &bit, TEXFORMAT_I4, 4);
    put(stream, &bit, 1, 8);  /* width */
    put(stream, &bit, 1, 8);  /* height */
    put(stream, &bit, TEXCOMPMETHOD_RLE, 4);
    put(stream, &bit, 0, 3);  /* back-reference field */
    put(stream, &bit, 0, 3);  /* run-length field */
    put(stream, &bit, 4, 4);  /* block size */
    put(stream, &bit, 0, 1);  /* a literal */
    put(stream, &bit, 0xA, 4);
    memset(pixels, 0x55, sizeof(pixels));
    memset(&record, 0, sizeof(record));
    memset(&pool, 0, sizeof(pool));
    pool.rightpos = &record;
    dirty_stack();

    /* Act */
    bytes = texInflateNonZlib(stream, pixels, 0, 0, &pool);

    /* Assert: the pixel after the image reads as zero at 64 bits. */
    if (bytes != 8 || record.width != 1 || record.height != 1) {
        fprintf(stderr, "test_image_inflate: decoded %d bytes, %dx%d\n", (int) bytes, (int) record.width, (int) record.height);
        return 1;
    }
    if (sizeof(void *) == 8 && pixels[0] != 0xA0) {
        fprintf(stderr, "test_image_inflate: last pixel pair 0x%02x, want 0xa0 (the value after the image must be zero)\n", pixels[0]);
        return 1;
    }
    printf("test_image_inflate: last pixel pair 0x%02x\n", pixels[0]);
    return 0;
}
