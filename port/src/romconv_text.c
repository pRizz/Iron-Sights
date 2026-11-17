/**
 * The text bank converter (port_romconv.h; LP64-004 Task 4).
 *
 * A text bank is a table of big-endian, bank-relative 4-byte offsets followed
 * by the strings. At 32 bits the game rewrites each slot in place as the
 * string's address (romdataTextPreprocess), and langGet returns the slot. A
 * 4-byte slot cannot hold a 64-bit address, so at 64 bits each slot stays an
 * offset, in native byte order, and langGet adds the bank's address, as its
 * N64 branch does. The port's relabels (romdata.c's sTextPatches) are strings
 * outside the bank, which an offset cannot name: at 64 bits each is copied
 * over the string its slot names, which must be at least as long and share no
 * byte with another slot's string. In the stock ROM the three relabels of
 * LoptionsE fit their own strings.
 *
 * Compiled only at 64 bits and in the 32-bit oracle build. At 32 bits the
 * converter writes what the in-place rebase writes (addresses from BASE, a
 * patched slot the patch's address), and checks the relabels against the
 * 64-bit rule, so that the oracle checks the stock relabels.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv.h"

/* port_console.c: the game's log. */
extern void portConLogf(const char *fmt, ...);

#define TEXT_NO_OFFSET 0xFFFFFFFFu

/* Whether the C strings A and B are equal (the game's <string.h>, first on
 * the include path, declares no strcmp). */
static int textSameName(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static u32 textLength(const char *text)
{
    u32 n = 0;

    while (text[n] != '\0')
    {
        n++;
    }
    return n;
}

/* The length of the string at OFFSET (< SIZE), or SIZE - OFFSET when it has no
 * terminator inside the bank. */
static u32 textStringLength(const u8 *data, u32 size, u32 offset)
{
    u32 n = 0;

    while (offset + n < size && data[offset + n] != 0)
    {
        n++;
    }
    return n;
}

static int textAllZero(const u8 *data, u32 size)
{
    u32 i;

    for (i = 0; i < size; i++)
    {
        if (data[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

/* Whether PATCH applies to slot SLOT of BANK's table of NSLOTS slots. */
static int textPatchApplies(const struct PortTextPatch *patch, const char *bank, u32 nslots)
{
    return patch->slot < nslots && textSameName(patch->bank, bank);
}

/* The table's slot count, as the in-place rebase finds it: the smallest
 * non-zero offset among the words before it, / 4. Returns 0 for an empty bank
 * and -1 after portConvRefuse when the smallest offset is past the bank or not
 * a multiple of 4. Reads only. */
static s32 textMeasureTable(const u8 *data, u32 size, const char *bank)
{
    u32 first = TEXT_NO_OFFSET;
    u32 i;

    for (i = 0; i + 4 <= size && i < first; i += 4)
    {
        u32 offset = portConvBe32(data + i);

        if (offset != 0 && offset < first)
        {
            first = offset;
        }
    }
    if (first != TEXT_NO_OFFSET && first <= size && (first & 3) == 0)
    {
        return (s32) (first / 4);
    }
    if (textAllZero(data, size))
    {
        return 0;
    }
    portConvRefuse(PORT_LP64_TEXT_BANKS, bank,
            "no offset table: the smallest offset 0x%X is not a multiple of 4 inside the %u-byte bank", first, size);
    return -1;
}

/* Refuses, naming both slots, a patch of slot SLOT (its string at OFFSET,
 * LENGTH long and terminated) whose string shares a byte, terminator included,
 * with another slot's string. Reads only. */
static int textStringUnshared(const u8 *data, u32 size, u32 nslots, const char *bank, u32 slot, u32 offset,
        u32 length)
{
    u32 j;

    for (j = 0; j < nslots; j++)
    {
        u32 other = portConvBe32(data + j * 4);
        u32 end;

        if (j == slot || other == 0 || other >= size)
        {
            continue;
        }
        end = other + textStringLength(data, size, other);
        if (other <= offset + length && offset <= end)
        {
            portConvRefuse(PORT_LP64_TEXT_BANKS, bank,
                    "slot %u's relabel would also change slot %u's string (at 0x%X and 0x%X)", slot, j, offset, other);
            return 0;
        }
    }
    return 1;
}

/* Refuses, naming the slot, a patch that cannot be copied over its own string:
 * the slot names none, the string has no terminator inside the bank, it is
 * shorter than the patch, or another slot's string shares its bytes. Reads
 * only. */
static int textPatchFits(const u8 *data, u32 size, u32 nslots, const char *bank, const struct PortTextPatch *patch)
{
    u32 offset = portConvBe32(data + patch->slot * 4);
    u32 length, wanted;

    if (offset == 0 || offset >= size)
    {
        portConvRefuse(PORT_LP64_TEXT_BANKS, bank, "slot %u names no string in the %u-byte bank for its relabel",
                patch->slot, size);
        return 0;
    }
    length = textStringLength(data, size, offset);
    if (offset + length >= size)
    {
        portConvRefuse(PORT_LP64_TEXT_BANKS, bank, "slot %u's string at 0x%X has no terminator inside the bank",
                patch->slot, offset);
        return 0;
    }
    wanted = textLength(patch->text);
    if (wanted > length)
    {
        portConvRefuse(PORT_LP64_TEXT_BANKS, bank, "slot %u's relabel is %u characters, its string at 0x%X only %u",
                patch->slot, wanted, offset, length);
        return 0;
    }
    return textStringUnshared(data, size, nslots, bank, patch->slot, offset, length);
}

/* The slot's run-time value for the file offset OFFSET: 0 for 0 or an offset
 * at or past SIZE (logged), else the native offset (64 bits) or the address
 * BASE + OFFSET (32 bits). */
static u32 textSlotValue(u32 offset, u32 size, uintptr_t base, const char *bank, u32 slot)
{
    if (offset == 0)
    {
        return 0;
    }
    if (offset >= size)
    {
        portConLogf("ge: romdata: text %s: slot %u offset 0x%X beyond the %u-byte bank - zeroed\n", bank, slot, offset,
                size);
        return 0;
    }
#if IS_64_BIT
    (void) base;
    return offset;
#else
    return (u32) (base + offset);
#endif
}

s32 portTextBankConvert(u8 *data, u32 size, uintptr_t base, const char *bank, const struct PortTextPatch *patches,
        u32 npatches)
{
    s32 measured = textMeasureTable(data, size, bank);
    u32 nslots, i, p;
    u8 *table;

    if (measured <= 0)
    {
        return measured;
    }
    nslots = (u32) measured;
    for (p = 0; p < npatches; p++)
    {
        if (textPatchApplies(&patches[p], bank, nslots) && !textPatchFits(data, size, nslots, bank, &patches[p]))
        {
            return -1;
        }
    }
    /* The table is read from a copy while it is rewritten in place. */
    table = portConvScratch(nslots * 4);
    memcpy(table, data, nslots * 4);
    for (i = 0; i < nslots; i++)
    {
        u32 v = textSlotValue(portConvBe32(table + i * 4), size, base, bank, i);

#if !IS_64_BIT
        for (p = 0; p < npatches; p++)
        {
            if (patches[p].slot == i && textPatchApplies(&patches[p], bank, nslots))
            {
                v = (u32) (uintptr_t) patches[p].text;
            }
        }
#endif
        memcpy(data + i * 4, &v, sizeof(v));
    }
#if IS_64_BIT
    /* In order, so that of two patches of one slot the last is read, as at 32
     * bits. Each fits its string, checked above; the strings follow the table. */
    for (p = 0; p < npatches; p++)
    {
        if (textPatchApplies(&patches[p], bank, nslots))
        {
            memcpy(data + portConvBe32(table + patches[p].slot * 4), patches[p].text, textLength(patches[p].text) + 1);
        }
    }
#endif
    return measured;
}
