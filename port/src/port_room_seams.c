/* Room Seams (FORK-086): bg.c brackets each room's opaque display list with
 * this tag so the renderer (gfx_pc.cpp, gfx_seam_grow.h) can cover the
 * level's seams. The tag names the room and the stage: the renderer learns
 * which of the room's edges no other triangle shares on its first draw, and
 * forgets every room when the stage changes. Translucent room lists, props,
 * characters, guns and hands are outside the span and render unchanged. */
#include <ultra64.h>
#include "port_room_seams.h"

extern s32 bossGetStageNum(void); /* boss.c */

Gfx *portRoomSeamsTag(Gfx *gdl, s32 on, s32 room)
{
    /* w1: 0x68 | stage (bits 11-18) | room (bits 1-10) | on (bit 0) */
    const u32 stage = (u32) bossGetStageNum() & 0xFFu;
    const u32 index = (u32) room & 0x3FFu;
    gDPNoOpTag(gdl++, 0x68000000u | (stage << 11) | (index << 1) | (on ? 1u : 0u));
    return gdl;
}
