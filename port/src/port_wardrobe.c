#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "port_wardrobe.h"

/* These are the watch arm's original ROM texture IDs (Csuit_lf_handZ),
 * grouped exactly as bondviewSelectCuff's six wardrobe switches. Textures
 * are loaded from the user's ROM, never copied into a generated asset. */
PortWardrobe portWardrobeStyle(s32 cuff, s32 folderBond)
{
    PortWardrobe style = {
        {{1796,32,32,6,0,2,G_TX_CLAMP,G_TX_CLAMP,0},
         {1639,8,8,4,0,2,G_TX_CLAMP,G_TX_CLAMP,0},
         {1605,32,32,6,0,2,G_TX_CLAMP,G_TX_CLAMP,0}},
        0.15f, 0.06f, FALSE
    };
    if (cuff == CUFF_FOLDER) cuff = folderBond == BOND_CONNERY ? CUFF_CONNERY : CUFF_BROSNAN;
    switch (cuff) {
    case CUFF_BOILER:
        style.material[PORT_SLEEVE].index = 1598;
        style.material[PORT_CUFF].index = 1651;
        style.material[PORT_CUFF].width = style.material[PORT_CUFF].height = 8;
        style.material[PORT_CUFF].level = 0;
        break;
    case CUFF_BROSNAN: case CUFF_DALTON: case CUFF_MOORE:
        style.material[PORT_SLEEVE].index = 1597;
        break;
    case CUFF_CONNERY:
        style.material[PORT_SLEEVE].index = 1695;
        style.material[PORT_SLEEVE].width = 32; style.material[PORT_SLEEVE].height = 48;
        break;
    case CUFF_JUNGLE:
        style.material[PORT_SLEEVE].index = 1644;
        style.material[PORT_SLEEVE].width = 32; style.material[PORT_SLEEVE].height = 48;
        style.material[PORT_CUFF] = style.material[PORT_SLEEVE];
        style.sleeveEnd = 0.70f; style.cuffEnd = 0.62f; style.rolled = TRUE;
        break;
    case CUFF_SNOW:
        style.material[PORT_SLEEVE].index = 1632;
        style.material[PORT_SLEEVE].width = 32; style.material[PORT_SLEEVE].height = 48;
        style.material[PORT_CUFF] = style.material[PORT_SLEEVE];
        style.sleeveEnd = 0.12f; style.cuffEnd = 0.03f;
        break;
    default: break; /* Same safe blue fallback as the player's default outfit. */
    }
    return style;
}

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
