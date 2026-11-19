/**
 * Where load_briefing_text_for_stage (src/game/front.c) puts the briefing
 * and its language bank in the front end's block (LP64-005 Task 8).
 *
 * front.c loads the briefing's PORT_BRIEFING_FILE_BYTES at
 * PORT_BRIEFING_FILE_AT bytes into the block, steps past it by
 * PORT_BRIEFING_FILE_STEP of the file's size, and loads the language bank,
 * PORT_BRIEFING_BANK_SCALE times the file's size, from there. The step is
 * the part that depends on the width: N64 code stepped a Gfx pointer by one
 * command per 8 bytes, which at 64 bits, with 16-byte commands, skipped
 * twice the file and ended the bank 0x200 bytes past the block (fixed in
 * a657b4b). The layout probe (scripts/port/abi_probe/probe.c) includes this
 * header and asserts that the bank starts and ends inside the block at both
 * widths, so it checks the step front.c compiles, not a copy of it.
 *
 * The offsets and sizes are front.c's literals and the block size is
 * initmenus.c's allocation; the N64 build keeps its own literals and step
 * (front.c, under TARGET_N64), which this header does not reach.
 */
#ifndef PORT_BRIEFING_SPAN_H
#define PORT_BRIEFING_SPAN_H

#include <stddef.h>
#include "platform_info.h"

/* ptr_logo_and_walletbond_DL's allocation (src/game/initmenus.c) */
#define PORT_BRIEFING_BLOCK_BYTES 0x78000
#define PORT_BRIEFING_FILE_AT (4096 * 10)
#define PORT_BRIEFING_FILE_BYTES 0x200
#define PORT_BRIEFING_BANK_SCALE 879

/* Bytes from the briefing file's start to its language bank's, for a file
 * of BYTES bytes. */
#if IS_64_BIT
#define PORT_BRIEFING_FILE_STEP(bytes) ((size_t) (bytes))
#else
#define PORT_BRIEFING_FILE_STEP(bytes) ((size_t) ((bytes) / 8) * sizeof(Gfx))
#endif

#endif
