/**
 * Display-list command words at their own width (LP64-003 Task 7).
 *
 * A Gfx is two pointer-sized words, so 8 bytes each in a 64-bit build. The
 * first is always an N64 command word, and writers must keep its upper half
 * zero: gbi.h's macros and every hand-written writer build it from a value
 * whose upper half is zero (an unsigned 32-bit value, or in two writers a
 * signed one that is never negative). The second holds either data (vertex
 * indices, colours, sizes) or an address. These accessors say which a site
 * reads, so a 32-bit view of a word is written once, explicitly, instead of
 * as an implicit narrowing.
 *
 *   GFX_W0(g)       the command word, as the N64's 32 bits
 *   GFX_OP(g)       its opcode, bits 24-31 of those 32 bits
 *   GFX_W1_DATA(g)  a second word that holds data, not an address; also
 *                   G_VTX's segment-relative word in port_baked_light.c,
 *                   of which only the low 24 bits are read
 *   GFX_W1_ADDR(g)  a second word that holds an address: pointer-sized
 *
 * At 32 bits each is the plain member access with a cast that changes
 * nothing, so a site that uses one compiles to the code it had. At 64 bits
 * GFX_W0 and GFX_OP give the N64's 32-bit command word and discard the upper
 * half, so a game-side reader using them cannot see a writer that put bits
 * there. The renderer's display-list walker (gfx_run_dl, port/fast3d/gfx_pc.cpp)
 * does not use them for the opcode at 64 bits: it shifts the whole word, so a
 * non-zero upper half gives an opcode above 0xFF that no case matches, and
 * the walker's default stops the game naming the word (native_64bit_lp64_003.md,
 * "Behaviour-sensitive sites").
 * Needs u32 (PR/ultratypes.h) and Gfx (PR/gbi.h) where it is used.
 */
#ifndef PORT_GFXWORDS_H
#define PORT_GFXWORDS_H

#define GFX_W0(g) ((u32) (g)->words.w0)
#define GFX_OP(g) (GFX_W0(g) >> 24)
#define GFX_W1_DATA(g) ((u32) (g)->words.w1)
#define GFX_W1_ADDR(g) ((g)->words.w1)

#endif
