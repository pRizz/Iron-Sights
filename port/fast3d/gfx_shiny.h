/* FORK-058: Richer Reflections and Gun Glint. gunfire.c brackets the
 * first-person draw of the Cougar Magnum, Golden Gun, Silver PP7, Gold PP7 and
 * the ordinary guns (PP7 to sniper rifle) with a G_NOOP tag 0x5E (port_shiny.c).
 * For the shiny guns, gfx_pc.cpp hands the reflection-mapped (G_TEXTURE_GEN)
 * triangles' lighting, reflection coordinates and texture to this module, the
 * scene reflection to gfx_screen_env.inl and bloom to gfx_bloom.inl; for the
 * ordinary guns, their metal triangles to the glint pass (gfx_glint.inl).
 * Everything else in the span, including the native hands, keeps the stock path. */
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
    /* the scene reflection (shiny reflections candidate 3a, gfx_screen_env.inl) */
    float screen;       /* 0..1 mix of the scene (a sphere map from the screen) into the reflection */
    float screen_gain;  /* brightness of that scene */
    float screen_back;  /* shade of directions behind the camera (mirrored from the front) */
    float screen_fov;   /* vertical field of view assumed for the screen, degrees */
    float screen_blur;  /* extra blur factor (1 = the sphere map's own size) */
    bool screen_flip;   /* sample the screen copy upside down */
    float screen_smooth; /* 0..1 share of each new frame blended into the reflection (1 = none: it flickered) */
    bool silver_screen; /* the silver guns take the screen reflection without the tuned look (default on) */
    int map;            /* prototype: cube face size reflecting the level itself (room cache), 0 = the screen */
    /* Gun Glint ([display] gun_glint): a moving highlight on the ordinary guns' metal (gfx_glint.inl) */
    float glint;        /* strength 0..1 of the per-pixel highlight added over metal triangles (0 = off) */
    float glint_power;  /* its tightness: higher = a smaller, sharper spot */
    float glint_sat;    /* the most colourful (0..255 mean saturation) an image may be and count as metal */
    uint32_t glint_skip; /* items (bit = ITEM_* id) left without the glint; glint_skip = 8, 15 in the file */
    float screen_hue;   /* 0..1 share of the scene's own colour in the scene reflection: 0 (default) =
                         * light and dark only, so gold reflects in gold and silver in silver */
    float screen_mirror; /* 0..1 share of the scene itself, in the metal's average colour, rather than
                          * as light and dark on the authored pattern (which near-white silver clips) */
};

/* whether the glint applies to this item (ITEM_WPPK..ITEM_SNIPERRIFLE, less glint_skip) */
bool glint_on(uint8_t item);

/* The screen reflection gfx_pc.cpp asks the backend for at a span's start. */
struct Screen {
    float mix, gain, back, fov, blur, smooth;
    int flip;
    int map_size; /* prototype: > 0 renders the level as a cube of this face size (gfx_envcube.inl) */
    float hue;    /* 0..1 share of the scene's own colour in the reflection (0 = the metal's colour) */
    float mirror; /* 0..1 share of the scene shown as a mirror in the metal's colour */
};
/* false when screen reflections are off */
bool screen(Screen *out);
/* whether this shiny weapon item takes the screen reflection now */
bool screen_applies(uint8_t item);

/* The glow gfx_pc.cpp asks the backend for at the end of a shiny weapon draw. */
struct Bloom {
    float threshold, strength, radius;
    int passes;
};

/* Re-reads shiny_weapons.ini when its contents change (twice a second). */
void start_frame();
const Tuning &tuning();
/* Whether a shiny weapon item (bondconstants.h ITEM_*) takes the look now:
 * the Richer Reflections setting is on and the file's gold/silver choice has it. */
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
