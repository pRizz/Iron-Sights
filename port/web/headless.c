/* Settings/input defaults for the ROM smoke target only; video owns these in play. */
#include <ultra64.h>
#ifdef GE_VIDEO
#error Headless adapters must not be linked into the playable browser target
#endif
static s32 hudWide = -1;
static s32 crosshair[4] = {-2, -2, -2, -2};
extern s32 getPlayerCount(void);
void portVideoKbdDigitalMove(s32 *dx, s32 *dy) { *dx = 0; *dy = 0; }
s32 portHudWideCfg(void) { return hudWide; }
void portHudWideAdopt(s32 wide) { hudWide = wide != 0; }
s32 portInputLayoutInUse(s32 device) { (void)device; return -1; }
const char *portInputLayoutShown(s32 device) { (void)device; return "None"; }
s32 portInputDeviceModeRaw(s32 device) { (void)device; return -1; }
void portVideoBindsResetDefaults(void) {}
s32 portInputPadResetDefaults(s32 device) { (void)device; return 0; }
s32 portPlayerCrosshairOwn(s32 player)
{
    return player >= 0 && player < 4 ? crosshair[player] : -2;
}
void portPlayerCrosshairSet(s32 player, s32 color)
{
    if (player >= 0 && player < 4) crosshair[player] = color < -1 ? -2 : color;
}
s32 portPlayerCrosshairColor(s32 player)
{
    s32 own = portPlayerCrosshairOwn(player);
    return getPlayerCount() >= 2 && own >= -1 ? own : -1;
}
