/* Wasm calls have exact signatures, including the extra varargs pointer. */
#ifndef MIDAS_WEB_COMPAT_H
#define MIDAS_WEB_COMPAT_H
#include <stdio.h>
#include <ultra64.h>
void romCopy(void *target, void *source, u32 size);
s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool);
char *strcpy(char *destination, const char *source);
double atof(const char *text);
float fabsf(float value);
float tanf(float value);
#ifndef GE_WEB_PROTOTYPE_SCAN
#include "../generated/web_prototypes.h"
#endif
#endif
