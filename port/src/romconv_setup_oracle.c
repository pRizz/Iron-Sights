/**
 * The setup file oracle (port_romconv_stage.h; LP64-004 Task 15), in the
 * 32-bit oracle build only (-DGE_LP64_ORACLE=ON).
 *
 * romdataFilePreprocess calls romfileSetupOracle in place of
 * romfileSetupConvert for every setup file a run loads: it keeps a copy of the
 * file, converts the file in place as always, converts the copy with
 * portSetupConvert into new memory, and compares the two images byte for byte
 * over the file's size. At 32 bits the images are equal except the credits
 * record's table word, which each pass writes as the table's address in its
 * own buffer; that word is compared as an offset from its buffer.
 *
 * portSetupOracleSweep counts, as differing, each setup file the ROM's
 * directory holds that the game does not serve: GE_ROMPROBE=1, which
 * lp64_oracle.py sets in the same boot, loads every served one through the
 * normal load path, and so through romfileSetupOracle.
 *
 * Its own file because port_lp64_oracle.c and the setup converter are linked
 * into the ROM-free tests, which have no file loader. Compiled with the other
 * romconv_ sources, and empty outside the oracle build.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "bondconstants.h"
#include "ob.h"
#include "romdata.h"
#include "port_romconv_stage.h"
#include "port_lp64_oracle.h"
#include "../generated/setup_kinds.h"

#ifdef GE_LP64_ORACLE

/* port_console.c: the game's log (stderr plus the tilde console ring). */
extern void portConLogf(const char *fmt, ...);
extern s32 romfileSetupConvert(u8 *data, u32 size, const char *name, const u8 *compiled); /* romfile_setup.c */

#define ORACLE_INTRO_OFFSET 8u /* struct stagesetup's intro field */

static u32 readNative32(const u8 *p)
{
    u32 v;

    memcpy(&v, p, 4);
    return v;
}

/* Rewrites each credits record's table word in IMAGE (SIZE bytes, converted,
 * native) from an address in IMAGE to one in BASE, by the intro walk the game
 * makes; returns 0 when that walk leaves the image. */
static int rebaseCredits(u8 *image, u32 size, uintptr_t base)
{
    u32 at = readNative32(image + ORACLE_INTRO_OFFSET);

    for (;;)
    {
        u32 type;

        if ((u64) at + 4 > size)
        {
            return 0;
        }
        type = image[at + 3]; /* INTRO_TYPE: the top byte of the native type word */
        if (type >= SETUP_INTRO_TYPES || (u64) at + kSetupIntroKinds[type].words * 4u > size)
        {
            return 0;
        }
        if (type == INTROTYPE_CREDITS)
        {
            const u32 word = readNative32(image + at + 4) - (u32) (uintptr_t) image + (u32) base;

            memcpy(image + at + 4, &word, 4);
        }
        if (type == INTROTYPE_END)
        {
            return 1;
        }
        at += kSetupIntroKinds[type].words * 4u;
    }
}

s32 romfileSetupOracle(u8 *data, u32 size, const char *name, const u8 *compiled)
{
    u8 *copy = (u8 *) malloc(size);
    u8 *image = NULL;
    u32 need;
    s32 inplace;

    if (copy == NULL)
    {
        portLp64OracleResult(PORT_LP64_SETUPS, name, 0, "the oracle could not copy the file");
        return romfileSetupConvert(data, size, name, compiled);
    }
    memcpy(copy, data, size);
    inplace = romfileSetupConvert(data, size, name, compiled);
    need = portSetupConvertedSize(copy, size, name);
    image = need >= size ? (u8 *) malloc(need) : NULL;
    if (need == 0 || image == NULL || !portSetupConvert(copy, size, image, need, name))
    {
        portLp64OracleResult(PORT_LP64_SETUPS, name, 0, "the converter refused the file");
    }
    else if (!inplace)
    {
        portLp64OracleResult(PORT_LP64_SETUPS, name, 0, "the in-place pass refused the file");
    }
    else if (!rebaseCredits(image, need, (uintptr_t) data))
    {
        portLp64OracleResult(PORT_LP64_SETUPS, name, 0, "the converted intro walk leaves the image");
    }
    else
    {
        portLp64OracleBytes(PORT_LP64_SETUPS, name, data, image, size);
    }
    free(image);
    free(copy);
    return inplace;
}

/* The game's string.h has no strncmp. */
static int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}

void portSetupOracleSweep(void)
{
    s32 r, index;
    u32 held = 0;

    for (r = 0; r < romdataFileCount(); r++)
    {
        const struct romfile *file = romdataFile(r);

        if (file == NULL || file->size == 0 || !(startsWith(file->name, "Usetup") || startsWith(file->name, "Ump_setup")))
        {
            continue;
        }
        held++;
        index = fileGetIndex((u8 *) file->name);
        if (index <= 0 || !romdataFileServed(index))
        {
            portLp64OracleResult(PORT_LP64_SETUPS, file->name, 0, "the ROM holds it, but it is not served");
        }
    }
    portConLogf("ge: lp64: oracle: setup sweep: the ROM holds %u setup files\n", held);
}

#endif
