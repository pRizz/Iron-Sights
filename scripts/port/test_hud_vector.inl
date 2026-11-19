/* Included by test_hud.c: the HUD vector command's words (G_HUD_VECTOR_EXT,
 * phVector). At 64 bits a Gfx word is 8 bytes; each of the three commands
 * after the opcode carries one float in the low half of each word, and the
 * upper half must be zero, not what the display-list buffer held before
 * (LP64-005 Task 11). */

s32 dynGetFreeGfx2(Gfx *gdl) { (void) gdl; return 0x10000; }

static void testHudVectorWholeWords(void)
{
    /* Arrange: a buffer whose every byte is set, as a reused list would be. */
    Gfx list[8];
    const f32 expected[6] = {12.5f, -3.0f, 2.0f, 0.75f, 1.5f, 0.0f};
    Gfx *end;
    s32 i;
    memset(list, 0xa5, sizeof(list));
    /* Act */
    end = portHudReticleDashed(list, 12.5f, -3.0f, 2.0f, 0.75f, 0x11223344u, 1.5f);
    /* Assert: four commands; the opcode word, then whole words holding each float. */
    assert(end == list + 4);
    assert(list[0].words.w0 == (((uintptr_t) 0x4e << 24) | 8));
    assert(list[0].words.w1 == 0x11223344u);
    for (i = 0; i < 6; i++) {
        const uintptr_t word = i % 2 ? list[1 + i / 2].words.w1 : list[1 + i / 2].words.w0;
        u32 bits;
        memcpy(&bits, &expected[i], sizeof(bits));
        if (word != bits) {
            fprintf(stderr, "test_hud: vector word %d is %#llx, expected %#x\n", i,
                    (unsigned long long) word, bits);
            abort();
        }
    }
}
