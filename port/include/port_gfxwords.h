/**
 * Display-list command words at their own width (LP64-003 Task 7).
 *
 * A Gfx is two pointer-sized words, so 8 bytes each in a 64-bit build. The
 * first is always an N64 command word, whose upper half is zero: gbi.h's
 * macros and every hand-written writer build it from an unsigned 32-bit
 * value. The second holds either data (vertex indices, colours, sizes) or an
 * address. These accessors say which a site reads, so a 32-bit view of a word
 * is written once, explicitly, instead of as an implicit narrowing.
 *
 *   GFX_W0(g)       the command word, as the N64's 32 bits
 *   GFX_OP(g)       its opcode, bits 24-31
 *   GFX_W1_DATA(g)  a second word that holds data, not an address
 *   GFX_W1_ADDR(g)  a second word that holds an address: pointer-sized
 *
 * At 32 bits each is the plain member access with a cast that changes
 * nothing, so a site that uses one compiles to the code it had. Readers do
 * not mask: a writer that sign-extended into the upper half is a defect in
 * the writer (native_64bit_lp64_003.md, "Behaviour-sensitive sites").
 * Needs u32 (PR/ultratypes.h) and Gfx (PR/gbi.h) where it is used.
 */
#ifndef PORT_GFXWORDS_H
#define PORT_GFXWORDS_H

#define GFX_W0(g) ((u32) (g)->words.w0)
#define GFX_OP(g) (GFX_W0(g) >> 24)
#define GFX_W1_DATA(g) ((u32) (g)->words.w1)
#define GFX_W1_ADDR(g) ((g)->words.w1)

#endif
