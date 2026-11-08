#ifndef PORT_ICON_CACHE_H
#define PORT_ICON_CACHE_H
#ifdef __cplusplus
extern "C" {
#endif
void portIconCacheInit(const char *rom, unsigned int fingerprint);
int portIconAvailable(int icon);
/* Whether the HUD may use icons: a validated cache seen at a mission start
 * (portIconCacheLatch). A cache prepared in the background mid-mission
 * shows from the next mission; until then the strip uses names. */
int portIconCacheReady(void);
void portIconCacheLatch(void);
const unsigned char *portIconPixels(int icon);
const char *portIconCacheStatus(void);
int portIconWorkerEnabled(void);
/* The in-process icon stage (the web build; natively GE_ICON_STAGE=1): armed
 * at boot when no cache exists, launched by front.c in place of the title,
 * active while it renders in export mode; its stage then returns to the legal
 * screen as an attract demo's does (portIconStageIsCurrent, asked as the
 * stage starts), and the first front-end tick after it ends the stage. */
int portIconStagePending(void);
int portIconStageActive(void);
void portIconStageBegin(void);
int portIconStageIsCurrent(void);
void portIconStageFrontTick(void);
/* Game thread, from each export draw: starts the fade once the stage is done. */
void portIconStageTick(void);
/* From arming until the stage has returned to the legal screen. */
int portIconStageBusy(void);
void portIconWorkerCapture(int icon, int capture, int width, int height, const unsigned char *rgb);
#ifdef __cplusplus
}
#endif
#endif
