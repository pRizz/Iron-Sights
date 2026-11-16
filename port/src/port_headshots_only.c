/* Headshots Only (FORK-083): the settings and the stage latch. The rules are
 * in port_headshots_only.h; the records in port_headshots_records.c and the
 * difficulty page in port_headshots_menu.c. */
#include <string.h>
#include <str.h>
#include "port_headshots_only.h"

static s32 headshotsOnly = PORT_HEADSHOTS_ONLY_DEFAULT;
static s32 headshotsMelee = PORT_HEADSHOTS_ONLY_MELEE_DEFAULT;
static s32 stageOn = FALSE;

s32 portHeadshotsOnlyOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "headshots_only") == 0) {
        *value = headshotsOnly;
        return TRUE;
    }
    if (strcmp(key, "headshots_only_melee") == 0) {
        *value = headshotsMelee;
        return TRUE;
    }
    return FALSE;
}

s32 portHeadshotsOnlyOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "headshots_only") == 0) {
        headshotsOnly = value != 0;
        return TRUE;
    }
    if (strcmp(key, "headshots_only_melee") == 0) {
        headshotsMelee = value != 0;
        return TRUE;
    }
    return FALSE;
}

s32 portHeadshotsOnlySetting(void)
{
    return headshotsOnly;
}

s32 portHeadshotsOnlyMeleeSetting(void)
{
    return headshotsMelee;
}

void portHeadshotsOnlyBeginStage(s32 solo, s32 demo)
{
    stageOn = portHeadshotsOnlyStageActive(headshotsOnly, solo, demo);
}

s32 portHeadshotsOnlyStageOn(void)
{
    return stageOn;
}

s32 portHeadshotsOnlyBlocksHit(s32 byPlayer, s32 hitpart, s32 weaponid)
{
    /* Headshots Melee is read live: it only relaxes the rule. */
    return portHeadshotsOnlyBlocks(stageOn, headshotsMelee, byPlayer, hitpart, weaponid);
}
