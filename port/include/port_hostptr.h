/**
 * PORT_IS_HOST_ADDR(v): is this 32-bit word a real host pointer, as opposed to
 * an N64 segment word (0x0Sxxxxxx) or a file offset?
 *
 * On PC, host pointers always live at or above 0x10000000: the Windows image is
 * rebased there, the Linux image sits at 0x08048000 (segments 8/9 are never
 * installed), and both arenas are pinned at 0x20000000+.
 *
 * The original Xbox can't follow that rule: the XBE base is forced to
 * 0x00010000, so the whole image (assets and static buffers) overlaps the N64
 * segment 0/1 word range. There the test is "above 0x10000000 OR inside the
 * image" (xbox_sys.c reads the image size from the XBE header at boot). Heap
 * allocations never hold display-list data, and every other display-list
 * allocation goes through the arenas. See docs/port/xbox-port.md.
 *
 * A 64-bit build tests "any of the upper 32 bits set": segment words and file
 * offsets always fit in 32 bits, so the two cannot collide provided no game
 * data lives below 4 GB. Nothing pins memory low at 64 bits, and the startup
 * check (port_lp64.c, portLp64CheckAddressSpace) stops the game if the
 * executable image, an arena or the ROM image sits below 4 GB
 * (scripts/port/native_64bit_plan.md, "Addresses"). The argument must be
 * pointer-sized or unsigned: at 64 bits it is converted to uintptr_t, so a
 * signed 32-bit value with bit 31 set would sign-extend and read as a host
 * pointer.
 */
#ifndef PORT_HOSTPTR_H
#define PORT_HOSTPTR_H

#ifdef GE_XBOX
#ifdef __cplusplus
extern "C" {
#endif
int portXboxIsHostAddr(unsigned int v);
#ifdef __cplusplus
}
#endif
#define PORT_IS_HOST_ADDR(v) (portXboxIsHostAddr((unsigned int) (v)))
#else
#include "platform_info.h"
#if IS_64_BIT
#define PORT_IS_HOST_ADDR(v) ((((uintptr_t) (v)) >> 32) != 0)
#else
#define PORT_IS_HOST_ADDR(v) ((unsigned int) (v) >= 0x10000000u)
#endif
#endif

/*
 * Command-word and slot predicates (LP64-003 Task 6). Each 32-bit form is the
 * expression its sites had, so the 32-bit program is unchanged; each 64-bit
 * form reads the whole word instead of a 32-bit copy of it.
 *
 * PORT_WORD_IS_HOST(w): is a display-list word a host pointer rather than a
 * segment word or an IMAGESEG tag? At 32 bits the literal threshold stays,
 * under GE_XBOX too (its PORT_IS_HOST_ADDR differs), and needs u32
 * (PR/ultratypes.h) where it is used.
 *
 * PORT_WORD_HAS_SEGMENT(w, seg): is w a segment word of segment seg? At 64
 * bits a host pointer whose bits 24-31 happen to equal seg is not one.
 *
 * PORT_WORD_IS_INDEX(w, limit): does a slot that holds a number or a pointer
 * hold a number below limit? Compared at full width, so a pointer with a small
 * low word is not taken for a number. limit must be non-negative: it is
 * converted to uintptr_t.
 */
#include "platform_info.h"
#if IS_64_BIT
#define PORT_WORD_IS_HOST(w) PORT_IS_HOST_ADDR(w)
#define PORT_WORD_HAS_SEGMENT(w, seg) (((uintptr_t) (w) >> 24) == (uintptr_t) (seg))
#else
#define PORT_WORD_IS_HOST(w) ((u32) (w) >= 0x10000000u)
#define PORT_WORD_HAS_SEGMENT(w, seg) (((uintptr_t) (w) & 0xFF000000) == ((uintptr_t) (seg) << 24))
#endif
#define PORT_WORD_IS_INDEX(w, limit) ((uintptr_t) (w) < (uintptr_t) (limit))

#endif
