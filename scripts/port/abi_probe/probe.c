/* Compile-time layout checks for the structs the renderer and ROM loader
 * depend on. Built at both pointer widths by scripts/port/test_abi_probe.sh. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ultra64.h>
#include <PR/abi.h>
#include "ob.h"
#include "platform_info.h"

/* Fixed at every width: the renderer reads these as N64 formats. */
_Static_assert(sizeof(s32) == 4 && sizeof(u32) == 4, "32-bit integer types");
_Static_assert(sizeof(Vtx) == 16, "Vertex stride");
_Static_assert(sizeof(Mtx) == 64, "Matrix layout");
_Static_assert(sizeof(Vp) == 16, "Viewport layout");
_Static_assert(sizeof(Acmd) == 8, "Audio command stride");
_Static_assert(sizeof(Light_t) == 12 && sizeof(Light) == 16, "Directional light layout");
_Static_assert(sizeof(Ambient_t) == 8 && sizeof(Ambient) == 8, "Ambient light layout");
_Static_assert(sizeof(Lights0) == 24 && sizeof(Lights1) == 24 && sizeof(Lights2) == 40
               && sizeof(Lights3) == 56 && sizeof(Lights4) == 72 && sizeof(Lights5) == 88
               && sizeof(Lights6) == 104 && sizeof(Lights7) == 120 && sizeof(Lightsn) == 120,
               "Light list layouts");
_Static_assert(sizeof(LookAt) == 32, "Look-at layout");
_Static_assert(sizeof(Hilite_t) == 16, "Highlight offsets layout");
/* Scales with the pointer. */
_Static_assert(IS_64_BIT == (sizeof(void *) == 8), "IS_64_BIT matches the host");
_Static_assert(sizeof(Gfx) == 2 * sizeof(uintptr_t), "Display-list command is two pointer words");
_Static_assert(offsetof(Gfx, words.w1) == sizeof(uintptr_t), "Display-list address word");
#if !IS_64_BIT
_Static_assert(sizeof(Gfx) == 8, "32-bit display-list stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");
/* Hilite's alignment member is `long int[4]` (include/PR/gbi.h), so it grows
 * from 16 to 32 bytes on LP64, where long is 8 bytes. */
_Static_assert(sizeof(Hilite) == 16, "32-bit highlight layout");
#endif

int main(void)
{
    printf("ABI probe OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
