#ifndef PORT_ICON_CACHE_H
#define PORT_ICON_CACHE_H
#ifdef __cplusplus
extern "C" {
#endif
void portIconCacheInit(const char *rom, unsigned int fingerprint);
int portIconAvailable(int icon);
int portIconCacheReady(void);
const unsigned char *portIconPixels(int icon);
const char *portIconCacheStatus(void);
int portIconWorkerEnabled(void);
void portIconWorkerCapture(int icon, int capture, int width, int height, const unsigned char *rgb);
#ifdef __cplusplus
}
#endif
#endif
