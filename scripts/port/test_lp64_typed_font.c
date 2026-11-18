/* ROM-free check that the debug cursor of options.c reads a font character's
 * height by name at each pointer width (LP64-005 Task 4,
 * test_lp64_typed_reads.sh). options.c is included whole, with stubs for what
 * sub_GAME_7F0A4B40 calls; --gc-sections drops the rest. */
#include <stdio.h>
#include "../../src/game/options.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_font: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

static struct font gothic;
struct font *ptrFontBankGothic = &gothic;
struct fontchar *ptrFontBankGothicChars = gothic.chars;

s8 joyGetStickX(s8 contpadnum) { return 0; }
s8 joyGetStickY(s8 contpadnum) { return 0; }
Gfx *microcode_constructor(Gfx *gdl) { return gdl; }
void textMeasure(s32 *textheight, s32 *textwidth, char *text, struct fontchar *font1, struct font *font2, s32 lineheight)
{
    *textheight = 0;
    *textwidth = 0;
}
Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour, s32 width,
                s32 height, u32 yOffset, s32 lineheight)
{
    return gdl;
}

/* The cursor moves up by character 91's height, less one. */
static void testCursorStepIsCharacterHeight(void)
{
    // Arrange: character 91 is 7 high; the cursor is at y 50.
    static Gfx dl[16];
    gothic.chars[91].height = 7;
    D_80040B4C = 50;

    // Act
    sub_GAME_7F0A4B40(dl);

    // Assert
    expect(D_80040B4C == 50 - 7 + 1, "sub_GAME_7F0A4B40 reads chars[91].height");
}

int main(void)
{
    testCursorStepIsCharacterHeight();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_font: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
