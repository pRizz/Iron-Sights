/* The audio bank converter's input checks (LP64-004 Task 5, review fixes): no
 * two records reached may share a byte, since the write pass turns in-file
 * records to native order while it walks, and every ADPCM or raw wave's samples
 * must lie in the wave table. Each refusal comes from the measuring pass, names
 * the bank, and leaves the file, the table and the output untouched. Built and
 * run at each pointer width by test_romconv.sh, under the sanitizers at 64
 * bits. Every input is written here. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <PR/libaudio.h>
#include "platform_info.h"
#include "port_romconv_audio.h"
#include "test_romconv_bank_fixture.h"

/* A bank of two instruments in which sound S's envelope lies inside
 * instrument B, over its bend range (0x0200) and sound count (1): turned to
 * native order, those four bytes read as a sound count of 2, and the word after
 * B's one sound offset names sound T. */
enum
{
    O_BANK = 0x08,  /* instruments A and B */
    O_INSTA = 0x1C, /* sound S */
    O_INSTB = 0x30, /* sound S; then 0x60 at 0x44, outside B */
    O_SNDS = 0x50,  /* envelope at B + 4, the key map, the wave */
    O_SNDT = 0x60,  /* its own envelope, the key map, the wave */
    O_KEY = 0x70,
    O_WAVE = 0x78, /* ADPCM, no samples, no book or loop */
    O_ENV = 0x90,
};

static void buildOverlap(void)
{
    buildBank();
    memset(s_File, 0, sizeof(s_File));
    put16(0, AL_BANK_VERSION);
    put16(2, 1);
    put32(4, O_BANK);
    put16(O_BANK, 2);
    put32(O_BANK + 4, 22050);
    put32(O_BANK + 12, O_INSTA);
    put32(O_BANK + 16, O_INSTB);
    put16(O_INSTA + 14, 1);
    put32(O_INSTA + 16, O_SNDS);
    put16(O_INSTB + 12, 0x0200);
    put16(O_INSTB + 14, 1);
    put32(O_INSTB + 16, O_SNDS);
    put32(O_INSTB + 20, O_SNDT);
    putSound(O_SNDS, O_INSTB + 4, O_KEY, O_WAVE, 64, 127);
    putSound(O_SNDT, O_ENV, O_KEY, O_WAVE, 64, 127);
    putEnvelope(O_ENV, 1000, 2000, 3000, 90, 80);
}

static int testRefusesAnEnvelopeInsideAnInstrument(void)
{
    // Arrange: a converter without the rule measures this file and then, its
    // walk changed by the envelope's swap, writes past the measured bytes; a
    // buffer of exactly that size lets the sanitizer see it.
    u32 size;
    buildOverlap();
    size = measure(s_File);
    if (size != 0)
    {
        u8 *out = (u8 *) __builtin_malloc(size);
        convertInto(s_File, s_Table, out, size);
        __builtin_free(out);
        buildOverlap();
    }

    // Act and Assert
    return expectRefused("the instrument at 0x30 overlaps the envelope at 0x34",
            "an envelope inside an instrument not yet reached is refused when measuring");
}

static int testAcceptsRecordsSharedByOffset(void)
{
    // Arrange: the fixture's two sounds of instrument 0 share an envelope and a
    // wave table, and sounds 1 and 2 share a key map.
    buildBank();

    // Act
    ALBank *b = convert()->bankArray[0];

    // Assert
    ALSound *s0 = b->instArray[0]->soundArray[0], *s1 = b->instArray[0]->soundArray[1];
    return expect(s0->envelope == s1->envelope && s0->wavetable == s1->wavetable
            && s1->keyMap == b->instArray[1]->soundArray[0]->keyMap,
            "records reached twice at one offset as one kind are accepted");
}

/* One changed word of the fixture's wave tables each, refused, naming WHY. */
static const struct
{
    u32 at, value;
    const char *why, *what;
} kTableRefusals[] = {
    {F_WAVE1 + 4, TABLE_SIZE, "raw wave table at 0xBC has samples at 0x80 to 0x180, past the 0x100-byte wave table",
            "a raw wave whose samples run past the table is refused"},
    {F_WAVE1, 0xFFFFFFFE, "raw wave table at 0xBC has samples at 0xFFFFFFFE to 0x100000002",
            "a raw wave whose base + len wraps 32 bits is refused"},
    {F_WAVE0, 0xF0, "ADPCM wave table at 0xA8 has samples at 0xF0 to 0x110, past the 0x100-byte wave table",
            "an ADPCM wave whose samples run past the table is refused"},
};

static int testTableRefusals(void)
{
    int failures = 0;
    size_t r;

    for (r = 0; r < sizeof(kTableRefusals) / sizeof(kTableRefusals[0]); r++)
    {
        buildBank();
        put32(kTableRefusals[r].at, kTableRefusals[r].value);
        failures += expectRefused(kTableRefusals[r].why, kTableRefusals[r].what);
    }
    return failures;
}

static int testAcceptsARawWaveEndingAtTheTableEnd(void)
{
    // Arrange
    buildBank();
    put32(F_WAVE1, TABLE_SIZE - 4);
    memcpy(s_Table + TABLE_SIZE - 4, "\x05\x06\x07\x08", 4);

    // Act
    ALBankFile *f = convert();

    // Assert
    return expect(f != NULL && ((s16 *) (s_Table + TABLE_SIZE - 4))[1] == 0x0708,
            "a raw wave whose samples end at the table's end is converted");
}

/* LP64-007 Task 7: a wave of any other type also gets table + base, which the
 * mixer reads for len bytes. */
static int testRefusesAWaveOfAnotherTypePastTheTable(void)
{
    // Arrange: wave 0 made type 2, its 0x20 bytes from 0xF0
    buildBank();
    s_File[F_WAVE0 + 8] = 2;
    put32(F_WAVE0, 0xF0);

    // Act, Assert
    return expectRefused("type 2 wave table at 0xA8 has samples at 0xF0 to 0x110, past the 0x100-byte wave table",
            "a wave of another type whose samples run past the table is refused");
}

static int testAcceptsAWaveOfAnotherTypeInsideTheTable(void)
{
    // Arrange: wave 0 made type 2, its samples left inside the table
    buildBank();
    s_File[F_WAVE0 + 8] = 2;

    // Act
    ALBankFile *f = convert();

    // Assert
    ALWaveTable *wave = f != NULL ? f->bankArray[0]->instArray[0]->soundArray[0]->wavetable : NULL;
    return expect(wave != NULL && wave->type == 2 && wave->base >= s_Table && wave->base + wave->len <= s_Table + TABLE_SIZE,
            "a wave of another type whose samples lie in the table is converted");
}

int main(void)
{
    int failures = 0;

    failures += testRefusesAnEnvelopeInsideAnInstrument();
    failures += testAcceptsRecordsSharedByOffset();
    failures += testTableRefusals();
    failures += testAcceptsARawWaveEndingAtTheTableEnd();
    failures += testRefusesAWaveOfAnotherTypePastTheTable();
    failures += testAcceptsAWaveOfAnotherTypeInsideTheTable();
    return failures == 0 ? 0 : 1;
}
