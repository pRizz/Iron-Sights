/* Always-on crosshair sizing and the shared menu/watch slider presentation. */
#include <string.h>
#include "port_crosshair.h"
#include "port_hud.h"

extern s32 portOptGet(s32 option);
extern s32 portOptSetValue(s32 option, s32 value);
extern const char *portOptKey(s32 option);

static s32 sCrosshairSize = PORT_CROSSHAIR_SIZE_DEFAULT;

s32 portCrosshairSize(void)
{
    return sCrosshairSize;
}

void portCrosshairSizeSet(s32 percent)
{
    if (percent < PORT_CROSSHAIR_SIZE_MIN) percent = PORT_CROSSHAIR_SIZE_MIN;
    if (percent > PORT_CROSSHAIR_SIZE_MAX) percent = PORT_CROSSHAIR_SIZE_MAX;
    sCrosshairSize = percent;
}

f32 portCrosshairSizeScale(s32 highHud)
{
    return ((f32)sCrosshairSize / 100.0f) * (highHud ? 0.5f : 1.0f);
}

s32 portCrosshairSizeFromFraction(f32 fraction)
{
    const s32 steps = (PORT_CROSSHAIR_SIZE_MAX - PORT_CROSSHAIR_SIZE_MIN)
        / PORT_CROSSHAIR_SIZE_STEP;
    if (!(fraction >= 0.0f)) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    return PORT_CROSSHAIR_SIZE_MIN
        + (s32)(fraction * steps + 0.5f) * PORT_CROSSHAIR_SIZE_STEP;
}

s32 portCrosshairSizeOption(s32 option)
{
    const char *maybeKey = portOptKey(option);
    return maybeKey && strcmp(maybeKey, "always_crosshair_size") == 0;
}

Gfx *portCrosshairSliderDraw(Gfx *g, f32 x, f32 y, f32 width, f32 previewX,
                             s32 rgb, s32 highHud)
{
    const f32 fraction = (f32)(sCrosshairSize - PORT_CROSSHAIR_SIZE_MIN)
        / (PORT_CROSSHAIR_SIZE_MAX - PORT_CROSSHAIR_SIZE_MIN);
    const f32 scale = portCrosshairSizeScale(highHud);
    const f32 thumb = x + width * fraction;
    const u32 color = ((u32)(rgb < 0 ? 0xff3a32 : rgb) << 8) | 220;
    g = portHudPanel(g, x, y - 1, width, 2, 0x557066ff);
    g = portHudPanel(g, x, y - 1, width * fraction, 2, 0x9bc19eff);
    g = portHudPanel(g, thumb - 2, y - 4, 4, 8, 0xd1e2ccff);
    return portHudReticle(g, previewX, y, scale, scale, color, FALSE);
}

s32 portCrosshairSliderPick(s32 option, f32 mx, f32 my, f32 x, f32 y, f32 width,
                            s32 held)
{
    s32 value;
    if (!held || !portCrosshairSizeOption(option)
        || width <= 0 || mx < x - 3 || mx > x + width + 3 || my < y - 6 || my > y + 6)
        return FALSE;
    value = portCrosshairSizeFromFraction((mx - x) / width);
    if (portOptGet(option) != value) portOptSetValue(option, value);
    return TRUE;
}
