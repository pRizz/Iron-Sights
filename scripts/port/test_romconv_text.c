/* The text bank converter (LP64-004 Task 4): portTextBankConvert rewrites a
 * bank's table of big-endian offsets in place, as native offsets at 64 bits
 * and as addresses at 32, applies the port relabels, and refuses, writing
 * nothing, a bank with no offset table or a relabel that does not fit its own
 * string. Built and run at each pointer width by test_romconv.sh. Every input
 * is written here. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform_info.h"
#include "port_romconv.h"

#define BANK_SIZE 0x18u
#define BANK_NAME "LtestE"

/* port_console.c's log, captured so the refusal line can be read back. */
static char s_Log[512];

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

static int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}

/* The game's <string.h> (first on the include path, as in the game build)
 * declares no memcmp. */
static int sameBytes(const void *a, const void *b, size_t size)
{
    const u8 *x = (const u8 *) a;
    const u8 *y = (const u8 *) b;
    size_t i;

    for (i = 0; i < size; i++)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}

/* Whether the terminated string at TEXT is EXPECTED (no strcmp is declared). */
static int sameString(const u8 *text, const char *expected)
{
    size_t i = 0;

    while (expected[i] != '\0' && text[i] == (u8) expected[i])
    {
        i++;
    }
    return expected[i] == '\0' && text[i] == 0;
}

static int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_text: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

/* A bank of 0x18 bytes: three big-endian slots naming 0x0C, nothing and
 * 0x12, then "ratio" at 0x0C and "16:9\n" at 0x12, each with its
 * terminator. */
static const u8 kBank[BANK_SIZE] = {
    0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12,
    'r', 'a', 't', 'i', 'o', 0x00, '1', '6', ':', '9', '\n', 0x00,
};

/* The bank's slot I as the converter left it, in native byte order. */
static u32 slot(const u8 *bank, u32 i)
{
    u32 v;

    memcpy(&v, bank + i * 4, sizeof(v));
    return v;
}

/* Puts OFFSET, big-endian, in slot I of BANK. */
static void setSlot(u8 *bank, u32 i, u32 offset)
{
    bank[i * 4] = (u8) (offset >> 24);
    bank[i * 4 + 1] = (u8) (offset >> 16);
    bank[i * 4 + 2] = (u8) (offset >> 8);
    bank[i * 4 + 3] = (u8) offset;
}

static s32 convert(u8 *bank, const struct PortTextPatch *patches, u32 npatches)
{
    return portTextBankConvert(bank, BANK_SIZE, (uintptr_t) bank, BANK_NAME, patches, npatches);
}

#define REFUSAL "ge: lp64: cannot convert text banks \"" BANK_NAME "\": "

/* Converts BANK, which must be refused, and checks that nothing was written
 * and the refusal names the class, the bank and the reason WHY. */
static int expectRefused(u8 *bank, const struct PortTextPatch *patches, u32 npatches, const char *why,
        const char *what)
{
    u8 before[BANK_SIZE];
    s32 slots;

    memcpy(before, bank, BANK_SIZE);
    s_Log[0] = '\0';
    slots = convert(bank, patches, npatches);
    return expect(slots == -1 && sameBytes(bank, before, BANK_SIZE) && startsWith(s_Log, REFUSAL)
            && startsWith(s_Log + sizeof(REFUSAL) - 1, why),
            what);
}

static int testReturnsTheSlotCount(void)
{
    // Arrange
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    s32 slots = convert(bank, NULL, 0);

    // Assert
    return expect(slots == 3, "a bank whose first string is at 0x0C has 3 slots");
}

static int testRewritesTheSlots(void)
{
    // Arrange
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    convert(bank, NULL, 0);

    // Assert
#if IS_64_BIT
    return expect(slot(bank, 0) == 0x0C && slot(bank, 1) == 0 && slot(bank, 2) == 0x12,
            "64-bit slots are native offsets, 0 kept");
#else
    return expect(slot(bank, 0) == (u32) (uintptr_t) (bank + 0x0C) && slot(bank, 1) == 0
            && slot(bank, 2) == (u32) (uintptr_t) (bank + 0x12),
            "32-bit slots are the strings' addresses, 0 kept");
#endif
}

static int testRebasesOnBase(void)
{
    // Arrange: BASE is not the bank's own address, as in the oracle, which
    // converts a copy with the real bank's address.
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    portTextBankConvert(bank, BANK_SIZE, 0x1000, BANK_NAME, NULL, 0);

    // Assert
#if IS_64_BIT
    return expect(slot(bank, 0) == 0x0C && slot(bank, 2) == 0x12, "64-bit slots do not depend on BASE");
#else
    return expect(slot(bank, 0) == 0x100C && slot(bank, 2) == 0x1012, "32-bit slots are BASE plus the offset");
#endif
}

static int testLeavesTheStringsUnpatched(void)
{
    // Arrange
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    convert(bank, NULL, 0);

    // Assert
    return expect(sameBytes(bank + 0x0C, kBank + 0x0C, BANK_SIZE - 0x0C), "without a patch no string is written");
}

static int testAppliesAPatch(void)
{
    // Arrange
    static const struct PortTextPatch patch = {BANK_NAME, 0, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    s32 slots = convert(bank, &patch, 1);

    // Assert
#if IS_64_BIT
    return expect(slots == 3 && slot(bank, 0) == 0x0C && sameString(bank + 0x0C, "hud\n"),
            "a 64-bit patch is copied over its slot's string");
#else
    return expect(slots == 3 && slot(bank, 0) == (u32) (uintptr_t) patch.text && sameString(bank + 0x0C, "ratio"),
            "a 32-bit patched slot holds the patch's address, its string untouched");
#endif
}

static int testPatchLeavesTheOtherString(void)
{
    // Arrange
    static const struct PortTextPatch patch = {BANK_NAME, 0, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    convert(bank, &patch, 1);

    // Assert
    return expect(sameBytes(bank + 0x12, kBank + 0x12, BANK_SIZE - 0x12), "a patch leaves slot 2's string untouched");
}

static int testIgnoresAnotherBanksPatch(void)
{
    // Arrange
    static const struct PortTextPatch patch = {"LotherE", 0, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act
    convert(bank, &patch, 1);

    // Assert
#if IS_64_BIT
    return expect(sameBytes(bank + 0x0C, kBank + 0x0C, BANK_SIZE - 0x0C), "a patch for another bank is not applied");
#else
    return expect(slot(bank, 0) == (u32) (uintptr_t) (bank + 0x0C), "a patch for another bank is not applied");
#endif
}

static int testZeroesAnOffsetPastTheBank(void)
{
    // Arrange
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);
    setSlot(bank, 2, BANK_SIZE);
    s_Log[0] = '\0';

    // Act
    s32 slots = convert(bank, NULL, 0);

    // Assert
    return expect(slots == 3 && slot(bank, 2) == 0
            && startsWith(s_Log, "ge: romdata: text " BANK_NAME ": slot 2 offset 0x18 beyond the 24-byte bank - zeroed"),
            "an offset at the bank's size becomes 0, logged");
}

static int testEmptyBank(void)
{
    // Arrange
    u8 bank[32] = {0};
    u8 zero[32] = {0};

    // Act
    s32 slots = portTextBankConvert(bank, sizeof(bank), (uintptr_t) bank, BANK_NAME, NULL, 0);

    // Assert
    return expect(slots == 0 && sameBytes(bank, zero, sizeof(bank)), "a bank of 32 zero bytes is empty, untouched");
}

static int testRefusesAnUnalignedTable(void)
{
    // Arrange
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);
    setSlot(bank, 0, 0x0D);

    // Act, Assert
    return expectRefused(bank, NULL, 0, "no offset table: the smallest offset 0xD ",
            "a first offset of 0x0D is refused, nothing written");
}

static int testRefusesATablePastTheBank(void)
{
    // Arrange: every non-zero offset is past the bank's end.
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);
    setSlot(bank, 0, 0x40);
    setSlot(bank, 2, 0x44);

    // Act, Assert
    return expectRefused(bank, NULL, 0, "no offset table: the smallest offset 0x40 ",
            "a smallest offset past the bank is refused, nothing written");
}

static int testRefusesALongerPatch(void)
{
    // Arrange
    static const struct PortTextPatch patch = {BANK_NAME, 0, "much longer\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act, Assert
    return expectRefused(bank, &patch, 1, "slot 0's relabel is 12 characters, its string at 0xC only 5",
            "a patch longer than its string is refused, nothing written");
}

static int testRefusesAPatchOfAnEmptySlot(void)
{
    // Arrange
    static const struct PortTextPatch patch = {BANK_NAME, 1, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);

    // Act, Assert
    return expectRefused(bank, &patch, 1, "slot 1 names no string",
            "a patch of a slot that names no string is refused, nothing written");
}

static int testRefusesAPatchOfASharedString(void)
{
    // Arrange: slot 1 names slot 0's string too.
    static const struct PortTextPatch patch = {BANK_NAME, 0, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);
    setSlot(bank, 1, 0x0C);

    // Act, Assert
    return expectRefused(bank, &patch, 1, "slot 0's relabel would also change slot 1's string",
            "a patch of a string another slot names is refused, nothing written");
}

static int testRefusesAPatchOverASuffix(void)
{
    // Arrange: slot 1 names "tio", the tail of slot 0's string.
    static const struct PortTextPatch patch = {BANK_NAME, 0, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);
    setSlot(bank, 1, 0x0E);

    // Act, Assert
    return expectRefused(bank, &patch, 1, "slot 0's relabel would also change slot 1's string",
            "a patch over another slot's tail string is refused, nothing written");
}

static int testRefusesAnUnterminatedString(void)
{
    // Arrange: slot 2's string runs to the bank's end with no terminator.
    static const struct PortTextPatch patch = {BANK_NAME, 2, "hud\n"};
    u8 bank[BANK_SIZE];
    memcpy(bank, kBank, BANK_SIZE);
    bank[BANK_SIZE - 1] = '!';

    // Act, Assert
    return expectRefused(bank, &patch, 1, "slot 2's string at 0x12 has no terminator",
            "a patch of an unterminated string is refused, nothing written");
}

int main(void)
{
    int failures = 0;

    failures += testReturnsTheSlotCount();
    failures += testRewritesTheSlots();
    failures += testRebasesOnBase();
    failures += testLeavesTheStringsUnpatched();
    failures += testAppliesAPatch();
    failures += testPatchLeavesTheOtherString();
    failures += testIgnoresAnotherBanksPatch();
    failures += testZeroesAnOffsetPastTheBank();
    failures += testEmptyBank();
    failures += testRefusesAnUnalignedTable();
    failures += testRefusesATablePastTheBank();
    failures += testRefusesALongerPatch();
    failures += testRefusesAPatchOfAnEmptySlot();
    failures += testRefusesAPatchOfASharedString();
    failures += testRefusesAPatchOverASuffix();
    failures += testRefusesAnUnterminatedString();
    return failures == 0 ? 0 : 1;
}
