#ifndef PORT_HD_TEXTURES_H
#define PORT_HD_TEXTURES_H
#if defined(GE_VIDEO) && !defined(__EMSCRIPTEN__) && !defined(GE_XBOX)
#ifdef __cplusplus
extern "C" {
#endif
void portTexturePackRoot(const char *directory);
/* Set before rendering starts; absent user packs may use this read-only root. */
void portTexturePackBundleRoot(const char *directory);
void portTexturePackSet(int mode);
int portTexturePackGet(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
