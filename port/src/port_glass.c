/* FORK-066: the display-list tags that bracket glass draws for Glass
 * Reflections. propobj.c brackets glass props and windowed doors, glass.c the
 * flying shards. gfx_pc.cpp reads G_NOOP 0x63 with the kind in the low byte
 * (0 = closed) and hands those triangles to the glass pass
 * (port/fast3d/gfx_glass.inl), which adds the level reflection cube over them.
 * The tag changes nothing else about the draw. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bgfog.h"
#include "port_glass.h"

s32 portGlassKindOfObject(const void *object)
{
    const ObjectRecord *obj = (const ObjectRecord *)object;

    if (obj == NULL) return PORT_GLASS_NONE;
    if (obj->type == PROPDEF_GLASS || obj->type == PROPDEF_TINTED_GLASS) return PORT_GLASS_PANE;
    if (obj->type == PROPDEF_DOOR && (((const struct DoorRecord *)obj)->doorFlags & DOORFLAG_WINDOWED))
    {
        return PORT_GLASS_DOOR;
    }
    return PORT_GLASS_NONE;
}

Gfx *portGlassReflectTag(Gfx *gdl, s32 kind)
{
    if (kind != PORT_GLASS_NONE)
    {
        /* the level's sky colour, for the reflection cube's empty space
         * (G_NOOP 0x60, as port_shiny.c sends it for the shiny guns) */
        const CurrentEnvironmentRecord *env = fogGetCurrentEnvironmentp();
        gDPNoOpTag(gdl++, 0x60000000u | ((u32)env->Red << 16) | ((u32)env->Green << 8) | (u32)env->Blue);
    }
    gDPNoOpTag(gdl++, 0x63000000u | ((u32)kind & 0xFFu));
    return gdl;
}
