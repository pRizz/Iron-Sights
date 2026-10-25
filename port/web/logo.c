/* Logo payloads belong to the user's ROM, not the downloaded Wasm. */
#include "logo.h"
#include <stdlib.h>
#include <string.h>
#include "port_console.h"
#include "romdata.h"

s32 portWebCopyLogoBlocks(const u8 *rom, u32 size,
                         const struct port_web_logo_block *blocks, u32 count)
{
    u32 index;
    if (!rom || !blocks) return 0;
    /* Validate every destination before changing any backing buffer. */
    for (index = 0; index < count; index++) {
        const struct port_web_logo_block *block = &blocks[index];
        if (!block->data || block->offset > size || block->size > size - block->offset) return 0;
        if (block->kind == PORT_LOGO_WORDS && block->size % 4) return 0;
        if (block->kind == PORT_LOGO_VERTICES && block->size % 16) return 0;
        if (block->kind < PORT_LOGO_BYTES || block->kind > PORT_LOGO_VERTICES) return 0;
    }
    for (index = 0; index < count; index++) {
        const struct port_web_logo_block *block = &blocks[index];
        const u8 *source = rom + block->offset;
        u8 *target = block->data;
        u32 at;
        memcpy(target, source, block->size);
        if (block->kind == PORT_LOGO_WORDS) {
            for (at = 0; at < block->size; at += 4) {
                u32 word = ((u32)source[at] << 24) | ((u32)source[at+1] << 16)
                         | ((u32)source[at+2] << 8) | source[at+3];
                memcpy(target + at, &word, 4);
            }
        } else if (block->kind == PORT_LOGO_VERTICES) {
            for (at = 0; at < block->size; at += 16) {
                u32 field;
                for (field = 0; field < 12; field += 2) {
                    u16 word = ((u16)source[at+field] << 8) | source[at+field+1];
                    memcpy(target + at + field, &word, 2);
                }
            }
        }
    }
    return 1;
}

void portWebLoadRarewareLogo(void)
{
    u32 size = 0;
    const u8 *rom = romdataSeg("rarewarelogo", &size);
    if (!portWebCopyLogoBlocks(rom, size, portWebLogoBlocks, portWebLogoBlockCount)) {
        portConLogf("ge: invalid ROM logo segment; refusing an incomplete title\n");
        exit(3);
    }
    portConLogf("ge: logo textures and vertices loaded from the player ROM\n");
}
