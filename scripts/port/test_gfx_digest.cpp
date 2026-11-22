/* ROM-free checks of the picture digest (LP64-006 Task 7, test_gfx_digest.sh):
 * port_gfx_digest.cpp wraps a stand-in rendering API that counts its calls,
 * and each check drives one frame through the wrapper and reads the F line it
 * writes. port_gfx_digest.cpp is included whole, so the checks see its entry
 * list; this file defines the two game values it reads. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>
#include "port_gfx_digest.h"
#include "../../port/src/port_gfx_digest.cpp"

/* The game values port_gfx_digest.cpp reads. */
extern "C" {
int32_t g_GlobalTimer;
static int32_t sReplayTick = -1;
int32_t portRamromReplayTick(void) { return sReplayTick; }
}

namespace {

int failures;
std::string sPath;

void expect(bool ok, const char* what) {
    if (!ok && ++failures) std::printf("test_gfx_digest: FAIL (%d-bit): %s\n", (int)(sizeof(void*) * 8), what);
}

/* ---- the stand-in inner table: every entry counts its calls ---------------- */
int sCalls[kGfxDigestEntries];
uint32_t sNextTexture;
int sNextFramebuffer, sNextRetained, sNextProgram;
alignas(8) unsigned char sPrograms[8][16]; /* stand-in ShaderPrograms: only their addresses are used */

#define COUNT(name) sCalls[E_##name]++
const char* s_get_name(void) { COUNT(get_name); return "stand-in"; }
int s_get_max_texture_size(void) { COUNT(get_max_texture_size); return 4096; }
GfxClipParameters s_get_clip_parameters(void) { COUNT(get_clip_parameters); return { false, true }; }
void s_unload_shader(ShaderProgram*) { COUNT(unload_shader); }
void s_load_shader(ShaderProgram*) { COUNT(load_shader); }
ShaderProgram* s_create_and_load_new_shader(uint64_t, uint32_t) {
    return COUNT(create_and_load_new_shader), reinterpret_cast<ShaderProgram*>(sPrograms[sNextProgram++ % 8]);
}
ShaderProgram* s_lookup_shader(uint64_t, uint32_t) { COUNT(lookup_shader); return nullptr; }
void s_shader_get_info(ShaderProgram*, uint8_t* num_inputs, bool used[2]) {
    COUNT(shader_get_info);
    *num_inputs = 2, used[0] = true, used[1] = false;
}
void s_clear_shaders(void) { COUNT(clear_shaders); }
uint32_t s_new_texture(void) { COUNT(new_texture); return ++sNextTexture; }
void s_select_texture(int, uint32_t, bool) { COUNT(select_texture); }
void s_upload_texture(const uint8_t*, uint32_t, uint32_t) { COUNT(upload_texture); }
void s_set_sampler_parameters(int, bool, uint32_t, uint32_t) { COUNT(set_sampler_parameters); }
void s_set_depth_mode(bool, bool, bool, bool, uint16_t) { COUNT(set_depth_mode); }
void s_set_depth_range(float, float) { COUNT(set_depth_range); }
void s_set_viewport(int, int, int, int) { COUNT(set_viewport); }
void s_set_scissor(int, int, int, int) { COUNT(set_scissor); }
void s_set_use_alpha(bool, bool) { COUNT(set_use_alpha); }
void s_draw_triangles(float[], size_t, size_t) { COUNT(draw_triangles); }
void s_init(void) { COUNT(init); }
void s_on_resize(void) { COUNT(on_resize); }
void s_start_frame(void) { COUNT(start_frame); }
void s_end_frame(void) { COUNT(end_frame); }
void s_finish_render(void) { COUNT(finish_render); }
int s_create_framebuffer(void) { COUNT(create_framebuffer); return ++sNextFramebuffer; }
void s_update_framebuffer_parameters(int, uint32_t, uint32_t, uint32_t, bool, bool, bool, bool) {
    COUNT(update_framebuffer_parameters); }
bool s_start_draw_to_framebuffer(int, float) { COUNT(start_draw_to_framebuffer); return true; }
void s_copy_framebuffer(int, int, int, int, bool, bool) { COUNT(copy_framebuffer); }
void s_clear_framebuffer(bool, bool) { COUNT(clear_framebuffer); }
void s_resolve_msaa_color_buffer(int, int) { COUNT(resolve_msaa_color_buffer); }
void* s_get_framebuffer_texture_id(int) { COUNT(get_framebuffer_texture_id); return nullptr; }
void s_select_texture_fb(int) { COUNT(select_texture_fb); }
void s_delete_texture(uint32_t) { COUNT(delete_texture); }
void s_set_texture_filter(FilteringMode) { COUNT(set_texture_filter); }
FilteringMode s_get_texture_filter(void) { COUNT(get_texture_filter); return FILTER_LINEAR; }
int s_retained_create(uint32_t) { COUNT(retained_create); return sNextRetained++; }
void s_retained_destroy(int) { COUNT(retained_destroy); }
int s_retained_build(int, uint32_t, ShaderProgram*, const float*, size_t, size_t, const float*, const float*,
                     const uint32_t[2], const bool[2], const uint8_t[8], int) {
    return COUNT(retained_build), sNextRetained++; }
void s_retained_draw(int, const float[16], const float[4], const float[4], const float[4], const float[3], uint32_t,
                     const float*, const float*, const float*) { COUNT(retained_draw); }
void s_retained_matrices(int, int, const float*) { COUNT(retained_matrices); }
void s_retained_draw_indexed(int, const uint16_t*, uint32_t, const float[16], const float[4], const float[4],
                             const float[4], const float[3], uint32_t, const float*, const float*, const float*) {
    COUNT(retained_draw_indexed); }
void s_resolve_n64_aa(int, int) { COUNT(resolve_n64_aa); }
void s_bloom_begin(void) { COUNT(bloom_begin); }
void s_bloom_mark(bool) { COUNT(bloom_mark); }
void s_bloom_end(float, float, float, int) { COUNT(bloom_end); }
void s_env_begin(float, float, float, float, float, int, float, int, int, float, float) { COUNT(env_begin); }
void s_env_end(void) { COUNT(env_end); }
void s_glint_draw(const float*, size_t) { COUNT(glint_draw); }
void s_env_cube_begin(int, float, float, float) { COUNT(env_cube_begin); }
void s_env_cube_face(int) { COUNT(env_cube_face); }
void s_env_cube_end(const char*) { COUNT(env_cube_end); }
void s_glass_draw(const float*, size_t, bool, bool) { COUNT(glass_draw); }
void s_grade(const GfxGradeParams*) { COUNT(grade); }
#undef COUNT

GfxRenderingAPI standIn() {
    return GfxRenderingAPI{
        s_get_name, s_get_max_texture_size, s_get_clip_parameters, s_unload_shader, s_load_shader,
        s_create_and_load_new_shader, s_lookup_shader, s_shader_get_info, s_clear_shaders, s_new_texture,
        s_select_texture, s_upload_texture, s_set_sampler_parameters, s_set_depth_mode, s_set_depth_range,
        s_set_viewport, s_set_scissor, s_set_use_alpha, s_draw_triangles, s_init, s_on_resize, s_start_frame,
        s_end_frame, s_finish_render, s_create_framebuffer, s_update_framebuffer_parameters,
        s_start_draw_to_framebuffer, s_copy_framebuffer, s_clear_framebuffer, s_resolve_msaa_color_buffer,
        s_get_framebuffer_texture_id, s_select_texture_fb, s_delete_texture, s_set_texture_filter,
        s_get_texture_filter, s_retained_create, s_retained_destroy, s_retained_build, s_retained_draw,
        s_retained_matrices, s_retained_draw_indexed, s_resolve_n64_aa, s_bloom_begin, s_bloom_mark, s_bloom_end,
        s_env_begin, s_env_end, s_glint_draw, s_env_cube_begin, s_env_cube_face, s_env_cube_end, s_glass_draw,
        s_grade,
    };
}

/* ---- one frame through the wrapper ---------------------------------------- */
/* What a frame draws, each copy at its own address. */
struct Frame {
    std::vector<float> vertices = std::vector<float>(3 * 6);
    std::vector<uint8_t> texels = std::vector<uint8_t>(2 * 2 * 4);
};

Frame someFrame() {
    Frame frame;
    for (size_t i = 0; i < frame.vertices.size(); i++) frame.vertices[i] = 0.25f * (float)i - 1.0f;
    for (size_t i = 0; i < frame.texels.size(); i++) frame.texels[i] = (uint8_t)(17 * i + 3);
    return frame;
}

void resetStandIn() {
    std::memset(sCalls, 0, sizeof(sCalls));
    sNextTexture = 100, sNextFramebuffer = 0, sNextRetained = 0, sNextProgram = 0;
}

/* Wraps a fresh stand-in with GE_GFXDIGEST set (and NODRAW when asked). */
GfxRenderingAPI* wrap(GfxRenderingAPI* inner, bool nodraw, const char* detail = nullptr) {
    resetStandIn();
    auto put = [](const char* name, const char* value) { value ? setenv(name, value, 1) : unsetenv(name); };
    put("GE_GFXDIGEST", sPath.c_str());
    put("GE_GFXDIGEST_NODRAW", nodraw ? "1" : nullptr);
    put("GE_GFXDIGEST_DETAIL", detail);
    return portGfxDigestWrap(inner);
}

/* One frame drawing FRAME, with a new texture unless TEXTURE names an old one. */
void drawFrame(GfxRenderingAPI* api, Frame& frame, uint32_t texture = 0) {
    api->start_frame();
    api->start_draw_to_framebuffer(0, 1.0f);
    api->clear_framebuffer(true, true);
    ShaderProgram* prg = api->lookup_shader(0x1122334455667788ull, 9);
    if (prg == nullptr) {
        prg = api->create_and_load_new_shader(0x1122334455667788ull, 9);
    }
    uint8_t inputs = 0;
    bool used[2] = {};
    api->shader_get_info(prg, &inputs, used);
    api->load_shader(prg);
    texture = texture != 0 ? texture : api->new_texture();
    api->select_texture(0, texture, true);
    api->upload_texture(frame.texels.data(), 2, 2);
    api->set_sampler_parameters(0, true, 0, 0);
    api->set_depth_mode(true, true, true, false, 0);
    api->set_viewport(0, 0, 640, 480);
    api->set_scissor(0, 0, 640, 480);
    api->set_use_alpha(false, false);
    api->draw_triangles(frame.vertices.data(), frame.vertices.size(), 1);
    api->end_frame();
}

std::string readDigest() {
    std::ifstream in(sPath);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/* The line one frame writes, drawing or not. */
std::string lineOf(Frame frame, bool nodraw = false) {
    GfxRenderingAPI inner = standIn();
    GfxRenderingAPI* api = wrap(&inner, nodraw);
    drawFrame(api, frame);
    return readDigest();
}

/* FIELD's value (`vtx`, `tex`, ...) in LINE. */
std::string field(const std::string& line, const char* name) {
    const std::string key = std::string(" ") + name + "=";
    const size_t at = line.find(key);
    if (at == std::string::npos) return "";
    const size_t from = at + key.size();
    return line.substr(from, line.find_first_of(" \n", from) - from);
}

/* Whether A and B differ in exactly the field ONLY. */
bool differsIn(const std::string& a, const std::string& b, const char* only) {
    static const char* const kFields[] = { "n", "tick", "draws", "tris", "vtx", "tex", "state", "post" };
    bool exactly = true;
    for (const char* name : kFields) exactly &= (field(a, name) == field(b, name)) == (std::strcmp(name, only) != 0);
    return exactly;
}

/* ---- the checks ------------------------------------------------------------ */
void same_calls_same_line() {
    // Arrange
    g_GlobalTimer = 42;

    // Act
    const std::string a = lineOf(someFrame());
    const std::string b = lineOf(someFrame());

    // Assert
    expect(a == b, "same_calls_same_line: two runs of one frame wrote different lines");
    expect(a.rfind("F n=1 tick=42 draws=1 tris=1 vtx=", 0) == 0 && a.find(" tex=1:") != std::string::npos &&
               a.find(" state=") != std::string::npos && a.find(" post=") != std::string::npos &&
               a.back() == '\n' && a.find('\n') == a.size() - 1,
           "same_calls_same_line: the line is not F n=1 tick=42 draws=1 tris=1 vtx=.. tex=1:.. state=.. post=..");
}

void one_vertex_bit_changes_vtx_only() {
    // Arrange
    Frame changed = someFrame();
    uint32_t bits;
    std::memcpy(&bits, &changed.vertices[7], sizeof(bits));
    bits ^= 1u;
    std::memcpy(&changed.vertices[7], &bits, sizeof(bits));

    // Act
    const std::string a = lineOf(someFrame());
    const std::string b = lineOf(changed);

    // Assert
    expect(differsIn(a, b, "vtx"), "one_vertex_bit_changes_vtx_only: a vertex bit changed more than vtx");
}

void one_texel_changes_tex_only() {
    // Arrange
    Frame changed = someFrame();
    changed.texels[5] ^= 0x10;

    // Act
    const std::string a = lineOf(someFrame());
    const std::string b = lineOf(changed);

    // Assert
    expect(differsIn(a, b, "tex"), "one_texel_changes_tex_only: a texel changed more than tex");
}

void same_bytes_at_two_addresses_same_digest() {
    // Arrange: the same values in buffers at other addresses, and a shader
    // program and texture id the backend hands out at other values
    Frame first = someFrame();
    Frame second = someFrame();
    second.vertices.reserve(4096);
    second.texels.reserve(4096);
    GfxRenderingAPI inner = standIn();

    // Act
    GfxRenderingAPI* api = wrap(&inner, false);
    drawFrame(api, first);
    const std::string a = readDigest();
    api = wrap(&inner, false);
    sNextProgram = 5;
    sNextTexture = 900;
    drawFrame(api, second);
    const std::string b = readDigest();

    // Assert
    expect(first.vertices.data() != second.vertices.data(), "same_bytes_at_two_addresses_same_digest: one address");
    expect(a == b, "same_bytes_at_two_addresses_same_digest: other addresses or handles changed the line");
}

/* The texture cache makes textures as frames come, so a paced run makes a
 * different number before a recording plays (dam1, Task 7): a texture is named
 * by what was uploaded into it, never by when it was made. */
void texture_named_by_contents_not_by_creation_order() {
    // Arrange
    GfxRenderingAPI inner = standIn();
    Frame frame = someFrame();

    // Act
    GfxRenderingAPI* api = wrap(&inner, false);
    drawFrame(api, frame);
    drawFrame(api, frame);
    const std::string a = readDigest();
    api = wrap(&inner, false);
    api->new_texture(); /* one texture more in the first frame */
    drawFrame(api, frame);
    drawFrame(api, frame);
    const std::string b = readDigest();

    // Assert
    expect(a.substr(a.find('\n') + 1) == b.substr(b.find('\n') + 1),
           "texture names: a texture made earlier changed a later frame's line");
}

/* The cache's free-id pool hands an old texture id out again without
 * new_texture: the draw is named by what is uploaded into it now. */
void recycled_texture_id_named_by_its_new_contents() {
    // Arrange
    GfxRenderingAPI inner = standIn();
    Frame first = someFrame();
    Frame second = someFrame();
    second.texels[0] ^= 0x01;

    // Act
    GfxRenderingAPI* api = wrap(&inner, false);
    drawFrame(api, first);
    drawFrame(api, second);
    const std::string fresh = readDigest();
    api = wrap(&inner, false);
    const uint32_t reused = api->new_texture();
    drawFrame(api, first, reused);
    drawFrame(api, second, reused);
    const std::string recycled = readDigest();

    // Assert
    expect(fresh.substr(fresh.find('\n') + 1) == recycled.substr(recycled.find('\n') + 1),
           "recycled texture id: a reused id's old contents changed the frame that refills it");
}

void nodraw_skips_draw_and_upload_and_forwards_shader_info() {
    // Arrange
    const std::string drawn = lineOf(someFrame());

    // Act
    const std::string skipped = lineOf(someFrame(), true);

    // Assert
    expect(drawn == skipped, "nodraw: drawing and not drawing wrote different lines");
    expect(sCalls[E_draw_triangles] == 0 && sCalls[E_upload_texture] == 0 && sCalls[E_clear_framebuffer] == 0 &&
               sCalls[E_set_viewport] == 0 && sCalls[E_load_shader] == 0,
           "nodraw: a draw, upload, clear, viewport or shader bind reached the inner table");
    expect(sCalls[E_create_and_load_new_shader] == 1 && sCalls[E_lookup_shader] == 1 &&
               sCalls[E_shader_get_info] == 1 && sCalls[E_new_texture] == 1 &&
               sCalls[E_start_draw_to_framebuffer] == 1 && sCalls[E_end_frame] == 1,
           "nodraw: shader creation, lookup or info, a texture id or the frame was not forwarded");
}

void null_entries_stay_null() {
    // Arrange
    GfxRenderingAPI inner = standIn();
    inner.glint_draw = nullptr;
    inner.retained_create = nullptr;
    inner.grade = nullptr;

    // Act
    GfxRenderingAPI* api = wrap(&inner, false);

    // Assert
    expect(api->glint_draw == nullptr && api->retained_create == nullptr && api->grade == nullptr,
           "null_entries_stay_null: an entry the inner table leaves NULL is set");
    expect(api->glass_draw != nullptr && api->retained_build != nullptr, "null_entries_stay_null: a set entry is NULL");
}

/* The wrapper's own static assertion fails the build when the table gains an
 * entry its list lacks; this repeats it and checks that every entry is the
 * wrapper's, not the inner table's. */
static_assert(sizeof(GfxRenderingAPI) == kGfxDigestEntries * sizeof(void (*)(void)),
              "port_gfx_digest.cpp's entry list does not cover struct GfxRenderingAPI");

void every_entry_of_the_table_is_wrapped() {
    // Arrange
    GfxRenderingAPI inner = standIn();
    void (*innerSlots[kGfxDigestEntries])(void);
    void (*wrappedSlots[kGfxDigestEntries])(void);

    // Act
    GfxRenderingAPI* api = wrap(&inner, false);
    std::memcpy(innerSlots, &inner, sizeof(inner));
    std::memcpy(wrappedSlots, api, sizeof(*api));

    // Assert
    for (int i = 0; i < kGfxDigestEntries; i++) {
        const bool wrapped = wrappedSlots[i] != nullptr && wrappedSlots[i] != innerSlots[i];
        expect(wrapped, (std::string("every_entry_of_the_table_is_wrapped: ") + kGfxDigestEntryNames[i]).c_str());
    }
}

/* A retained buffer built from POSITIONS and drawn once; MODIFY runs between
 * the build and the draw. */
std::string retainedLine(std::vector<float> positions, void (*modify)(std::vector<float>&)) {
    GfxRenderingAPI inner = standIn();
    GfxRenderingAPI* api = wrap(&inner, false);
    api->start_frame();
    ShaderProgram* prg = api->create_and_load_new_shader(7, 1);
    const int buffer = api->retained_create(3);
    std::vector<float> stream(3 * 4, 0.5f);
    const uint32_t textures[2] = { api->new_texture(), 0 };
    const bool used[2] = { true, false };
    const uint8_t kinds[8] = { 7, 0, 0, 0, 0, 0, 0, 0 };
    const int desc = api->retained_build(buffer, 0, prg, stream.data(), 4, 1, positions.data(), nullptr, textures,
                                         used, kinds, 0);
    modify(positions);
    modify(stream);
    const float mvp[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }, fixup[4] = { 1, 0, 1, 0 };
    const float fog[4] = {}, rgb[3] = {};
    api->retained_draw(desc, mvp, fixup, fog, fog, rgb, 0, nullptr, nullptr, nullptr);
    api->end_frame();
    return readDigest();
}

void retained_draw_hashes_the_built_contents() {
    // Arrange
    const std::vector<float> positions = { 1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0 };
    std::vector<float> other = positions;
    other[4] = 4.5f;

    // Act
    const std::string built = retainedLine(positions, [](std::vector<float>&) {});
    const std::string overwritten = retainedLine(positions, [](std::vector<float>& v) { v.assign(v.size(), 0.0f); });
    const std::string changed = retainedLine(other, [](std::vector<float>&) {});

    // Assert
    expect(built == overwritten, "retained_draw: memory changed after the build changed the draw's digest");
    expect(differsIn(built, changed, "vtx"), "retained_draw: other built contents did not change vtx alone");
    expect(field(built, "draws") == "1" && field(built, "tris") == "1", "retained_draw: not counted as 1 draw, 1 tri");
}

void demo_frames_end_with_the_recording_tick() {
    // Arrange
    sReplayTick = 17;

    // Act
    const std::string line = lineOf(someFrame());
    sReplayTick = -1;
    const std::string plain = lineOf(someFrame());

    // Assert
    expect(line.size() > 9 && line.compare(line.size() - 9, 9, " demo=17\n") == 0,
           "demo frames: a frame during a replay does not end with demo=K");
    expect(line.substr(0, line.size() - 9) + "\n" == plain, "demo frames: demo=K changed the rest of the line");
    expect(plain.find("demo=") == std::string::npos, "demo frames: a frame outside a replay has demo=");
}

void detail_writes_a_line_a_call_in_its_frames() {
    // Arrange
    GfxRenderingAPI inner = standIn();
    Frame frame = someFrame();

    // Act
    GfxRenderingAPI* api = wrap(&inner, false, "2-2");
    drawFrame(api, frame);
    drawFrame(api, frame);
    drawFrame(api, frame);
    const std::string text = readDigest();

    // Assert
    expect(text.rfind("F n=1 ", 0) == 0, "detail: frame 1 has a call line");
    expect(text.find("C n=2 k=1 entry=start_frame part=state hash=") != std::string::npos &&
               text.find(" entry=draw_triangles part=vtx hash=") != std::string::npos,
           "detail: frame 2's calls are not each on a C line");
    expect(text.find("C n=1 ") == std::string::npos && text.find("C n=3 ") == std::string::npos,
           "detail: a frame outside 2-2 has call lines");
}

void without_the_variable_the_table_is_the_inner_one() {
    // Arrange
    GfxRenderingAPI inner = standIn();
    unsetenv("GE_GFXDIGEST");

    // Act
    GfxRenderingAPI* api = portGfxDigestWrap(&inner);

    // Assert
    expect(api == &inner, "without GE_GFXDIGEST the wrapper replaced the table");
}

}  // namespace

int main() {
    char path[] = "/tmp/midas-gfx-digest-XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) return std::perror("test_gfx_digest: mkstemp"), 1;
    close(fd);
    sPath = path;
    same_calls_same_line();
    one_vertex_bit_changes_vtx_only();
    one_texel_changes_tex_only();
    same_bytes_at_two_addresses_same_digest();
    texture_named_by_contents_not_by_creation_order();
    recycled_texture_id_named_by_its_new_contents();
    nodraw_skips_draw_and_upload_and_forwards_shader_info();
    null_entries_stay_null();
    every_entry_of_the_table_is_wrapped();
    retained_draw_hashes_the_built_contents();
    demo_frames_end_with_the_recording_tick();
    detail_writes_a_line_a_call_in_its_frames();
    without_the_variable_the_table_is_the_inner_one();
    std::remove(path);
    if (failures != 0) return 1;
    std::printf("test_gfx_digest (%d-bit): PASS\n", (int)(sizeof(void*) * 8));
    return 0;
}
