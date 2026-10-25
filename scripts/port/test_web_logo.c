#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port/web/logo.c"

extern int memcmp(const void *, const void *, size_t);
void osSyncPrintf(const char *format, ...) { (void)format; abort(); }

/* Only the pure converter is exercised; normal startup supplies these. */
const struct port_web_logo_block portWebLogoBlocks[1];
const u32 portWebLogoBlockCount = 0;
const u8 *romdataSeg(const char *name, u32 *size) { (void)name; *size = 0; return NULL; }
void portConLogf(const char *format, ...) { (void)format; }

int main(void)
{
    const u8 rom[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                     0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    u32 word = 0;
    Vtx vertex;
    u8 colors[4] = {0};
    struct port_web_logo_block blocks[] = {
        {&word, 4, 0, PORT_LOGO_WORDS},
        {&vertex, 16, 0, PORT_LOGO_VERTICES},
        {colors, 4, 12, PORT_LOGO_BYTES},
    };
    u16 value;
    assert(sizeof(Vtx) == 16);
    assert(portWebCopyLogoBlocks(rom, sizeof(rom), blocks, 3));
    assert(word == 0x12345678);
    memcpy(&value, &vertex, 2);
    assert(value == 0x1234);
    memcpy(&value, (u8 *)&vertex + 10, 2);
    assert(value == 0x3344);
    assert(memcmp((u8 *)&vertex + 12, rom + 12, 4) == 0);
    assert(memcmp(colors, rom + 12, 4) == 0);
    word = 0xAABBCCDD;
    blocks[2].offset = 15;
    assert(!portWebCopyLogoBlocks(rom, sizeof(rom), blocks, 3));
    assert(word == 0xAABBCCDD); /* No partial mutation on a later invalid block. */
    blocks[2].offset = 0xFFFFFFFF;
    assert(!portWebCopyLogoBlocks(rom, sizeof(rom), blocks, 3));
    blocks[2].offset = 12;
    blocks[1].size = 15;
    assert(!portWebCopyLogoBlocks(rom, sizeof(rom), blocks, 3));
    puts("ROM-free logo endian/bounds/atomicity regression passed");
    return 0;
}
