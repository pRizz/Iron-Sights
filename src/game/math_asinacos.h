#ifndef _MATH_ASINACOS_H_
#define _MATH_ASINACOS_H_

#include <ultra64.h>

#ifdef __EMSCRIPTEN__
/* N64 fixed-point angles are distinct from the hosted double libm functions. */
#define acos ge_n64_acos
#define asin ge_n64_asin
#endif
u16 acos(s16 arg0);
s16 asin(s16 arg0);

#endif
