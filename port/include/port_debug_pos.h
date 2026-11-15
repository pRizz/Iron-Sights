/* Debug Position (FORK-084): show the leftover "testing man pos" readout
 * (room id, x/y/z, heading H and pitch P across the top of the view) from a
 * setting that changes while the game runs.
 *
 * `[display] show_debug_pos` (default 0) is the setting. The GE_DEBUGPOS
 * environment variable still overrides it either way: a non-zero value shows
 * the readout, 0 hides it, whatever midas.ini says. The Debug Pos cheat keeps
 * its own flag (g_DebugManPos); the readout shows when either is on. */
#ifndef PORT_DEBUG_POS_H
#define PORT_DEBUG_POS_H

#include <ultra64.h>

#define PORT_DEBUG_POS_DEFAULT 0

s32 portDebugPosOptionGet(const char *key, s32 *value);
s32 portDebugPosOptionSet(const char *key, s32 value);
/* debugmenu_handler.c get_debug_testingmanpos_flag: the setting, after the
 * GE_DEBUGPOS override. */
s32 portDebugPosShown(void);

#endif
