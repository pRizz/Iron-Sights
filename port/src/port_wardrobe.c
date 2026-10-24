#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "port_wardrobe.h"

/* Native pistol skin must not paint over an added sleeve drawn earlier. */
s32 portWardrobeDepthRequired(s32 item)
{
    switch (item) {
    case ITEM_WPPK: case ITEM_WPPKSIL: case ITEM_TT33: case ITEM_RUGER:
    case ITEM_GOLDENGUN: case ITEM_SILVERWPPK: case ITEM_GOLDWPPK:
        return TRUE;
    default: return FALSE;
    }
}

/* Matches the six authored watch-arm switches in bondviewSelectCuff. */
s32 portWardrobeVariant(s32 cuff, s32 folderBond)
{
    if (cuff == CUFF_FOLDER) cuff = folderBond == BOND_CONNERY ? CUFF_CONNERY : CUFF_BROSNAN;
    switch (cuff) {
    case CUFF_BOILER: return 1;
    case CUFF_JUNGLE: return 2;
    case CUFF_SNOW: return 3;
    case CUFF_BROSNAN: case CUFF_DALTON: case CUFF_MOORE: return 4;
    case CUFF_CONNERY: return 5;
    default: return 0;
    }
}

s32 portWardrobeCuff(s32 variant)
{
    static const s32 cuffs[6] = {CUFF_BLUE, CUFF_BOILER, CUFF_JUNGLE, CUFF_SNOW, CUFF_BROSNAN, CUFF_CONNERY};
    return cuffs[variant >= 0 && variant < 6 ? variant : 0];
}
