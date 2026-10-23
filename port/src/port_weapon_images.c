/* Generated images are embedded in the executable; no runtime asset paths. */
#include "asmsym.h"
#include "port_weapon_images.h"
__asm__(
    ".section .rodata\n"
    ".balign 16\n"
    ".globl " ASM_SYM("port_weapon_images") "\n"
    ASM_SYM("port_weapon_images") ":\n"
#if PORT_WEAPON_IMAGES_AVAILABLE
    ".incbin \"assets/port/weapon_icons/weapon_icons.rle\"\n"
#else
    ".byte 0\n"
#endif
    ".text\n"
);
