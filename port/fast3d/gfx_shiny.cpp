/* FORK-058: Richer Reflections - the look's settings and image processing. See
 * gfx_shiny.h. [display] shiny_weapons (video.c -> gfx_shiny_weapons) switches
 * it; the optional shiny_weapons.ini beside midas.ini tunes it. */
#include "gfx_shiny.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" const char *portRootPath(const char *name);
extern uint32_t gfx_shiny_weapons; /* gfx_pc.cpp, from [display] shiny_weapons */
extern uint32_t gfx_gun_glint;     /* gfx_pc.cpp, from [display] gun_glint */
extern uint32_t gfx_glass_reflections; /* gfx_pc.cpp, from [display] glass_reflections (FORK-066) */

namespace gfx_shiny {
namespace {

const char kFile[] = "shiny_weapons.ini";

/* The shipped look (the owner's pick in play, 2026-11-07): a gentle tone curve
 * on the gold guns only; every other knob neutral. */
const Tuning kDefaults = { -1.0f, 1.0f, 1.0f, 0.5f, 0.4f, 0.0f, 1.0f, 0.0f, 0.0f,
                           false, false, 0, -1.0f, 1.0f, true, false,
                           0.5f, 1.0f, 0.85f, 60.0f, 8.0f, false, 0.2f, true, 128,
                           0.25f, 64.0f, 12.0f, 0u, 0.0f, 1.0f, 0.5f,
                           1.0f, 0.25f, 1.25f, 0.6f, 0.6f, 96.0f, 1.0f, false };
Tuning s_tuning = kDefaults;
std::string s_text;
unsigned s_frame;
uint8_t s_variant = 1;

void parse(const std::string &text, Tuning &t)
{
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t\r") + 1);
        const float v = (float)atof(line.c_str() + eq + 1);
        if (key == "ambient") t.ambient = v > 255.0f ? 255.0f : v;
        else if (key == "diffuse") t.diffuse = v < 0.0f ? 0.0f : v;
        else if (key == "room") t.room = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "contrast") t.contrast = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "gain") t.gain = v < 0.0f ? 0.0f : v;
        else if (key == "levels") t.levels = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "coverage") t.coverage = v < 0.1f ? 0.1f : v;
        else if (key == "sweep") t.sweep = v;
        else if (key == "drift") t.drift = v;
        else if (key == "sharp") t.sharp = v != 0.0f;
        else if (key == "log") t.log = v != 0.0f;
        else if (key == "bloom") t.bloom = v < 0.0f ? 0 : (v > 2.0f ? 2 : (int)v);
        else if (key == "bloom_threshold") t.bloom_threshold = v > 1.0f ? 1.0f : v;
        else if (key == "bloom_strength") t.bloom_strength = v < 0.0f ? 0.0f : v;
        else if (key == "gold") t.gold = v != 0.0f;
        else if (key == "silver") t.silver = v != 0.0f;
        else if (key == "screen") t.screen = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "screen_gain") t.screen_gain = v < 0.0f ? 0.0f : v;
        else if (key == "screen_back") t.screen_back = v < 0.0f ? 0.0f : v;
        else if (key == "screen_fov") t.screen_fov = v < 10.0f ? 10.0f : (v > 170.0f ? 170.0f : v);
        else if (key == "screen_blur") t.screen_blur = v < 0.1f ? 0.1f : v;
        else if (key == "screen_flip") t.screen_flip = v != 0.0f;
        else if (key == "screen_smooth") t.screen_smooth = v < 0.01f ? 0.01f : (v > 1.0f ? 1.0f : v);
        else if (key == "silver_screen") t.silver_screen = v != 0.0f;
        else if (key == "screen_hue") t.screen_hue = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "glint_ads") t.glint_ads = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "screen_mirror") t.screen_mirror = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "map") t.map = v <= 0.0f ? 0 : (v < 16.0f ? 16 : (v > 512.0f ? 512 : (int)v));
        else if (key == "glint") t.glint = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "glint_power") t.glint_power = v < 1.0f ? 1.0f : (v > 256.0f ? 256.0f : v);
        else if (key == "glint_sat") t.glint_sat = v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v);
        else if (key == "glass") t.glass = v < 0.0f ? 0.0f : (v > 4.0f ? 4.0f : v);
        else if (key == "glass_f0") t.glass_f0 = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "glass_shard") t.glass_shard = v < 0.0f ? 0.0f : (v > 4.0f ? 4.0f : v);
        else if (key == "glass_shard_f0") t.glass_shard_f0 = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "glass_glint") t.glass_glint = v < 0.0f ? 0.0f : (v > 4.0f ? 4.0f : v);
        else if (key == "glass_power") t.glass_power = v < 1.0f ? 1.0f : (v > 512.0f ? 512.0f : v);
        else if (key == "glass_blur") t.glass_blur = v < 0.0f ? 0.0f : (v > 8.0f ? 8.0f : v);
        else if (key == "glass_split") t.glass_split = v != 0.0f;
        else if (key == "glint_skip") { /* item ids, separated by commas or spaces */
            t.glint_skip = 0;
            for (const char *p = line.c_str() + eq + 1; *p;) {
                char *next;
                const long id = strtol(p, &next, 10);
                if (next == p) {
                    p++;
                    continue;
                }
                if (id >= 0 && id < 32) t.glint_skip |= 1u << id;
                p = next;
            }
        }
    }
    if (getenv("GE_SHINY_LOG")) t.log = true;
}

void reload()
{
    std::string text; /* no file: the shipped defaults */
    if (FILE *f = fopen(portRootPath(kFile), "rb")) {
        char buf[1024];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && text.size() < 16384) text.append(buf, n);
        fclose(f);
    }
    if (text == s_text && s_frame > 1) return;
    s_text = text;
    Tuning t = kDefaults;
    parse(text, t);
    if (t.contrast != s_tuning.contrast || t.gain != s_tuning.gain || t.levels != s_tuning.levels) {
        s_variant = (uint8_t)(s_variant % 255u + 1u);
    }
    s_tuning = t;
    if (t.log) {
        fprintf(stderr, "shiny: ambient=%.0f diffuse=%.2f room=%.2f contrast=%.2f gain=%.2f levels=%.2f"
                        " coverage=%.2f sweep=%.2f drift=%.2f sharp=%d bloom=%d gold=%d silver=%d\n",
                t.ambient, t.diffuse, t.room, t.contrast, t.gain, t.levels, t.coverage, t.sweep, t.drift, t.sharp,
                t.bloom, t.gold, t.silver);
    }
}

float fold(float x) /* a triangle wave through [-1, 1]: no clamped edge to stick on */
{
    float t = fmodf(x + 1.0f, 4.0f);
    if (t < 0.0f) t += 4.0f;
    return t <= 2.0f ? t - 1.0f : 3.0f - t;
}

float seconds()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    return std::chrono::duration<float>(clock::now() - start).count();
}

float luma_curve(float l) /* l in [0, 1] */
{
    const Tuning &t = s_tuning;
    if (t.contrast > 0.0f) { /* a logistic S-curve, pinned so 0 and 1 stay put */
        const float a = 1.0f + 11.0f * t.contrast;
        const float lo = 1.0f / (1.0f + expf(a * 0.5f));
        const float hi = 1.0f / (1.0f + expf(-a * 0.5f));
        l = (1.0f / (1.0f + expf(-a * (l - 0.5f))) - lo) / (hi - lo);
    }
    return l * (1.0f + t.gain * l * l); /* lifts the highlights, leaves the shadows */
}

} // namespace

void start_frame()
{
    if ((s_frame++ % 30u) == 0) reload();
}

const Tuning &tuning() { return s_tuning; }

bool applies(uint8_t item)
{
    if (!gfx_shiny_weapons) return false;
    const bool gold = item == 19 || item == 21; /* ITEM_GOLDENGUN, ITEM_GOLDWPPK (bondconstants.h) */
    return gold ? s_tuning.gold : s_tuning.silver;
}

uint8_t texture_variant() { return s_variant; }

float remap(float dot, int axis, const int8_t *lookat_x, const int8_t *lookat_y, bool lookat)
{
    const Tuning &t = s_tuning;
    if (t.coverage == 1.0f && t.sweep == 0.0f && t.drift == 0.0f) {
        return dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot);
    }
    float x = dot * t.coverage;
    if (t.sweep != 0.0f && lookat) {
        /* the camera's yaw from its right axis; pitch from its forward axis
         * (up x right), whose height is the sine of the pitch */
        const float yaw = atan2f(-(float)lookat_x[2], (float)lookat_x[0]);
        const float fy = ((float)lookat_y[2] * lookat_x[0] - (float)lookat_y[0] * lookat_x[2]) / (127.0f * 127.0f);
        const float pitch = asinf(fy < -1.0f ? -1.0f : (fy > 1.0f ? 1.0f : fy));
        x += t.sweep * (axis == 0 ? yaw : pitch) * (2.0f / 3.14159265f);
    }
    if (t.drift != 0.0f) x += 4.0f * t.drift * seconds() * (axis == 0 ? 1.0f : 0.37f);
    return fold(x);
}

void process(uint8_t *rgba, uint32_t width, uint32_t height)
{
    const Tuning &t = s_tuning;
    float lo = 1.0f, hi = 0.0f;
    for (uint32_t i = 0; i < width * height; i++) {
        const float l = (0.299f * rgba[i * 4] + 0.587f * rgba[i * 4 + 1] + 0.114f * rgba[i * 4 + 2]) / 255.0f;
        lo = l < lo ? l : lo;
        hi = l > hi ? l : hi;
    }
    if (t.log) fprintf(stderr, "shiny: reflection image %ux%u luma %.0f..%.0f\n", width, height, lo * 255, hi * 255);
    if (t.contrast <= 0.0f && t.gain <= 0.0f && t.levels <= 0.0f) return;
    for (uint32_t i = 0, n = width * height; i < n; i++, rgba += 4) {
        float c[3] = { rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f };
        const float l = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
        if (l <= 0.0f) continue;
        /* levels: towards the image's own range stretched over black to white */
        float ln = l;
        if (t.levels > 0.0f && hi > lo) ln += t.levels * ((l - lo) / (hi - lo) - l);
        const float k = luma_curve(ln) / l; /* scale the colour, so gold stays gold */
        float m = 0.0f;
        for (float &v : c) {
            v *= k;
            if (v > m) m = v;
        }
        if (m > 1.0f) { /* past white: keep the hue and fade towards white, like a specular hot spot */
            const float w = fminf(1.0f, m - 1.0f) * 0.6f;
            for (float &v : c) v = v / m + (1.0f - v / m) * w;
        }
        for (int j = 0; j < 3; j++) rgba[j] = (uint8_t)lroundf(c[j] * 255.0f);
    }
}

bool screen(Screen *out)
{
    const Tuning &t = s_tuning;
    if (!gfx_shiny_weapons || t.screen <= 0.0f) return false;
    *out = Screen{ t.screen, t.screen_gain, t.screen_back, t.screen_fov, t.screen_blur, t.screen_smooth,
                   t.screen_flip ? 1 : 0, t.map, t.screen_hue,
                   t.screen_mirror };
    return true;
}

bool glint_on(uint8_t item)
{
    /* ITEM_WPPK (4) to ITEM_SNIPERRIFLE (17), less any in glint_skip */
    return gfx_gun_glint && s_tuning.glint > 0.0f && item >= 4 && item <= 17 && !(s_tuning.glint_skip & (1u << item));
}

bool screen_applies(uint8_t item)
{
    const Tuning &t = s_tuning;
    if (!gfx_shiny_weapons || t.screen <= 0.0f) return false;
    const bool gold = item == 19 || item == 21; /* ITEM_GOLDENGUN, ITEM_GOLDWPPK */
    return gold ? t.gold : t.silver_screen;
}

bool bloom(Bloom *out)
{
    const Tuning &t = s_tuning;
    if (!gfx_shiny_weapons || t.bloom <= 0) return false;
    /* subtle: a soft halo on the hottest highlights; ludicrous: a wide,
     * multi-pass glare from most of the lit metal */
    *out = t.bloom == 1 ? Bloom{ 0.55f, 0.6f, 1.5f, 2 } : Bloom{ 0.25f, 5.0f, 2.0f, 4 };
    if (t.bloom_threshold >= 0.0f) out->threshold = t.bloom_threshold;
    out->strength *= t.bloom_strength;
    return true;
}

bool glass(uint8_t kind, Glass *out)
{
    const Tuning &t = s_tuning;
    if (!gfx_glass_reflections || kind == 0) return false;
    const bool shard = kind == 3; /* PORT_GLASS_SHARDS */
    *out = Glass{ shard ? t.glass_shard : t.glass, shard ? t.glass_shard_f0 : t.glass_f0, t.glass_glint,
                  t.glass_power, t.glass_blur, t.glass_split };
    return out->strength > 0.0f || out->glint > 0.0f;
}

} // namespace gfx_shiny

/* roc_hooks.c (sim thread), prototype: whether the level reflection wants the
 * room cache's capture-only mode. Reads the knob without reloading the file. */
extern "C" int gfx_shiny_level_cube(void)
{
    /* FORK-066: Glass Reflections sample the same cube */
    return (gfx_shiny_weapons || gfx_glass_reflections) && gfx_shiny::s_tuning.map > 0;
}
