#ifndef _BOOT_H_
#define _BOOT_H_
#include <ultra64.h>

void boot(void);
u8 *get_csegmentSegmentStart(void);
u8 *get_cdataSegmentRomStart(void);
u8 *get_cdataSegmentRomEnd(void);
u8 *get_inflateSegmentRomStart(void);
u8 *get_inflateSegmentRomEnd(void);
u32 jump_decompressfile(u8 *source, u8 *target, u32 buffer);

#endif
