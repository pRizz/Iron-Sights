#ifndef _VTXSTORE_H_
#define _VTXSTORE_H_
#include <ultra64.h>

/* In this header so the layout probe can assert its 32-bit size (0x14). */
// unsure if these structs are defined as something else, elsewhere
struct unk_09B7A0_struct_parent {
    Vertex* unk00;
    void *unk04;
    s32 unk08;
    s16 unk0C;
    s16 unk0E;
    s16 unk10;
    s16 unk12;
};

void sub_GAME_7F09B820(void);
void sub_GAME_7F09BBBC(void);
Vertex *vtxstore_allocate(s32 arg0, s32 type, void *arg2, s32 arg3);
void sub_GAME_7F09C044(Vertex* arg0);

#endif
