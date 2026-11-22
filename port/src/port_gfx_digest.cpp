/**
 * The picture digest (LP64-006 Task 7; port_gfx_digest.h has the line's
 * contract). A wrapper around the backend's GfxRenderingAPI table: every
 * entry hashes what it is asked, then calls the backend's entry, which
 * GE_GFXDIGEST_NODRAW skips for the entries the interpreter reads nothing
 * back from. It asks the GL driver nothing of its own. CMake compiles this
 * file only with GE_COMPARE and GE_VIDEO.
 */
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include "port_gfx_digest.h"
#include "../fast3d/gfx_rendering_api.h"

extern "C" {
extern int32_t g_GlobalTimer;       /* src/game/lv.c */
int32_t portRamromReplayTick(void); /* src/game/ramromreplay.c: -1 when no recording plays */
}

/* Every entry of struct GfxRenderingAPI, in its order. A wrapper w_NAME must
 * exist for each, and the static assertion below fails the build when the
 * struct gains an entry this list lacks. */
#define GFX_DIGEST_ENTRIES(X)                                                                                     \
    X(get_name) X(get_max_texture_size) X(get_clip_parameters) X(unload_shader) X(load_shader)                 \
    X(create_and_load_new_shader) X(lookup_shader) X(shader_get_info) X(clear_shaders) X(new_texture)         \
    X(select_texture) X(upload_texture) X(set_sampler_parameters) X(set_depth_mode) X(set_depth_range)        \
    X(set_viewport) X(set_scissor) X(set_use_alpha) X(draw_triangles) X(init) X(on_resize) X(start_frame)     \
    X(end_frame) X(finish_render) X(create_framebuffer) X(update_framebuffer_parameters)                       \
    X(start_draw_to_framebuffer) X(copy_framebuffer) X(clear_framebuffer) X(resolve_msaa_color_buffer)        \
    X(get_framebuffer_texture_id) X(select_texture_fb) X(delete_texture) X(set_texture_filter)                \
    X(get_texture_filter) X(retained_create) X(retained_destroy) X(retained_build) X(retained_draw)           \
    X(retained_matrices) X(retained_draw_indexed) X(resolve_n64_aa) X(bloom_begin) X(bloom_mark) X(bloom_end) \
    X(env_begin) X(env_end) X(glint_draw) X(env_cube_begin) X(env_cube_face) X(env_cube_end) X(glass_draw)    \
    X(grade)

#define GFX_DIGEST_ENUM(name) E_##name,
#define GFX_DIGEST_NAME(name) #name,
enum GfxDigestEntry { GFX_DIGEST_ENTRIES(GFX_DIGEST_ENUM) kGfxDigestEntries };
static const char* const kGfxDigestEntryNames[] = { GFX_DIGEST_ENTRIES(GFX_DIGEST_NAME) };
static_assert(sizeof(GfxRenderingAPI) == kGfxDigestEntries * sizeof(void (*)(void)),
              "struct GfxRenderingAPI has an entry GFX_DIGEST_ENTRIES lacks: wrap and hash it");

namespace {

constexpr uint32_t kFnvOffset = 0x811c9dc5u;
constexpr uint32_t kFnvPrime = 0x01000193u;
constexpr uint32_t kUnknown = 0xffffffffu;   /* a handle the backend never handed out through this table */
constexpr uint32_t kNone = 0xfffffffeu;      /* a NULL argument, or a texture the shader does not sample */
constexpr uint32_t kFresh = 0xfffffffdu;     /* a texture nothing has been uploaded into yet */
constexpr uint32_t kFbTexture = 0x80000000u; /* a framebuffer's texture: its framebuffer's order, marked */

/* The frame's parts, and NONE for bookkeeping: names, frees, queries and
 * lookups. A demo's frames before its recording plays follow the host's pace,
 * and the texture cache's new names, recycled ids and frees follow those
 * frames (dam1, Task 7), so they hash into no part; what they make reaches the
 * frame through what is drawn with it. */
enum Part { VTX, TEX, STATE, POST, NONE, PARTS };
const char* const kPartNames[PARTS] = { "vtx", "tex", "state", "post", "none" };

/* FNV-1a over VALUE's four bytes, lowest first, whatever the host's order. */
inline uint32_t fnv(uint32_t hash, uint32_t value) {
    hash = (hash ^ (value & 0xffu)) * kFnvPrime;
    hash = (hash ^ ((value >> 8) & 0xffu)) * kFnvPrime;
    hash = (hash ^ ((value >> 16) & 0xffu)) * kFnvPrime;
    return (hash ^ (value >> 24)) * kFnvPrime;
}

struct Shader { /* a program's combiner ids, and the textures it samples (both until shader_get_info says) */
    uint64_t id0;
    uint32_t id1;
    bool used[2];
};
struct Built { /* a retained descriptor's built contents and triangles */
    uint32_t hash, tris;
};

struct Digest {
    FILE* file = nullptr;
    const char* path = nullptr;
    bool nodraw = false;
    uint32_t detailFirst = 1, detailLast = 0; /* first > last: no detail */
    uint32_t frame = 1;                       /* the frame being hashed, from 1 */
    uint32_t draws = 0, tris = 0, uploads = 0, calls = 0;
    uint32_t sums[PARTS] = {};
    std::unordered_map<const ShaderProgram*, Shader> shaders; /* looked up by address, never ordered by it */
    const ShaderProgram* loaded = nullptr;
    /* Framebuffers by the order the backend handed them out (1, 2, ...): the
     * interpreter makes them at boot and for its own passes. Textures are
     * named by their contents: their last upload's hash, or kFresh. */
    std::unordered_map<int, uint32_t> framebuffers;
    std::unordered_map<uint32_t, uint32_t> contents;
    std::unordered_map<uint64_t, uint32_t> fbTextures; /* get_framebuffer_texture_id's results */
    uint32_t tiles[2] = {};                            /* the texture each tile has selected */
    uint32_t bound = 0;                                /* the last selected, which an upload fills */
    std::unordered_map<int, Built> built;              /* retained_build's descriptors */
};

Digest s;
GfxRenderingAPI sInner;
GfxRenderingAPI sTable;

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "ge: FATAL: GE_GFXDIGEST %s %s: %s\n", what, s.path, std::strerror(errno));
    std::fflush(nullptr);
    std::exit(1);
}

void startSums() {
    for (uint32_t& sum : s.sums) {
        sum = kFnvOffset;
    }
    s.draws = s.tris = s.uploads = s.calls = 0;
}

uint32_t textureName(uint32_t id) {
    const auto content = s.contents.find(id);
    const auto fb = s.fbTextures.find(id);
    return content != s.contents.end() ? content->second : fb != s.fbTextures.end() ? fb->second : kUnknown;
}
uint32_t framebufferName(int id) {
    const auto found = s.framebuffers.find(id);
    return id == 0 ? 0u : found == s.framebuffers.end() ? kUnknown : found->second;
}

/* One call: hashes its entry number into its part, then each value given, into
 * the part and into the call's own hash for its detail line. A Call made
 * without an entry only hashes, for a name (a texture's contents). */
class Call {
  public:
    Call(GfxDigestEntry entry, Part part) : entry_(entry), part_(part) {
        s.calls++;
        u(entry);
    }
    Call() : entry_(kGfxDigestEntries), part_(NONE) {}
    ~Call() {
        if (entry_ == kGfxDigestEntries || s.frame < s.detailFirst || s.frame > s.detailLast) {
            return;
        }
        if (std::fprintf(s.file, "C n=%u k=%u entry=%s part=%s hash=%08x\n", s.frame, s.calls,
                         kGfxDigestEntryNames[entry_], kPartNames[part_], own_) < 0) {
            fail("cannot write");
        }
    }
    Call& u(uint32_t value) { return into(part_, value); }
    /* VALUE into PART instead of the call's own part (a draw's texture names into tex). */
    Call& into(Part part, uint32_t value) {
        s.sums[part] = fnv(s.sums[part], value);
        own_ = fnv(own_, value);
        return *this;
    }
    Call& i(int32_t value) { return u((uint32_t)value); }
    Call& b(bool value) { return u(value ? 1u : 0u); }
    Call& u64(uint64_t value) { return u((uint32_t)value).u((uint32_t)(value >> 32)); }
    Call& f(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return u(bits);
    }
    /* COUNT floats, or a marker for a NULL array. */
    Call& floats(const float* values, size_t count) {
        if (values == nullptr) return u(kNone);
        u((uint32_t)count);
        for (size_t n = 0; n < count; n++) f(values[n]);
        return *this;
    }
    Call& bytes(const uint8_t* data, size_t count) {
        if (data == nullptr) return u(kNone);
        u((uint32_t)count);
        size_t n = 0;
        for (; n + 4 <= count; n += 4) {
            u((uint32_t)data[n] | (uint32_t)data[n + 1] << 8 | (uint32_t)data[n + 2] << 16 |
              (uint32_t)data[n + 3] << 24);
        }
        for (; n < count; n++) u(data[n]);
        return *this;
    }
    Call& shader(const ShaderProgram* prg) {
        const auto found = s.shaders.find(prg);
        return found == s.shaders.end() ? u(prg == nullptr ? kNone : kUnknown)
                                        : u64(found->second.id0).u(found->second.id1);
    }
    Call& framebuffer(int id) { return u(framebufferName(id)); }
    /* The contents of the textures the loaded shader samples, into tex. */
    Call& sampled() {
        const auto found = s.shaders.find(s.loaded);
        for (int t = 0; t < 2; t++) {
            const bool used = found == s.shaders.end() || found->second.used[t];
            into(TEX, used ? textureName(s.tiles[t]) : kNone);
        }
        return *this;
    }
    uint32_t own() const { return own_; }

  private:
    GfxDigestEntry entry_;
    Part part_;
    uint32_t own_ = kFnvOffset;
};

#define DRAW(call) if (!s.nodraw) sInner.call /* the backend's entry, unless drawing is skipped */
#define WRAP(name, part) Call c(E_##name, part)
/* w_NAME(PARAMS): hashes HASH into PART, then draws NAME(ARGS). */
#define SIMPLE(name, part, params, hash, args) \
    void w_##name params {                      \
        WRAP(name, part);                       \
        c hash;                                 \
        DRAW(name args);                        \
    }

ShaderProgram* remember(Call& c, ShaderProgram* prg, uint64_t id0, uint32_t id1, bool load) {
    if (prg != nullptr && s.shaders.count(prg) == 0) {
        s.shaders[prg] = { id0, id1, { true, true } };
    }
    s.loaded = load ? prg : s.loaded;
    c.b(prg != nullptr);
    return prg;
}
/* ---- queries and shaders --------------------------------------------------- */
const char* w_get_name(void) { WRAP(get_name, NONE); return sInner.get_name(); }
int w_get_max_texture_size(void) { WRAP(get_max_texture_size, NONE); return sInner.get_max_texture_size(); }
GfxClipParameters w_get_clip_parameters(void) { WRAP(get_clip_parameters, NONE); return sInner.get_clip_parameters(); }
SIMPLE(unload_shader, STATE, (ShaderProgram* old_prg), .shader(old_prg), (old_prg))
void w_load_shader(ShaderProgram* new_prg) {
    WRAP(load_shader, STATE);
    c.shader(s.loaded = new_prg);
    DRAW(load_shader(new_prg));
}
ShaderProgram* w_create_and_load_new_shader(uint64_t id0, uint32_t id1) {
    WRAP(create_and_load_new_shader, STATE);
    c.u64(id0).u(id1);
    return remember(c, sInner.create_and_load_new_shader(id0, id1), id0, id1, true);
}
ShaderProgram* w_lookup_shader(uint64_t id0, uint32_t id1) {
    WRAP(lookup_shader, NONE);
    c.u64(id0).u(id1);
    return remember(c, sInner.lookup_shader(id0, id1), id0, id1, false);
}
void w_shader_get_info(ShaderProgram* prg, uint8_t* num_inputs, bool used_textures[2]) {
    WRAP(shader_get_info, NONE);
    c.shader(prg);
    sInner.shader_get_info(prg, num_inputs, used_textures);
    const auto found = s.shaders.find(prg);
    if (found != s.shaders.end()) {
        found->second.used[0] = used_textures[0];
        found->second.used[1] = used_textures[1];
    }
}
void w_clear_shaders(void) {
    WRAP(clear_shaders, NONE);
    s.shaders.clear(); /* the backend frees them: an address may come back for another program */
    s.loaded = nullptr;
    sInner.clear_shaders();
}
/* ---- textures -------------------------------------------------------------- */
uint32_t w_new_texture(void) {
    WRAP(new_texture, NONE);
    const uint32_t id = sInner.new_texture();
    s.contents[id] = kFresh;
    return id;
}
void w_select_texture(int tile, uint32_t texture_id, bool linear_filter) {
    WRAP(select_texture, STATE);
    c.i(tile).b(linear_filter); /* the texture itself: its contents, at the draws that sample it */
    s.tiles[tile & 1] = s.bound = texture_id;
    DRAW(select_texture(tile, texture_id, linear_filter));
}
void w_upload_texture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    WRAP(upload_texture, TEX);
    Call content; /* the upload alone: the bound texture's new name */
    content.u(width).u(height).bytes(rgba32_buf, (size_t)width * height * 4);
    s.uploads++;
    c.u(content.own());
    if (s.contents.count(s.bound) != 0) {
        s.contents[s.bound] = content.own();
    }
    DRAW(upload_texture(rgba32_buf, width, height));
}
SIMPLE(set_sampler_parameters, STATE, (int sampler, bool linear_filter, uint32_t cms, uint32_t cmt),
       .i(sampler).b(linear_filter).u(cms).u(cmt), (sampler, linear_filter, cms, cmt))
void w_delete_texture(uint32_t texID) {
    WRAP(delete_texture, NONE);
    c.u(textureName(texID));
    s.contents.erase(texID);
    sInner.delete_texture(texID);
}
void w_set_texture_filter(FilteringMode mode) {
    WRAP(set_texture_filter, STATE);
    c.u((uint32_t)mode);
    sInner.set_texture_filter(mode); /* get_texture_filter answers it */
}
FilteringMode w_get_texture_filter(void) { WRAP(get_texture_filter, NONE); return sInner.get_texture_filter(); }
/* ---- state and draws ------------------------------------------------------- */
SIMPLE(set_depth_mode, STATE, (bool depth_test, bool depth_update, bool depth_compare, bool depth_source_prim,
       uint16_t zmode),
       .b(depth_test).b(depth_update).b(depth_compare).b(depth_source_prim).u(zmode),
       (depth_test, depth_update, depth_compare, depth_source_prim, zmode))
SIMPLE(set_depth_range, STATE, (float znear, float zfar), .f(znear).f(zfar), (znear, zfar))
SIMPLE(set_viewport, STATE, (int x, int y, int width, int height), .i(x).i(y).i(width).i(height), (x, y, width, height))
SIMPLE(set_scissor, STATE, (int x, int y, int width, int height), .i(x).i(y).i(width).i(height), (x, y, width, height))
SIMPLE(set_use_alpha, STATE, (bool use_alpha, bool modulate), .b(use_alpha).b(modulate), (use_alpha, modulate))
void w_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    WRAP(draw_triangles, VTX);
    s.draws++;
    s.tris += (uint32_t)buf_vbo_num_tris;
    c.u((uint32_t)buf_vbo_num_tris).floats(buf_vbo, buf_vbo_len).sampled();
    DRAW(draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris));
}
/* ---- the frame ------------------------------------------------------------- */
void w_init(void) { WRAP(init, NONE); sInner.init(); }
void w_on_resize(void) { WRAP(on_resize, NONE); sInner.on_resize(); }
void w_start_frame(void) { WRAP(start_frame, STATE); sInner.start_frame(); }
void w_end_frame(void) {
    const int32_t demo = portRamromReplayTick();
    char tail[32] = "";
    if (demo >= 0) {
        std::snprintf(tail, sizeof(tail), " demo=%d", (int)demo);
    }
    if (std::fprintf(s.file, "F n=%u tick=%d draws=%u tris=%u vtx=%08x tex=%u:%08x state=%08x post=%08x%s\n",
                     s.frame, (int)g_GlobalTimer, s.draws, s.tris, s.sums[VTX], s.uploads, s.sums[TEX],
                     s.sums[STATE], s.sums[POST], tail) < 0 ||
        std::fflush(s.file) == EOF) {
        fail("cannot write");
    }
    s.frame++;
    startSums();
    sInner.end_frame();
}
void w_finish_render(void) { WRAP(finish_render, STATE); sInner.finish_render(); }
/* ---- framebuffers ---------------------------------------------------------- */
int w_create_framebuffer(void) {
    WRAP(create_framebuffer, NONE);
    const int id = sInner.create_framebuffer();
    c.u(s.framebuffers[id] = (uint32_t)s.framebuffers.size() + 1);
    return id;
}
void w_update_framebuffer_parameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invert_y, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) {
    WRAP(update_framebuffer_parameters, STATE);
    c.framebuffer(fb_id).u(width).u(height).u(msaa_level).b(opengl_invert_y).b(render_target).b(has_depth_buffer);
    c.b(can_extract_depth);
    /* forwarded either way: get_clip_parameters answers from it */
    sInner.update_framebuffer_parameters(fb_id, width, height, msaa_level, opengl_invert_y, render_target,
                                         has_depth_buffer, can_extract_depth);
}
bool w_start_draw_to_framebuffer(int fb_id, float noise_scale) {
    WRAP(start_draw_to_framebuffer, STATE);
    c.framebuffer(fb_id).f(noise_scale);
    const bool ok = sInner.start_draw_to_framebuffer(fb_id, noise_scale);
    return c.b(ok), ok;
}
SIMPLE(copy_framebuffer, POST, (int fb_dst, int fb_src, int left, int top, bool flip_y, bool use_back),
       .framebuffer(fb_dst).framebuffer(fb_src).i(left).i(top).b(flip_y).b(use_back),
       (fb_dst, fb_src, left, top, flip_y, use_back))
SIMPLE(clear_framebuffer, STATE, (bool clear_color, bool clear_depth),
       .b(clear_color).b(clear_depth), (clear_color, clear_depth))
SIMPLE(resolve_msaa_color_buffer, POST, (int fb_id_target, int fb_id_source),
       .framebuffer(fb_id_target).framebuffer(fb_id_source), (fb_id_target, fb_id_source))
void* w_get_framebuffer_texture_id(int fb_id) {
    WRAP(get_framebuffer_texture_id, NONE);
    c.framebuffer(fb_id);
    void* id = sInner.get_framebuffer_texture_id(fb_id);
    s.fbTextures[(uint64_t)(uintptr_t)id] = kFbTexture | framebufferName(fb_id);
    return id;
}
SIMPLE(select_texture_fb, STATE, (int fb_id), .framebuffer(fb_id), (fb_id))

/* ---- retained geometry ----------------------------------------------------- */
int w_retained_create(uint32_t vertex_count) {
    WRAP(retained_create, NONE);
    const int buffer = sInner.retained_create(vertex_count);
    c.u(vertex_count).b(buffer >= 0);
    return buffer;
}
void w_retained_destroy(int buffer) { WRAP(retained_destroy, NONE); sInner.retained_destroy(buffer); }
int w_retained_build(int buffer, uint32_t first_vertex, ShaderProgram* prg, const float* buf_vbo, size_t stride,
                     size_t num_tris, const float* model_pos, const float* model_nrm, const uint32_t tex_id[2],
                     const bool tex_used[2], const uint8_t input_kind[8], int shade_slot) {
    WRAP(retained_build, VTX); /* its contents, never the buffer's handle */
    const size_t vertices = num_tris * 3;
    c.u(first_vertex).shader(prg).u((uint32_t)stride).u((uint32_t)num_tris).floats(buf_vbo, stride * vertices);
    c.floats(model_pos, 4 * vertices).floats(model_nrm, 3 * vertices);
    for (int t = 0; t < 2; t++) {
        c.u(tex_used[t] ? textureName(tex_id[t]) : kNone).b(tex_used[t]);
    }
    c.bytes(input_kind, 8).i(shade_slot);
    const int desc = sInner.retained_build(buffer, first_vertex, prg, buf_vbo, stride, num_tris, model_pos, model_nrm,
                                           tex_id, tex_used, input_kind, shade_slot);
    if (desc >= 0) {
        s.built[desc] = { c.own(), (uint32_t)num_tris };
    }
    return c.b(desc >= 0), desc;
}
/* A retained draw of TRIS triangles (-1: all its descriptor's): the built contents and its arguments. */
void hashRetained(Call& c, int desc, int64_t tris, const float mvp[16], const float fixup[4], const float fogp[4],
                  const float fogsel[4], const float fog_rgb[3], uint32_t cull_mode, const float* lights,
                  const float* prim_env, const float* decal) {
    const auto found = s.built.find(desc);
    s.draws++;
    s.tris += tris >= 0 ? (uint32_t)tris : found == s.built.end() ? 0 : found->second.tris;
    c.u(found == s.built.end() ? kUnknown : found->second.hash);
    c.floats(mvp, 16).floats(fixup, 4).floats(fogp, 4).floats(fogsel, 4).floats(fog_rgb, 3).u(cull_mode);
    c.floats(lights, 20).floats(prim_env, 12).floats(decal, 4).sampled();
}
void w_retained_draw(int desc, const float mvp[16], const float fixup[4], const float fogp[4], const float fogsel[4],
                     const float fog_rgb[3], uint32_t cull_mode, const float* lights, const float* prim_env,
                     const float* decal) {
    WRAP(retained_draw, VTX);
    hashRetained(c, desc, -1, mvp, fixup, fogp, fogsel, fog_rgb, cull_mode, lights, prim_env, decal);
    DRAW(retained_draw(desc, mvp, fixup, fogp, fogsel, fog_rgb, cull_mode, lights, prim_env, decal));
}
SIMPLE(retained_matrices, VTX, (int first_slot, int count, const float* mats),
       .i(first_slot).floats(mats, count > 0 ? (size_t)count * 16 : 0), (first_slot, count, mats))
void w_retained_draw_indexed(int desc, const uint16_t* idx, uint32_t n, const float mvp[16], const float fixup[4],
                             const float fogp[4], const float fogsel[4], const float fog_rgb[3], uint32_t cull_mode,
                             const float* lights, const float* prim_env, const float* decal) {
    WRAP(retained_draw_indexed, VTX);
    c.u(n);
    for (uint32_t k = 0; idx != nullptr && k < n; k++) {
        c.u(idx[k]);
    }
    hashRetained(c, desc, n / 3, mvp, fixup, fogp, fogsel, fog_rgb, cull_mode, lights, prim_env, decal);
    DRAW(retained_draw_indexed(desc, idx, n, mvp, fixup, fogp, fogsel, fog_rgb, cull_mode, lights, prim_env, decal));
}
/* ---- the optional passes --------------------------------------------------- */
SIMPLE(resolve_n64_aa, POST, (int fb_dst, int fb_src), .framebuffer(fb_dst).framebuffer(fb_src), (fb_dst, fb_src))
void w_bloom_begin(void) { WRAP(bloom_begin, POST); DRAW(bloom_begin()); }
SIMPLE(bloom_mark, POST, (bool shiny), .b(shiny), (shiny))
SIMPLE(bloom_end, POST, (float threshold, float strength, float radius, int passes),
       .f(threshold).f(strength).f(radius).i(passes), (threshold, strength, radius, passes))
SIMPLE(env_begin, POST, (float mix, float gain, float back, float fov_y, float blur, int flip, float smooth, int key,
       int source, float hue, float mirror),
       .f(mix).f(gain).f(back).f(fov_y).f(blur).i(flip).f(smooth).i(key).i(source).f(hue).f(mirror),
       (mix, gain, back, fov_y, blur, flip, smooth, key, source, hue, mirror))
void w_env_end(void) { WRAP(env_end, POST); DRAW(env_end()); }
SIMPLE(glint_draw, POST, (const float* buf, size_t vertex_count), .floats(buf, vertex_count * 15), (buf, vertex_count))
SIMPLE(env_cube_begin, POST, (int size, float r, float g, float b), .i(size).f(r).f(g).f(b), (size, r, g, b))
SIMPLE(env_cube_face, POST, (int face), .i(face), (face))
SIMPLE(env_cube_end, POST, (const char* dump), .b(dump != nullptr), (dump)) /* a path is the host's */
SIMPLE(glass_draw, POST, (const float* buf, size_t vertex_count, bool cube, bool split),
       .floats(buf, vertex_count * 18).b(cube).b(split), (buf, vertex_count, cube, split))
void w_grade(const GfxGradeParams* p) {
    WRAP(grade, POST);
    if (p == nullptr) {
        c.u(kNone);
    } else { /* field by field: the struct's padding is not content */
        c.b(p->tonemap).f(p->exposure).f(p->expand).f(p->brightness).f(p->contrast).f(p->filmic).f(p->vibrance);
        c.f(p->clarity).f(p->sharpen).f(p->glow).f(p->peak).f(p->saturation).floats(p->temperature, 3);
        c.f(p->split_tone).floats(p->shadow_tint, 3).floats(p->high_tint, 3).f(p->fade).f(p->vignette);
        c.f(p->grain).f(p->auto_exposure).f(p->adapt_speed).b(p->split);
    }
    DRAW(grade(p));
}

#undef DRAW
#undef WRAP
#undef SIMPLE

void readDetailRange() {
    const char* e = std::getenv("GE_GFXDIGEST_DETAIL");
    unsigned first, last;
    char extra;
    if (e == nullptr || e[0] == '\0') return;
    if (std::sscanf(e, "%u-%u%c", &first, &last, &extra) != 2 || first > last) {
        std::fprintf(stderr, "ge: GE_GFXDIGEST_DETAIL=%s ignored: it takes A-B, A at most B\n", e);
        return;
    }
    s.detailFirst = first;
    s.detailLast = last;
}

}  // namespace

extern "C" struct GfxRenderingAPI* portGfxDigestWrap(struct GfxRenderingAPI* inner) {
    if (s.file != nullptr) std::fclose(s.file);
    s = Digest();
    const char* path = std::getenv("GE_GFXDIGEST");
    if (inner == nullptr || path == nullptr || path[0] == '\0') {
        return inner;
    }
    s.path = path;
    s.file = std::fopen(path, "w");
    if (s.file == nullptr) {
        fail("cannot open");
    }
    const char* nodraw = std::getenv("GE_GFXDIGEST_NODRAW");
    s.nodraw = nodraw != nullptr && std::strcmp(nodraw, "1") == 0;
    readDetailRange();
    startSums();
    sInner = *inner;
#define GFX_DIGEST_WIRE(name) sTable.name = inner->name != nullptr ? w_##name : nullptr;
    GFX_DIGEST_ENTRIES(GFX_DIGEST_WIRE)
#undef GFX_DIGEST_WIRE
    return &sTable;
}
