#ifndef GE_VIDEO
#include "port_icon_cache.h"
void portIconCacheInit(const char *rom, unsigned int fingerprint) {}
int portIconAvailable(int icon) { return 0; }
int portIconCacheReady(void) { return 0; }
void portIconCacheLatch(void) {}
const unsigned char *portIconPixels(int icon) { return 0; }
const char *portIconCacheStatus(void) { return "Unavailable in a headless build"; }
int portIconWorkerEnabled(void) { return 0; }
int portIconStagePending(void) { return 0; }
int portIconStageActive(void) { return 0; }
void portIconStageBegin(void) {}
int portIconStageIsCurrent(void) { return 0; }
void portIconStageFrontTick(void) {}
void portIconStageTick(void) {}
int portIconStageBusy(void) { return 0; }
void portIconWorkerCapture(int icon, int capture, int width, int height, const unsigned char *rgb) {}
#endif
