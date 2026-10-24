#ifndef MIDAS_WEB_SAMPLER_H
#define MIDAS_WEB_SAMPLER_H
#include <stdint.h>
void portWebSamplerFind(uint32_t program, int locations[2]);
void portWebSamplerSync(const int locations[2]);
void portWebSamplerFlags(int tile, uint32_t cms, uint32_t cmt, bool linear);
uint32_t portWebMirrorClamp(void);
void portWebTextureUploaded(uint32_t id);
void portWebTextureDeleted(uint32_t id);
bool portWebTextureHasPixels(uint32_t id);
#endif
