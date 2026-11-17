#include <ultra64.h>
#include <memp.h>
#include "model.h"
#include "initunk_005520.h"
#include "objecthandler.h"
#include "memp.h"

#ifndef TARGET_N64
/* Reinforcements (FORK-081): room for the stage's added guards and their guns.
 * model.c's level-reset search skips the same spare slots. */
#include "port_reinforcements.h"
#define MODEL_SPARE_SLOTS          (30 + portReinforcementsExtraModels(portReinforcementsStageExtraChrs()))
#define ANIM_MODEL_SPARE_SLOTS     (10 + portReinforcementsExtraAnimModels(portReinforcementsStageExtraChrs()))
#else
#define MODEL_SPARE_SLOTS          30
#define ANIM_MODEL_SPARE_SLOTS     10
#endif

#define MODEL_SPARE_RWDATALEN      0x14
#define ANIM_MODEL_SPARE_RWDATALEN 0x8c


void modelmgrResetSlotCounts(void)
{
    g_MaxAnimModelSlots = 0;
    g_MaxModelSlots = 0;
}


void modelmgrSetLevelResetting(bool resetting) 
{
    g_ModelIsLvResetting = resetting;
}

//this may be a file split

/**
 * NTSC address 0x7F005540.
*/
void modelmgrAllocateModelSlots(s32 numobjs)
{
    s32 i;

    g_MaxModelSlots = numobjs + MODEL_SPARE_SLOTS;
    
    g_ModelSlots = mempAllocBytesInBank(g_MaxModelSlots * sizeof(struct ModelSlot), MEMPOOL_STAGE);

    for (i = 0; i < g_MaxModelSlots; i++)
    {
        MODEL_SLOT_OBJ(g_ModelSlots[i]) = 0;

        if (i < numobjs)
        {
            MODEL_SLOT_RWDATA(g_ModelSlots[i]) = NULL;
        }
        else
        {
            MODEL_SLOT_RWDATA(g_ModelSlots[i]) = mempAllocBytesInBank(MODEL_SPARE_RWDATALEN * sizeof(union ModelRwData *), MEMPOOL_STAGE);
            g_ModelSlots[i].unk02 = MODEL_SPARE_RWDATALEN;
        }
    }
}


/**
 * NTSC address 0x7F005540.
*/
void modelmgrAllocateAnimModelSlots(s32 numanimated)
{
    s32 temp_t6;
    s32 i;

    g_MaxAnimModelSlots = numanimated + ANIM_MODEL_SPARE_SLOTS;

    // mips2c says: g_AnimModelSlots = mempAllocBytesInBank(temp_t6 * 0xC0, 4);
    // however, the pointer is incremented by 0xbc in the loop below.
#if IS_64_BIT
    /* LP64-004: the N64's 4 spare bytes a slot leave the bank at 4 mod 8
     * after an odd count of 264-byte slots; keep them, rounded to 16 as the
     * 32-bit 0xC0 stride keeps the bank */
    g_AnimModelSlots = mempAllocBytesInBank((g_MaxAnimModelSlots * (4 + sizeof(struct AnimModelSlot)) + 0xF) & ~0xFu, MEMPOOL_STAGE);
#else
    g_AnimModelSlots = mempAllocBytesInBank(g_MaxAnimModelSlots * (4 + sizeof(struct AnimModelSlot)), MEMPOOL_STAGE);
#endif

    for (i = 0; i < g_MaxAnimModelSlots; i++)
    {
        MODEL_SLOT_OBJ(g_AnimModelSlots[i]) = 0;

        if (i < numanimated)
        {
            MODEL_SLOT_RWDATA(g_AnimModelSlots[i]) = NULL;
        }
        else
        {
            MODEL_SLOT_RWDATA(g_AnimModelSlots[i]) = mempAllocBytesInBank(ANIM_MODEL_SPARE_RWDATALEN * sizeof(union ModelRwData *), MEMPOOL_STAGE);
            g_AnimModelSlots[i].unk02 = ANIM_MODEL_SPARE_RWDATALEN;
        }
    }
}
