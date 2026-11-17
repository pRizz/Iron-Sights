/**
 * The display-list expanders (port_romconv.h; LP64-004 Task 8, the textures
 * class).
 *
 * A ROM display-list command is two big-endian 32-bit words, 8 bytes; a host
 * Gfx is two pointer-sized words. At 32 bits the two are the same size and
 * the game swaps the words in place (texCopyGdls); at 64 bits each command
 * needs twice its bytes, so it is written as a new Gfx, both words
 * zero-extended. Each command's words are read into unsigned 32-bit locals
 * before either is written, which keeps a command that overlaps its own
 * destination correct and makes the zero-extension explicit.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build, where texCopyGdls
 * checks portGfxFromRomBackward against its own copy of every room list.
 */
#include <stddef.h>
#include <stdint.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv.h"
#ifdef GE_LP64_ORACLE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_lp64_oracle.h"
#endif

#define ROM_COMMAND_BYTES 8u

/* Writes one command, both words zero-extended into their pointer-sized slots. */
static void gfxPut(Gfx *dst, u32 w0, u32 w1)
{
    dst->words.w0 = (uintptr_t) w0;
    dst->words.w1 = (uintptr_t) w1;
}

void portGfxFromRom(Gfx *dst, const u8 *src, u32 commands)
{
    u32 i;

    for (i = 0; i < commands; i++, src += ROM_COMMAND_BYTES)
    {
        u32 w0 = portConvBe32(src);
        u32 w1 = portConvBe32(src + 4);

        gfxPut(&dst[i], w0, w1);
    }
}

void portGfxFromRomBackward(Gfx *dst, const u8 *src, u32 commands)
{
    u32 i = commands;

    while (i > 0)
    {
        const u8 *command;
        u32 w0, w1;

        i--;
        command = src + (size_t) i * ROM_COMMAND_BYTES;
        w0 = portConvBe32(command);
        w1 = portConvBe32(command + 4);
        gfxPut(&dst[i], w0, w1);
    }
}

void portGfxFromWords(Gfx *dst, const u32 *words, u32 commands)
{
    u32 i;

    for (i = 0; i < commands; i++, words += 2)
    {
        u32 w0 = words[0];
        u32 w1 = words[1];

        gfxPut(&dst[i], w0, w1);
    }
}

#ifdef GE_LP64_ORACLE
/* What portGfxCopyOracleBefore converted, waiting for the in-place result:
 * s_Block is freed after, s_Result is the converter's DST inside it. */
static u8 *s_Block;
static const u8 *s_Result;
static const Gfx *s_Dst;
static u32 s_Bytes;
static u32 s_Lists;

static void gfxOracleClear(void)
{
    free(s_Block);
    s_Block = NULL;
    s_Result = NULL;
    s_Dst = NULL;
}

void portGfxCopyOracleBefore(const Gfx *src, const Gfx *dst, s32 bytes)
{
    const u8 *s = (const u8 *) src;
    const u8 *d = (const u8 *) dst;
    u32 commands, srcbytes;
    const u8 *start, *end;

    gfxOracleClear();
    if (bytes <= 0)
    {
        return;
    }
    commands = (u32) bytes / ROM_COMMAND_BYTES;
    srcbytes = commands * ROM_COMMAND_BYTES;
    s_Bytes = commands * (u32) sizeof(Gfx);
    s_Lists++;
    if ((uintptr_t) d + s_Bytes <= (uintptr_t) s || (uintptr_t) s + srcbytes <= (uintptr_t) d)
    {
        /* Apart: the source's bytes after room for the result. A room block
         * can be megabytes, so the span between the two is not copied. */
        s_Block = (u8 *) malloc((size_t) s_Bytes + srcbytes);
        if (s_Block == NULL)
        {
            portLp64OracleResult(PORT_LP64_TEXTURES, "room list", 0, "the oracle could not allocate the copy");
            return;
        }
        memcpy(s_Block + s_Bytes, s, srcbytes);
        portGfxFromRomBackward((Gfx *) s_Block, s_Block + s_Bytes, commands);
        s_Result = s_Block;
    }
    else
    {
        /* Overlapping: the span both cover, converted at the same offsets. */
        start = (uintptr_t) s < (uintptr_t) d ? s : d;
        end = (uintptr_t) s + srcbytes > (uintptr_t) d + s_Bytes ? s + srcbytes : d + s_Bytes;
        s_Block = (u8 *) malloc((size_t) (end - start));
        if (s_Block == NULL)
        {
            portLp64OracleResult(PORT_LP64_TEXTURES, "room list", 0, "the oracle could not allocate the copy");
            return;
        }
        memcpy(s_Block, start, (size_t) (end - start));
        portGfxFromRomBackward((Gfx *) (s_Block + (d - start)), s_Block + (s - start), commands);
        s_Result = s_Block + (d - start);
    }
    s_Dst = dst;
}

void portGfxCopyOracleAfter(void)
{
    char file[32];

    if (s_Result == NULL || s_Dst == NULL)
    {
        gfxOracleClear();
        return;
    }
    snprintf(file, sizeof(file), "room list %u", s_Lists);
    portLp64OracleBytes(PORT_LP64_TEXTURES, file, s_Dst, s_Result, s_Bytes);
    gfxOracleClear();
}
#endif
