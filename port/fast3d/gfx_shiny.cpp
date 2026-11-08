/* FORK-058: Shiny Gold Guns - the look's settings and image processing. See
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

namespace gfx_shiny {
namespace {

const char kFile[] = "shiny_weapons.ini";

/* The shipped look (the owner's pick in play, 2026-11-07): a gentle tone curve
 * on the gold guns only; every other knob neutral. */
const Tuning kDefaults = { -1.0f, 1.0f, 1.0f, 0.5f, 0.4f, 0.0f, 1.0f, 0.0f, 0.0f,
                           false, false, 0, -1.0f, 1.0f, true, false,
                           1.0f, 0.25f, 0.2f, true };
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
        else if (key == "walk") t.walk = v;
        else if (key == "env_tint") t.env_tint = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        else if (key == "env_bright") t.env_bright = v < 0.0f ? 0.0f : v;
        else if (key == "silver_motion") t.silver_motion = v != 0.0f;
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

static bool is_gold(uint8_t item)
{
    return item == 19 || item == 21; /* ITEM_GOLDENGUN, ITEM_GOLDWPPK (bondconstants.h) */
}

bool look(uint8_t item)
{
    return gfx_shiny_weapons && (is_gold(item) ? s_tuning.gold : s_tuning.silver);
}

bool motion(uint8_t item)
{
    return gfx_shiny_weapons && (is_gold(item) ? s_tuning.gold : s_tuning.silver_motion);
}

uint8_t texture_variant() { return s_variant; }

float remap(float dot, int axis, const int8_t *lookat_x, const int8_t *lookat_y, bool lookat, const View &view,
            bool tuned)
{
    const Tuning &t = s_tuning;
    const bool walk = t.walk != 0.0f && lookat && view.valid;
    const float coverage = tuned ? t.coverage : 1.0f, sweep = tuned ? t.sweep : 0.0f, drift = tuned ? t.drift : 0.0f;
    if (coverage == 1.0f && sweep == 0.0f && drift == 0.0f && !walk) {
        return dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot);
    }
    float x = dot * coverage;
    if (walk) {
        /* the camera's ground position along its own right axis moves the
         * pattern sideways and along its forward axis up and down, so strafing
         * and walking slide the reflection like scenery passing in the metal.
         * Height is left out: the head bob jittered it. */
        float rx = lookat_x[0], rz = lookat_x[2];
        const float rl = sqrtf(rx * rx + rz * rz);
        if (rl > 0.0f) {
            rx /= rl;
            rz /= rl;
            const float *p = view.pos;
            const float along = axis == 0 ? p[0] * rx + p[2] * rz : p[2] * rx - p[0] * rz;
            /* a slow, bounded sway around the stock pattern: amplitude 0.15 of
             * the reflection's range per unit of walk, one swing per ~6,300
             * units. The first tries were unbounded (the phase drifted onto
             * dark bands) and then ten times as fast (the owner found the whole
             * reflection changed with small steps, 2026-11-08). */
            x += t.walk * 0.15f * sinf(along / 1000.0f);
        }
    }
    if (sweep != 0.0f && lookat) {
        /* the camera's yaw from its right axis; pitch from its forward axis
         * (up x right), whose height is the sine of the pitch */
        const float yaw = atan2f(-(float)lookat_x[2], (float)lookat_x[0]);
        const float fy = ((float)lookat_y[2] * lookat_x[0] - (float)lookat_y[0] * lookat_x[2]) / (127.0f * 127.0f);
        const float pitch = asinf(fy < -1.0f ? -1.0f : (fy > 1.0f ? 1.0f : fy));
        x += sweep * (axis == 0 ? yaw : pitch) * (2.0f / 3.14159265f);
    }
    if (drift != 0.0f) x += 4.0f * drift * seconds() * (axis == 0 ? 1.0f : 0.37f);
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

void tint(int rgb[3], uint8_t fog_alpha, const View &view)
{
    const Tuning &t = s_tuning;
    if (!view.valid || (t.env_tint <= 0.0f && t.env_bright <= 0.0f)) return;
    /* set_color_shading_from_tile: alpha = (255 - floor luminance) x 0.75 */
    float lit = 1.0f - fog_alpha / 191.0f;
    lit = lit < 0.0f ? 0.0f : (lit > 1.0f ? 1.0f : lit);
    float hue[3] = { 1.0f, 1.0f, 1.0f };
    const int top = view.sky[0] > view.sky[1] ? (view.sky[0] > view.sky[2] ? view.sky[0] : view.sky[2])
                                              : (view.sky[1] > view.sky[2] ? view.sky[1] : view.sky[2]);
    if (top >= 24) { /* a black sky (indoor levels) tints nothing */
        for (int i = 0; i < 3; i++) hue[i] = (float)view.sky[i] / (float)top;
    }
    const float w = t.env_tint * lit, gain = 1.0f + t.env_bright * lit;
    for (int i = 0; i < 3; i++) rgb[i] = (int)lroundf(rgb[i] * (1.0f + w * (hue[i] - 1.0f)) * gain);
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

} // namespace gfx_shiny
