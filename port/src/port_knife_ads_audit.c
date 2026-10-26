/* Deterministic private captures exercise native throws, never synthesized ammo events. */
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/bondinv.h"
#include "game/gun.h"
#include "game/lv.h"
#include "game/player.h"
#include "port_ads.h"
#include "port_knife_ads.h"

static f32 knifeAuditTime;
static s32 knifeAuditThrows;
static s32 knifeAuditEnabled(void)
{
#ifdef GE_VIDEO
    const char *value = getenv("GE_ADS_AUDIT");
    return value && atoi(value) == ITEM_THROWKNIFE;
#else
    return FALSE;
#endif
}

s32 portKnifeAdsAuditTrigger(s32 normal)
{
    if (!knifeAuditEnabled() || !knifeAuditThrows || get_cur_playernum() != 0) return normal;
    return (knifeAuditTime >= 150 && knifeAuditTime < 156)
        || (knifeAuditTime >= 250 && knifeAuditTime < 256);
}

s32 portKnifeAdsAudit(s32 available, s32 *aim)
{
#ifdef GE_VIDEO
    static s32 equipped, captured;
    const char *throws = getenv("GE_KNIFE_ADS_THROWS");
    const char *baseline = getenv("GE_KNIFE_ADS_BASELINE");
    extern void portConLogf(const char *fmt, ...);
    extern int ge_shot_force;
    if (!knifeAuditEnabled() || !available || get_cur_playernum() != 0) return FALSE;
    knifeAuditThrows = throws && *throws;
    if (!equipped) {
        bondinvSetAllGunsFlag(TRUE);
        currentPlayerEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
        currentPlayerEquipWeaponWrapper(GUNRIGHT, ITEM_THROWKNIFE);
        equipped = TRUE;
    }
    knifeAuditTime += g_GlobalTimerDelta;
    *aim = !(baseline && *baseline) && knifeAuditTime >= 75
        && (knifeAuditThrows ? knifeAuditTime < 252 : knifeAuditTime < 130);
    if (knifeAuditTime < 140 && getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_THROWKNIFE)
        g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine = 1;
    if (knifeAuditTime >= 120 && !captured && getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_THROWKNIFE
        && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state == GUN_ANIM_STATE_IDLE
        && g_CurrentPlayer->hands[GUNRIGHT].weapon_current_animation == 0) {
        portConLogf("ge: ads-audit: capture item=%d\n", ITEM_THROWKNIFE);
        ge_shot_force = 1;
        captured = TRUE;
    }
    if (knifeAuditThrows && knifeAuditTime >= 140 && knifeAuditTime < 320) {
        const struct hand *hand = &g_CurrentPlayer->hands[GUNRIGHT];
        portConLogf("ge: knife-audit: time=%.2f aim=%d state=%d anim=%d ammo=%d visible=%d blend=%.4f\n",
            knifeAuditTime, *aim, hand->weapon_action_state, hand->field_890,
            hand->weapon_ammo_in_magazine, hand->field_87E, portAdsBlend());
        ge_shot_force = 1;
    }
    if (captured && knifeAuditTime >= (knifeAuditThrows ? 340 : 150)) {
        portConLogf("ge: ads-audit: complete (1 guns)\n");
        exit(0);
    }
    return TRUE;
#else
    return FALSE;
#endif
}
