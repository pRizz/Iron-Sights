/* FORK-066: Glass Reflections (port_glass.c, port/fast3d/gfx_glass.inl). */
#ifndef PORT_GLASS_H
#define PORT_GLASS_H

#include <ultra64.h>

/* What a glass span holds; the renderer picks its strength by kind. */
enum PortGlassKind {
    PORT_GLASS_NONE = 0,
    PORT_GLASS_PANE = 1,   /* a glass or tinted glass prop: its blended triangles */
    PORT_GLASS_DOOR = 2,   /* a windowed door: only its blended (window) triangles */
    PORT_GLASS_SHARDS = 3  /* the flying shards of a broken pane (glass.c) */
};

/* The glass kind of a prop's object (an ObjectRecord *), PORT_GLASS_NONE for
 * anything else. */
s32 portGlassKindOfObject(const void *obj);
/* Opens (kind != PORT_GLASS_NONE) or closes a glass draw span (G_NOOP 0x63). */
Gfx *portGlassReflectTag(Gfx *gdl, s32 kind);

#endif
