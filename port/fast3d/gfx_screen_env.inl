/* FORK-058 (shiny reflections candidate 3a): a reflection texture
 * built from the screen. Included by gfx_opengl.cpp after gfx_bloom.inl.
 *
 * The shiny weapon draws after the world and before the HUD, so when its span
 * opens (gfx_opengl_env_begin) the target already holds the scene: it is
 * copied (resolving MSAA) into a mipmapped texture. At the span's first shiny
 * batch, a warp pass builds a sphere-map texture in the game's own G_TEXTURE_GEN
 * layout: texel (s, t) is the eye-space normal (2s - 1, 2t - 1, nz) along the
 * camera's right and up axes (gSPLookAt). Its reflection of the view ray,
 * mirrored forward when it points behind the camera (and darkened by `back`),
 * is projected onto the screen copy and sampled at a blurred mip level, then
 * multiplied into the batch's authored reflection texture relative to the
 * screen's average brightness (the metal keeps its colour; the scene shows as
 * light, dark and hue on it), mixed by `screen`. Shiny batches draw with
 * that texture on both texture units; nothing else in the pipeline changes. */
#include <cmath>

static bool gl_env_active = false, gl_env_built = false, gl_env_failed = false;
static GLuint gl_env_fbo = 0, gl_env_copy = 0, gl_env_copy_fbo = 0, gl_env_map = 0;
static uint32_t gl_env_w = 0, gl_env_h = 0, gl_env_levels = 1;
static GLuint gl_env_prog = 0, gl_env_vao = 0, gl_env_vbo = 0;
static GLint gl_env_u_screen = -1, gl_env_u_authored = -1, gl_env_u_lod = -1, gl_env_u_top = -1, gl_env_u_mix = -1,
             gl_env_u_gain = -1,
             gl_env_u_back = -1, gl_env_u_tan = -1, gl_env_u_flip = -1;
static float gl_env_mix, gl_env_gain, gl_env_back, gl_env_tan[2], gl_env_blur, gl_env_smooth;
static int gl_env_flip;
/* what the sphere map last held: a new frame blends into it (temporal
 * smoothing) unless the weapon or the target size changed */
static int gl_env_key = -1, gl_env_built_key = -1;
static uint32_t gl_env_built_w = 0, gl_env_built_h = 0;
static const uint32_t kEnvSize = 64; /* the sphere map; the game's own are 32 or 64 */

static bool gl_env_program(void) {
    if (gl_env_prog != 0) return true;
    if (gl_env_failed) return false;
    if (gl_glsl_version < 130 || glGenVertexArrays == NULL || glGenerateMipmap == NULL) {
        sysLogPrintf(LOG_WARNING, "shiny screen reflections: need GLSL 1.30, vertex arrays and mipmaps - off");
        gl_env_failed = true;
        return false;
    }
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n%s", gl_glsl_version_str, gl_es ? "precision mediump float;\n" : "");
    const std::string vs = std::string(hdr) + "in vec2 aPos;\nout vec2 vUv;\n"
                           "void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    const std::string fs = std::string(hdr) +
        "uniform sampler2D uScreen;\nuniform sampler2D uAuthored;\n"
        "uniform float uLod, uTop, uMix, uGain, uBack;\nuniform vec2 uTan;\nuniform int uFlip;\n"
        "in vec2 vUv;\nout vec4 outColor;\n"
        "void main() {\n"
        "    vec2 n2 = vUv * 2.0 - 1.0;\n"
        "    float r2 = dot(n2, n2);\n"
        "    vec3 n = r2 < 1.0 ? vec3(n2, sqrt(1.0 - r2)) : vec3(n2 * inversesqrt(r2), 0.0);\n"
        "    vec3 r = reflect(vec3(0.0, 0.0, -1.0), n);\n"   /* eye space: the camera looks down -z */
        /* behind the camera: mirror it forward, fading the shade across the
         * seam (a hard step swept across the metal as the gun moved) */
        "    float shade = mix(1.0, uBack, smoothstep(-0.25, 0.25, r.z));\n"
        "    r.z = -max(abs(r.z), 0.05);\n"
        "    vec2 s = clamp(r.xy / (-r.z) / uTan, -1.0, 1.0) * 0.5 + 0.5;\n"
        "    if (uFlip != 0) s.y = 1.0 - s.y;\n"
        /* the scene relative to the screen's average brightness (its 1x1 mip),
         * so the metal keeps its own colour, detail and level-to-level
         * brightness and the surroundings show as light, dark and hue on it */
        "    vec3 scene = textureLod(uScreen, s, uLod).rgb * shade;\n"
        "    float avg = max(dot(textureLod(uScreen, vec2(0.5), uTop).rgb, vec3(0.299, 0.587, 0.114)), 0.05);\n"
        "    vec3 rel = min(scene / avg * uGain, vec3(3.0));\n"
        "    vec4 authored = texture(uAuthored, vUv);\n"
        "    outColor = vec4(mix(authored.rgb, min(authored.rgb * rel, vec3(1.0)), uMix), authored.a);\n"
        "}\n";
    gl_env_prog = vi_compile(vs.c_str(), fs.c_str());
    if (gl_env_prog == 0) {
        gl_env_failed = true;
        return false;
    }
    gl_env_u_screen = glGetUniformLocation(gl_env_prog, "uScreen");
    gl_env_u_authored = glGetUniformLocation(gl_env_prog, "uAuthored");
    gl_env_u_lod = glGetUniformLocation(gl_env_prog, "uLod");
    gl_env_u_top = glGetUniformLocation(gl_env_prog, "uTop");
    gl_env_u_mix = glGetUniformLocation(gl_env_prog, "uMix");
    gl_env_u_gain = glGetUniformLocation(gl_env_prog, "uGain");
    gl_env_u_back = glGetUniformLocation(gl_env_prog, "uBack");
    gl_env_u_tan = glGetUniformLocation(gl_env_prog, "uTan");
    gl_env_u_flip = glGetUniformLocation(gl_env_prog, "uFlip");
    static const float quad[] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f };
    glGenVertexArrays(1, &gl_env_vao);
    glGenBuffers(1, &gl_env_vbo);
    glBindVertexArray(gl_env_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_env_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    const GLint loc = glGetAttribLocation(gl_env_prog, "aPos");
    if (loc >= 0) {
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }
    return true;
}

/* the screen copy (mipmapped, at the target's size) and the sphere map */
static bool gl_env_buffers(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return false;
    if (gl_env_copy != 0 && w == gl_env_w && h == gl_env_h) return true;
    if (gl_env_copy != 0) {
        glDeleteFramebuffers(1, &gl_env_copy_fbo);
        glDeleteTextures(1, &gl_env_copy);
    }
    gl_env_w = w;
    gl_env_h = h;
    gl_env_levels = 1;
    while ((w >> gl_env_levels) > 0 || (h >> gl_env_levels) > 0) gl_env_levels++;
    glGenTextures(1, &gl_env_copy);
    glBindTexture(GL_TEXTURE_2D, gl_env_copy);
    for (uint32_t l = 0; l < gl_env_levels; l++) {
        const GLsizei lw = (GLsizei)((w >> l) ? (w >> l) : 1), lh = (GLsizei)((h >> l) ? (h >> l) : 1);
        glTexImage2D(GL_TEXTURE_2D, (GLint)l, GL_RGBA8, lw, lh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &gl_env_copy_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_env_copy_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_env_copy, 0);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (gl_env_map == 0) {
        gl_env_map = gl_bloom_texture(kEnvSize, kEnvSize);
        glGenFramebuffers(1, &gl_env_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, gl_env_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_env_map, 0);
        ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    if (!ok) {
        sysLogPrintf(LOG_WARNING, "shiny screen reflections: incomplete framebuffer - off");
        gl_env_failed = true;
    }
    return ok;
}

/* blur: the mip level whose width is about 1/blur of the sphere map's */
static void gfx_opengl_env_begin(float mix, float gain, float back, float fov_y, float blur, int flip, float smooth,
                                 int key) {
    gl_env_active = gl_env_built = false;
    if (gl_env_failed || mix <= 0.0f || current_framebuffer >= framebuffers.size()) return;
    const Framebuffer& fb = framebuffers[current_framebuffer];
    GLint tex, vao, vbo, read_fbo, draw_fbo;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    const bool ok = gl_env_program() && gl_env_buffers(fb.width, fb.height);
    if (ok) {
        GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)draw_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl_env_copy_fbo);
        glBlitFramebuffer(0, 0, fb.width, fb.height, 0, 0, fb.width, fb.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, gl_env_copy);
        glGenerateMipmap(GL_TEXTURE_2D);
        if (scissor) glEnable(GL_SCISSOR_TEST);
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fbo);
    if (!ok) return;
    const float aspect = (float)fb.width / (float)fb.height;
    gl_env_tan[1] = std::tan(fov_y * 0.5f * 3.14159265f / 180.0f);
    gl_env_tan[0] = gl_env_tan[1] * aspect;
    gl_env_mix = mix;
    gl_env_gain = gain;
    gl_env_back = back;
    gl_env_flip = flip;
    gl_env_smooth = smooth;
    gl_env_key = key;
    gl_env_blur = std::log2((float)fb.width / (float)kEnvSize) + (blur > 0.0f ? std::log2(blur) : 0.0f);
    gl_env_active = true;
}

static void gfx_opengl_env_end(void) {
    gl_env_active = gl_env_built = false;
}

/* the sphere map from the screen copy and the batch's authored texture (unit 0) */
static void gl_env_build(void) {
    GLint viewport[4], active, tex0, prog, vao, vbo, draw_fbo;
    GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull = glIsEnabled(GL_CULL_FACE), stencil = glIsEnabled(GL_STENCIL_TEST), cmask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);

    GLint src_rgb, dst_rgb, src_a, dst_a, eq_rgb, eq_a;
    GLfloat blend_color[4];
    glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_a);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_a);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eq_a);
    glGetFloatv(GL_BLEND_COLOR, blend_color);
    /* temporal smoothing: the new frame is blended into what the map holds,
     * unless this is a different weapon or target (then it starts fresh).
     * Rebuilt from scratch every frame, the map flickered as the player
     * moved: the screen-average normalisation jumped when bright areas came
     * into view, and fine scene detail popped between its texels. */
    const bool blend_in = gl_env_smooth < 1.0f && gl_env_built_key == gl_env_key && gl_env_built_w == gl_env_w
                          && gl_env_built_h == gl_env_h;
    if (blend_in) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendColor(0.f, 0.f, 0.f, gl_env_smooth);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    gl_env_built_key = gl_env_key;
    gl_env_built_w = gl_env_w;
    gl_env_built_h = gl_env_h;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_env_fbo);
    glViewport(0, 0, kEnvSize, kEnvSize);
    glUseProgram(gl_env_prog);
    glBindVertexArray(gl_env_vao);
    /* unit 0 keeps the authored texture; the screen copy goes on unit 2 */
    glActiveTexture(GL_TEXTURE2);
    GLint tex2;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex2);
    glBindTexture(GL_TEXTURE_2D, gl_env_copy);
    glUniform1i(gl_env_u_screen, 2);
    glUniform1i(gl_env_u_authored, 0);
    glUniform1f(gl_env_u_lod, gl_env_blur);
    glUniform1f(gl_env_u_top, (float)(gl_env_levels - 1));
    glUniform1f(gl_env_u_mix, gl_env_mix);
    glUniform1f(gl_env_u_gain, gl_env_gain);
    glUniform1f(gl_env_u_back, gl_env_back);
    glUniform2f(gl_env_u_tan, gl_env_tan[0], gl_env_tan[1]);
    glUniform1i(gl_env_u_flip, gl_env_flip);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindTexture(GL_TEXTURE_2D, tex2);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    glActiveTexture((GLenum)active);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)draw_fbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUseProgram(prog);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
    glBlendEquationSeparate(eq_rgb, eq_a);
    glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
    glBlendColor(blend_color[0], blend_color[1], blend_color[2], blend_color[3]);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE);
    if (stencil) glEnable(GL_STENCIL_TEST);
    gl_env_built = true;
}

/* gfx_opengl_draw_triangles, before a shiny batch: the sphere map on units 0
 * and 1 in place of the authored texture; returns whether to restore after */
static bool gl_env_bind(void) {
    if (!gl_env_active || !gl_bloom_shiny) return false;
    if (!gl_env_built) gl_env_build();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_env_map);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gl_env_map);
    return true;
}

static void gl_env_unbind(void) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, current_texture_ids[1]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, current_texture_ids[0]);
    glActiveTexture(GL_TEXTURE0 + last_selected_tile); /* later sampler and upload calls act on it */
}
