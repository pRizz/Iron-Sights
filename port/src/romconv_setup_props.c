/**
 * The setup file converter's prop records (port_romconv_stage.h). In LP64-004
 * Task 14 these are stubs: they accept only a prop list that is the end
 * record alone, so no stock setup converts yet; Task 15 replaces them with
 * the records' converter. Kept in their own file so that a test can link
 * another prop converter beside the rest of the setup converter. Compiled
 * only at 64 bits and in the 32-bit oracle build.
 */
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "bondconstants.h"
#include "port_romconv_stage.h"

/* The end record's one word, padded to a pointer as Task 15 pads every
 * record (to two words at 64 bits; at 32 bits records keep their size). */
#define PROPS_END_FILE_BYTES 4u
#define PROPS_END_OUT_BYTES ((u32) sizeof(void *))

static int isEndAlone(const u8 *props, u32 size)
{
    return props != NULL && size == PROPS_END_FILE_BYTES && props[3] == PROPDEF_END;
}

u32 portSetupPropsSize(const u8 *props, u32 size)
{
    return isEndAlone(props, size) ? PROPS_END_OUT_BYTES : 0;
}

s32 portSetupPropsConvert(const u8 *props, u32 size, u8 *out, u32 outsize)
{
    if (!isEndAlone(props, size) || out == NULL || outsize < PROPS_END_OUT_BYTES)
    {
        return 0;
    }
    memset(out, 0, PROPS_END_OUT_BYTES);
    /* {u16 scale; u8 hidden; u8 type}: the scale to native order, as
     * romfile_setup.c's KSK. */
    out[0] = props[1];
    out[1] = props[0];
    out[2] = props[2];
    out[3] = props[3];
    return 1;
}
