/* Browser input uses SDL's browser/gamepad path; native HID polling is disabled. */
#include <ultra64.h>
unsigned int g_PortRaphnetPolls;
s32 portRaphnetAvailable(void) { return 0; }
const char *portRaphnetName(void) { return ""; }
void portRaphnetProbeMode(void) {}
s32 portRaphnetPeek(s32 channel) { (void)channel; return 0; }
s32 portRaphnetRumble(s32 channel, s32 on) { (void)channel; (void)on; return 0; }
s32 portRaphnetPoll(s32 channel, u16 *buttons, s8 *x, s8 *y)
{
    (void)channel;
    if (buttons) *buttons = 0;
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}
