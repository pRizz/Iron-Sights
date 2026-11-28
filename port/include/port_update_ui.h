#ifndef PORT_UPDATE_UI_H
#define PORT_UPDATE_UI_H
/* The in-game updater's screens (FORK-095, UPD-003 of
 * scripts/port/self_update_plan.md): the Updates option, the launch's
 * indicator on the legal screen, the file-select notice and the Updates page
 * of both menus. port_update_ui.c, compiled only where the updater is
 * (GE_UPDATER); every caller guards its call with GE_UPDATER too. Game thread
 * only: the check and the install run on a worker thread of their own. */

/* port_options.c's bridge for [updates] mode= (the "mode" row). */
int portUpdateOptionGet(const char *key, int *out);
int portUpdateOptionSet(const char *key, int value);
/* video.c, after the options' defaults are captured: reads midas.ini's
 * value into the row. */
void portUpdateOptionsLoad(void);

/* boss.c's main loop, once a game frame on the game thread (videoStartFrame
 * runs on the render thread or not at all): starts the launch's run on the
 * first frame and collects a finished one, restarting the game after an
 * install in the front end. */
void portUpdateFrame(void);

/* front.c's legal screen, each frame: 1 while the launch's Automatic run
 * holds the screen. pressed is player 1's buttons this frame (Esc, a click
 * and every pad button arrive as one), which skips it unless the install
 * has begun. */
int portUpdateLegalHold(int pressed);
/* port_startup.c: the indicator's line i, NULL past the last or when the
 * screen is not held. */
const char *portUpdateIndicatorLine(int i);
/* port_startup.c: the file-select notice of a newer build, or NULL. */
const char *portUpdateNotifyLine(void);

/* The Updates page (port_download_page.h's portUpdatesPage) in the watch,
 * which runs in a mission and so never installs: its first button. */
const char *portUpdatesWatchActionLabel(void);
void portUpdatesWatchAction(void);
/* The release page of the newest build seen, or the latest release. */
void portUpdatesOpenReleasePage(void);

#endif
