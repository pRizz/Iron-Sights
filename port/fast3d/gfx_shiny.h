/* FORK-058: Shiny Gold Guns. gunfire.c brackets the first-person draw of the
 * Cougar Magnum, Golden Gun, Silver PP7 and Gold PP7 with a G_NOOP tag 0x5E
 * (port_shiny.c); inside it, gfx_pc.cpp hands the reflection-mapped
 * (G_TEXTURE_GEN) triangles' lighting, reflection coordinates and texture to
 * this module, and the backend's bloom to gfx_bloom.inl. Everything else in
 * the span, including the native hands, keeps the stock path. */
#ifndef GFX_SHINY_H
#define GFX_SHINY_H

#include <stdint.h>

namespace gfx_shiny {

/* The look. Defaults are the shipped one; shiny_weapons.ini beside midas.ini,
 * when a player creates it, overrides any of them (tools/midas_ini_help.txt). */
struct Tuning {
    float ambient;   /* lighting floor 0..255 replacing the game's 150; < 0 keeps it */
    float diffuse;   /* directional light multiplier (1 = the game's) */
    float room;      /* share of the game's floor-tile darkening kept (1 = all, 0 = none) */
    float contrast;  /* reflection texture S-curve strength, 0 = none */
    float gain;      /* extra brightness for the texture's highlights, 0 = none */
    float levels;    /* 0..1 towards the texture's own range stretched to black..white */
    float coverage;  /* reflection coordinate scale (1 = the game's) */
    float sweep;     /* reflection shift per half turn of the camera (0 = none) */
    float drift;     /* constant reflection shift, cycles per second (0 = none) */
    bool sharp;      /* sample the reflection without its mip chain: crisper highlights */
    bool log;        /* log=1 or GE_SHINY_LOG: draw state to stderr */
    int bloom;             /* 0 off, 1 subtle, 2 ludicrous: glow from the brightest reflective pixels */
    float bloom_threshold; /* luma 0..1 where the glow starts; < 0 = the mode's */
    float bloom_strength;  /* multiplier on the mode's glow strength */
    bool gold;             /* Golden Gun and Gold PP7 take the look (default on) */
    bool silver;           /* Cougar Magnum and Silver PP7 take it (default off: the owner
                            * preferred them stock in play, 2026-11-07) */
};

/* The glow gfx_pc.cpp asks the backend for at the end of a shiny weapon draw. */
struct Bloom {
    float threshold, strength, radius;
    int passes;
};

/* Re-reads shiny_weapons.ini when its contents change (twice a second). */
void start_frame();
const Tuning &tuning();
/* Whether a shiny weapon item (bondconstants.h ITEM_*) takes the look now:
 * the Shiny Gold Guns setting is on and the file's gold/silver choice has it. */
bool applies(uint8_t item);
/* Texture cache variant for a reflection texture in the span: changes when a
 * texture-affecting setting does, so old images are not reused. Never 0. */
uint8_t texture_variant();
/* The reflection coordinate after coverage, sweep and drift; lookat_x/y are
 * the game's camera reflection axes (gSPLookAt) when it set them. */
float remap(float dot, int axis, const int8_t *lookat_x, const int8_t *lookat_y, bool lookat);
/* Tone curve, in place, over an RGBA8 image. */
void process(uint8_t *rgba, uint32_t width, uint32_t height);
/* false when bloom is off */
bool bloom(Bloom *out);

} // namespace gfx_shiny

#endif
