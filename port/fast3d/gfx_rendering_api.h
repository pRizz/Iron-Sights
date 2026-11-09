#ifndef GFX_RENDERING_API_H
#define GFX_RENDERING_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct ShaderProgram;

struct GfxClipParameters {
    bool z_is_from_0_to_1;
    bool invert_y;
};

enum FilteringMode { FILTER_NONE, FILTER_LINEAR, FILTER_THREE_POINT };

struct GfxRenderingAPI {
    const char* (*get_name)(void);
    int (*get_max_texture_size)(void);
    struct GfxClipParameters (*get_clip_parameters)(void);
    void (*unload_shader)(struct ShaderProgram* old_prg);
    void (*load_shader)(struct ShaderProgram* new_prg);
    struct ShaderProgram* (*create_and_load_new_shader)(uint64_t shader_id0, uint32_t shader_id1);
    struct ShaderProgram* (*lookup_shader)(uint64_t shader_id0, uint32_t shader_id1);
    void (*shader_get_info)(struct ShaderProgram* prg, uint8_t* num_inputs, bool used_textures[2]);
    void (*clear_shaders)(void);
    uint32_t (*new_texture)(void);
    void (*select_texture)(int tile, uint32_t texture_id, bool linear_filter);
    void (*upload_texture)(const uint8_t* rgba32_buf, uint32_t width, uint32_t height);
    void (*set_sampler_parameters)(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt);
    void (*set_depth_mode)(bool depth_test, bool depth_update, bool depth_compare, bool depth_source_prim, uint16_t zmode);
    void (*set_depth_range)(float znear, float zfar);
    void (*set_viewport)(int x, int y, int width, int height);
    void (*set_scissor)(int x, int y, int width, int height);
    void (*set_use_alpha)(bool use_alpha, bool modulate);
    void (*draw_triangles)(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris);
    void (*init)(void);
    void (*on_resize)(void);
    void (*start_frame)(void);
    void (*end_frame)(void);
    void (*finish_render)(void);
    int (*create_framebuffer)();
    void (*update_framebuffer_parameters)(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                          bool opengl_invert_y, bool render_target, bool has_depth_buffer,
                                          bool can_extract_depth);
    bool (*start_draw_to_framebuffer)(int fb_id, float noise_scale);
    void (*copy_framebuffer)(int fb_dst, int fb_src, int left, int top, bool flip_y, bool use_back);
    void (*clear_framebuffer)(bool clear_color, bool clear_depth);
    void (*resolve_msaa_color_buffer)(int fb_id_target, int fb_id_source);
    void* (*get_framebuffer_texture_id)(int fb_id);
    void (*select_texture_fb)(int fb_id);
    void (*delete_texture)(uint32_t texID);
    void (*set_texture_filter)(enum FilteringMode mode);
    enum FilteringMode (*get_texture_filter)(void);
    /* Retained (GPU-resident, object-space) geometry for the room cache's GPU
     * mode (gfx_roomcache.inl). NULL on backends without it. retained_create:
     * allocate a vertex buffer of vertex_count vertices (-1 on failure).
     * retained_build: convert one draw's captured fast3d float stream (buf_vbo
     * layout of prg, num_tris*3 vertices, model-space positions)
     *   into the buffer at first_vertex and return a draw descriptor, or -1.
     *   tex_id[t] are the texture ids bound for that draw (for texcoord
     *   scaling).
     * retained_draw: draw a descriptor with the current shader/textures/depth/blend state.
     *   mvp = 4 rows (row-vector convention), fixup = (x scale, x offset, z
     *   scale, z offset) in NDC, fogp = (mul, ofs, 0, flat alpha),
     *   fogsel = (gpu, flat, vertex) weights, fog_rgb = the frame's fog colour,
     *   cull_mode = the N64 G_CULL_* bits. */
    int (*retained_create)(uint32_t vertex_count);
    void (*retained_destroy)(int buffer);
    /* model_pos: 4 floats per vertex (x y z + matrix slot 0..15, see
     * retained_matrices); model_nrm: 3 floats per vertex (n/127) or NULL.
     * input_kind[j] per combiner input: 0 static, 1 prim, 2 env, 3 prim alpha
     * (as rgb), 4 env alpha (as rgb), 5 prim lod fraction (rgb), 6 prim lod
     * fraction (alpha), 7 shade (per vertex); shade_slot = input index of the
     * shade, or -1. */
    int (*retained_build)(int buffer, uint32_t first_vertex, struct ShaderProgram* prg, const float* buf_vbo,
                          size_t stride, size_t num_tris, const float* model_pos, const float* model_nrm,
                          const uint32_t tex_id[2], const bool tex_used[2], const uint8_t input_kind[8],
                          int shade_slot);
    /* lights: 20 floats = ambient rgb + pad, then (dir xyz in model space,
     * normalised, + pad; colour rgb + pad) for light0 and light1; NULL = unlit
     * (captured vertex colours). prim_env: 12 floats = prim rgba, env rgba,
     * prim lod fraction, 0, 0, 0, patched into the draw's constant inputs.
     * decal: 4 floats (b*k, k, 4k, 0) for gfx_sp_tri1's ZMODE_DEC pull (z +=
     * b*k/(w-k) for w > 4k), or NULL. */
    void (*retained_draw)(int desc, const float mvp[16], const float fixup[4], const float fogp[4],
                          const float fogsel[4], const float fog_rgb[3], uint32_t cull_mode,
                          const float* lights, const float* prim_env, const float* decal);
    /* the extra matrices a vertex can select (slot 1..15; slot 0 is the mvp
     * of retained_draw): count matrices of 16 floats (rows) into slots
     * first_slot.. - a model part list's per-limb matrices. */
    void (*retained_matrices)(int first_slot, int count, const float* mats);
    /* retained_draw by an index list (16-bit, absolute into the descriptor's
     * buffer; n a multiple of 3) - the deferred-XLU sorter's replay of
     * retained triangles in sorted order. */
    void (*retained_draw_indexed)(int desc, const uint16_t* idx, uint32_t n, const float mvp[16], const float fixup[4],
                                  const float fogp[4], const float fogsel[4], const float fog_rgb[3], uint32_t cull_mode,
                                  const float* lights, const float* prim_env, const float* decal);
    /* GE N64 AA: resolve the multisampled framebuffer fb_src into fb_dst the way
     * the N64's Video Interface filtered its frame - see gfx_opengl.cpp. NULL on
     * backends without it (gfx_pc falls back to the plain resolve). */
    void (*resolve_n64_aa)(int fb_dst, int fb_src);
    /* FORK-058 shiny weapon bloom (gfx_bloom.inl). bloom_begin: clear the bloom
     * buffer and mirror every following draw_triangles into it - batches marked
     * shiny in colour, the rest as black, with the same depth state, so it holds
     * only the visible reflective pixels. bloom_mark: whether the next draws are
     * shiny. bloom_end: stop mirroring, then add the blurred bright part
     * (soft-thresholded luma above threshold, 0..1) times strength to the
     * current target; radius in pixels per blur pass, passes >= 1. NULL on
     * backends without it. */
    void (*bloom_begin)(void);
    void (*bloom_mark)(bool shiny);
    void (*bloom_end)(float threshold, float strength, float radius, int passes);
    /* FORK-058, screen reflections (gfx_screen_env.inl). env_begin:
     * copy the current target (the scene so far) and, for the following draws
     * that bloom_mark calls shiny, replace the reflection texture with a sphere
     * map sampled from it: mix 0..1 against the authored texture, gain on the
     * scene, back = shade for directions behind the camera, fov_y in degrees,
     * blur = extra mip blur factor (1 = none), flip = sample the copy upside
     * down, smooth = share of the new frame blended into the previous map
     * (1 = rebuild each time), key = the weapon (a new key starts fresh),
     * source 1 = sample the level cube (env_cube_*) instead of the screen,
 * hue 0..1 = how much of the scene's own colour tints the metal (0 = light
 * and dark only), mirror 0..1 = the share of the scene itself in the
 * metal's average colour rather than as light and dark on its pattern.
     * env_end: back to the authored texture. NULL on backends without it. */
    void (*env_begin)(float mix, float gain, float back, float fov_y, float blur, int flip, float smooth, int key,
                      int source, float hue, float mirror);
    void (*env_end)(void);
    /* FORK-058, Gun Glint (gfx_glint.inl): draw triangles additively
     * over the current target, depth tested (LEQUAL) but not written, lit per
     * pixel (Blinn-Phong). buf: 15 floats per vertex - clip x, y, z, w exactly
     * as the batch emitted them, camera-space position, face normal, light
     * direction, strength, power. NULL on backends without it. */
    void (*glint_draw)(const float* buf, size_t vertex_count);
    /* FORK-058 prototype, the level cube (gfx_envcube.inl): env_cube_begin binds
     * and clears (to the sky colour) a 3x2 atlas of size x size faces, saving
     * the target; env_cube_face confines the next draws to face 0..5 (call it
     * after any viewport change); env_cube_end builds its mips and restores the
     * target, writing the atlas to dump (a BMP path) when set. */
    void (*env_cube_begin)(int size, float r, float g, float b);
    void (*env_cube_face)(int face);
    void (*env_cube_end)(const char* dump);
    /* FORK-066, Glass Reflections (gfx_glass.inl): draw glass triangles again
     * additively over the current target, depth tested (LEQUAL) but not
     * written: the level cube (env_cube_*, when cube is true) reflected off
     * the face, weighted by a Fresnel term, plus an overhead highlight.
     * buf: 18 floats per vertex - clip x, y, z, w exactly as the batch emitted
     * them, camera-space position, face normal, light direction, reflection
     * strength, head-on reflectance, highlight strength and power, the cube's
     * mip level. split: only the right half of the viewport (a review aid).
     * NULL on backends without it. */
    void (*glass_draw)(const float* buf, size_t vertex_count, bool cube, bool split);
};

#endif
