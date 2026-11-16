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
/* Scales with the pointer. */
_Static_assert(IS_64_BIT == (sizeof(void *) == 8), "IS_64_BIT matches the host");
_Static_assert(sizeof(Gfx) == 2 * sizeof(uintptr_t), "Display-list command is two pointer words");
_Static_assert(offsetof(Gfx, words.w1) == sizeof(uintptr_t), "Display-list address word");
#if !IS_64_BIT
_Static_assert(sizeof(Gfx) == 8, "32-bit display-list stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");
#endif

int main(void)
{
    printf("ABI probe OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
