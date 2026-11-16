/**
 * gfx_gl_shadow.inl - answers GL state queries from memory (FORK-088).
 *
 * The fork's passes (glint, glass, bloom, grade, HDR, screen reflections) save
 * the GL state they change by asking the driver for it and put it back when
 * they are done. A query is the one kind of GL call that cannot be queued: the
 * driver has to finish everything before it can answer, and in a browser the
 * question crosses into JavaScript and waits on the GPU process. Profiling put
 * 6.5 ms of a 10.1 ms browser frame in those queries (FORK-087).
 *
 * This file sits between the renderer and glad's function pointers. Every call
 * that sets a piece of state the passes later ask about is recorded on its way
 * to the driver, and glGetIntegerv / glGetBooleanv / glGetFloatv / glIsEnabled
 * answer from that record. A value nobody has set yet is asked of the driver
 * once and remembered. Deleting an object forgets whatever it might have been
 * bound to. Anything this file does not track goes to the driver as before, so
 * the passes need no change and a query it does not know stays correct.
 *
 * Installed by gfx_gl_shadow_install() after glad loads, on every context.
 *   GE_GLSHADOW=0        leave glad's pointers alone (the old behaviour)
 *   GE_GLSHADOW_CHECK=1  also ask the driver each time and report differences
 */

#include <cstdlib>
#include <cstring>

#define GL_SHADOW_UNITS 32
/* Sizes and colours inside these bounds are stored by the driver as given;
 * outside them it may clamp or refuse, so the value is asked for instead. */
#define GL_SHADOW_MAX_SIZE 8192

/* One remembered value: up to four numbers, and whether they are known. */
struct GlShadowValue {
    bool known = false;
    GLint i[4] = { 0, 0, 0, 0 };
    GLfloat f[4] = { 0, 0, 0, 0 };
};

static struct GlShadowState {
    GlShadowValue program, vao, array_buffer, renderbuffer, draw_fbo, read_fbo, active_texture;
    GlShadowValue texture_2d[GL_SHADOW_UNITS], sampler[GL_SHADOW_UNITS];
    GlShadowValue blend_src_rgb, blend_dst_rgb, blend_src_a, blend_dst_a, blend_eq_rgb, blend_eq_a, blend_color;
    GlShadowValue depth_func, depth_mask, color_mask, clear_color, viewport, scissor_box;
    GlShadowValue cap_blend, cap_scissor, cap_depth, cap_cull, cap_stencil;
    GlShadowValue max_texture_size, max_samples, max_color_texture_samples;
} gl_shadow;

static struct GlShadowReal {
    decltype(glad_glUseProgram) UseProgram;
    decltype(glad_glBindVertexArray) BindVertexArray;
    decltype(glad_glBindBuffer) BindBuffer;
    decltype(glad_glBindRenderbuffer) BindRenderbuffer;
    decltype(glad_glBindFramebuffer) BindFramebuffer;
    decltype(glad_glActiveTexture) ActiveTexture;
    decltype(glad_glBindTexture) BindTexture;
    decltype(glad_glBindSampler) BindSampler;
    decltype(glad_glBlendFunc) BlendFunc;
    decltype(glad_glBlendFuncSeparate) BlendFuncSeparate;
    decltype(glad_glBlendEquation) BlendEquation;
    decltype(glad_glBlendEquationSeparate) BlendEquationSeparate;
    decltype(glad_glBlendColor) BlendColor;
    decltype(glad_glDepthFunc) DepthFunc;
    decltype(glad_glDepthMask) DepthMask;
    decltype(glad_glColorMask) ColorMask;
    decltype(glad_glClearColor) ClearColor;
    decltype(glad_glViewport) Viewport;
    decltype(glad_glScissor) Scissor;
    decltype(glad_glEnable) Enable;
    decltype(glad_glDisable) Disable;
    decltype(glad_glDeleteBuffers) DeleteBuffers;
    decltype(glad_glDeleteTextures) DeleteTextures;
    decltype(glad_glDeleteFramebuffers) DeleteFramebuffers;
    decltype(glad_glDeleteRenderbuffers) DeleteRenderbuffers;
    decltype(glad_glDeleteVertexArrays) DeleteVertexArrays;
    decltype(glad_glDeleteSamplers) DeleteSamplers;
    decltype(glad_glDeleteProgram) DeleteProgram;
    decltype(glad_glGetIntegerv) GetIntegerv;
    decltype(glad_glGetBooleanv) GetBooleanv;
    decltype(glad_glGetFloatv) GetFloatv;
    decltype(glad_glIsEnabled) IsEnabled;
} gl_shadow_real;

static bool gl_shadow_check = false;
static unsigned gl_shadow_answers = 0, gl_shadow_asked = 0, gl_shadow_mismatches = 0;

static void gl_shadow_set(GlShadowValue& value, GLint a, GLint b = 0, GLint c = 0, GLint d = 0) {
    value.known = true;
    value.i[0] = a; value.i[1] = b; value.i[2] = c; value.i[3] = d;
}

static void gl_shadow_setf(GlShadowValue& value, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
    value.known = true;
    value.f[0] = a; value.f[1] = b; value.f[2] = c; value.f[3] = d;
}

static bool gl_shadow_unit_range(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    return r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f && a >= 0.0f && a <= 1.0f;
}

/* The unit glActiveTexture last chose, or -1 while that is not known. */
static int gl_shadow_unit(void) {
    if (!gl_shadow.active_texture.known) return -1;
    const int unit = gl_shadow.active_texture.i[0] - GL_TEXTURE0;
    return (unit >= 0 && unit < GL_SHADOW_UNITS) ? unit : -1;
}

static GlShadowValue* gl_shadow_cap(GLenum cap) {
    switch (cap) {
        case GL_BLEND: return &gl_shadow.cap_blend;
        case GL_SCISSOR_TEST: return &gl_shadow.cap_scissor;
        case GL_DEPTH_TEST: return &gl_shadow.cap_depth;
        case GL_CULL_FACE: return &gl_shadow.cap_cull;
        case GL_STENCIL_TEST: return &gl_shadow.cap_stencil;
        default: return nullptr;
    }
}

/* The remembered value an integer query names, and how many numbers it has. */
static GlShadowValue* gl_shadow_integer(GLenum pname, int* count) {
    const int unit = gl_shadow_unit();
    *count = 1;
    switch (pname) {
        case GL_CURRENT_PROGRAM: return &gl_shadow.program;
        case GL_VERTEX_ARRAY_BINDING: return &gl_shadow.vao;
        case GL_ARRAY_BUFFER_BINDING: return &gl_shadow.array_buffer;
        case GL_RENDERBUFFER_BINDING: return &gl_shadow.renderbuffer;
        case GL_DRAW_FRAMEBUFFER_BINDING: return &gl_shadow.draw_fbo; /* = GL_FRAMEBUFFER_BINDING */
        case GL_READ_FRAMEBUFFER_BINDING: return &gl_shadow.read_fbo;
        case GL_ACTIVE_TEXTURE: return &gl_shadow.active_texture;
        case GL_TEXTURE_BINDING_2D: return unit >= 0 ? &gl_shadow.texture_2d[unit] : nullptr;
        case GL_SAMPLER_BINDING: return unit >= 0 ? &gl_shadow.sampler[unit] : nullptr;
        case GL_BLEND_SRC_RGB: return &gl_shadow.blend_src_rgb;
        case GL_BLEND_DST_RGB: return &gl_shadow.blend_dst_rgb;
        case GL_BLEND_SRC_ALPHA: return &gl_shadow.blend_src_a;
        case GL_BLEND_DST_ALPHA: return &gl_shadow.blend_dst_a;
        case GL_BLEND_EQUATION_RGB: return &gl_shadow.blend_eq_rgb;
        case GL_BLEND_EQUATION_ALPHA: return &gl_shadow.blend_eq_a;
        case GL_DEPTH_FUNC: return &gl_shadow.depth_func;
        case GL_VIEWPORT: *count = 4; return &gl_shadow.viewport;
        case GL_SCISSOR_BOX: *count = 4; return &gl_shadow.scissor_box;
        case GL_MAX_TEXTURE_SIZE: return &gl_shadow.max_texture_size;
        case GL_MAX_SAMPLES: return &gl_shadow.max_samples;
        case GL_MAX_COLOR_TEXTURE_SAMPLES: return &gl_shadow.max_color_texture_samples;
        default: return nullptr;
    }
}

static void gl_shadow_report(GLenum pname) {
    if (++gl_shadow_mismatches <= 20) {
        printf("ge: glshadow: MISMATCH for 0x%04x: the remembered value differs from the driver's\n", (unsigned)pname);
        fflush(stdout);
    }
}

/* ---- setters: record, then pass on ---- */

static void APIENTRY gl_shadow_UseProgram(GLuint program) {
    gl_shadow_set(gl_shadow.program, (GLint)program);
    gl_shadow_real.UseProgram(program);
}

static void APIENTRY gl_shadow_BindVertexArray(GLuint array) {
    gl_shadow_set(gl_shadow.vao, (GLint)array);
    gl_shadow_real.BindVertexArray(array);
}

static void APIENTRY gl_shadow_BindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) gl_shadow_set(gl_shadow.array_buffer, (GLint)buffer);
    gl_shadow_real.BindBuffer(target, buffer);
}

static void APIENTRY gl_shadow_BindRenderbuffer(GLenum target, GLuint renderbuffer) {
    if (target == GL_RENDERBUFFER) gl_shadow_set(gl_shadow.renderbuffer, (GLint)renderbuffer);
    gl_shadow_real.BindRenderbuffer(target, renderbuffer);
}

static void APIENTRY gl_shadow_BindFramebuffer(GLenum target, GLuint framebuffer) {
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) gl_shadow_set(gl_shadow.draw_fbo, (GLint)framebuffer);
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) gl_shadow_set(gl_shadow.read_fbo, (GLint)framebuffer);
    gl_shadow_real.BindFramebuffer(target, framebuffer);
}

static void APIENTRY gl_shadow_ActiveTexture(GLenum texture) {
    gl_shadow_set(gl_shadow.active_texture, (GLint)texture);
    gl_shadow_real.ActiveTexture(texture);
}

static void APIENTRY gl_shadow_BindTexture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D) {
        const int unit = gl_shadow_unit();
        if (unit >= 0) {
            gl_shadow_set(gl_shadow.texture_2d[unit], (GLint)texture);
        } else {
            for (GlShadowValue& value : gl_shadow.texture_2d) value.known = false;
        }
    }
    gl_shadow_real.BindTexture(target, texture);
}

static void APIENTRY gl_shadow_BindSampler(GLuint unit, GLuint sampler) {
    if (unit < GL_SHADOW_UNITS) gl_shadow_set(gl_shadow.sampler[unit], (GLint)sampler);
    gl_shadow_real.BindSampler(unit, sampler);
}

static void APIENTRY gl_shadow_BlendFunc(GLenum sfactor, GLenum dfactor) {
    gl_shadow_set(gl_shadow.blend_src_rgb, (GLint)sfactor);
    gl_shadow_set(gl_shadow.blend_src_a, (GLint)sfactor);
    gl_shadow_set(gl_shadow.blend_dst_rgb, (GLint)dfactor);
    gl_shadow_set(gl_shadow.blend_dst_a, (GLint)dfactor);
    gl_shadow_real.BlendFunc(sfactor, dfactor);
}

static void APIENTRY gl_shadow_BlendFuncSeparate(GLenum src_rgb, GLenum dst_rgb, GLenum src_a, GLenum dst_a) {
    gl_shadow_set(gl_shadow.blend_src_rgb, (GLint)src_rgb);
    gl_shadow_set(gl_shadow.blend_dst_rgb, (GLint)dst_rgb);
    gl_shadow_set(gl_shadow.blend_src_a, (GLint)src_a);
    gl_shadow_set(gl_shadow.blend_dst_a, (GLint)dst_a);
    gl_shadow_real.BlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
}

static void APIENTRY gl_shadow_BlendEquation(GLenum mode) {
    gl_shadow_set(gl_shadow.blend_eq_rgb, (GLint)mode);
    gl_shadow_set(gl_shadow.blend_eq_a, (GLint)mode);
    gl_shadow_real.BlendEquation(mode);
}

static void APIENTRY gl_shadow_BlendEquationSeparate(GLenum mode_rgb, GLenum mode_a) {
    gl_shadow_set(gl_shadow.blend_eq_rgb, (GLint)mode_rgb);
    gl_shadow_set(gl_shadow.blend_eq_a, (GLint)mode_a);
    gl_shadow_real.BlendEquationSeparate(mode_rgb, mode_a);
}

static void APIENTRY gl_shadow_BlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if (gl_shadow_unit_range(r, g, b, a)) {
        gl_shadow_setf(gl_shadow.blend_color, r, g, b, a);
    } else {
        gl_shadow.blend_color.known = false;
    }
    gl_shadow_real.BlendColor(r, g, b, a);
}

static void APIENTRY gl_shadow_DepthFunc(GLenum func) {
    gl_shadow_set(gl_shadow.depth_func, (GLint)func);
    gl_shadow_real.DepthFunc(func);
}

static void APIENTRY gl_shadow_DepthMask(GLboolean flag) {
    gl_shadow_set(gl_shadow.depth_mask, flag ? 1 : 0);
    gl_shadow_real.DepthMask(flag);
}

static void APIENTRY gl_shadow_ColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    gl_shadow_set(gl_shadow.color_mask, r ? 1 : 0, g ? 1 : 0, b ? 1 : 0, a ? 1 : 0);
    gl_shadow_real.ColorMask(r, g, b, a);
}

static void APIENTRY gl_shadow_ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if (gl_shadow_unit_range(r, g, b, a)) {
        gl_shadow_setf(gl_shadow.clear_color, r, g, b, a);
    } else {
        gl_shadow.clear_color.known = false;
    }
    gl_shadow_real.ClearColor(r, g, b, a);
}

static void APIENTRY gl_shadow_Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (width >= 0 && height >= 0 && width <= GL_SHADOW_MAX_SIZE && height <= GL_SHADOW_MAX_SIZE) {
        gl_shadow_set(gl_shadow.viewport, x, y, width, height);
    } else {
        gl_shadow.viewport.known = false;
    }
    gl_shadow_real.Viewport(x, y, width, height);
}

static void APIENTRY gl_shadow_Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (width >= 0 && height >= 0) {
        gl_shadow_set(gl_shadow.scissor_box, x, y, width, height);
    } else {
        gl_shadow.scissor_box.known = false;
    }
    gl_shadow_real.Scissor(x, y, width, height);
}

static void APIENTRY gl_shadow_Enable(GLenum cap) {
    if (GlShadowValue* maybe_value = gl_shadow_cap(cap)) gl_shadow_set(*maybe_value, 1);
    gl_shadow_real.Enable(cap);
}

static void APIENTRY gl_shadow_Disable(GLenum cap) {
    if (GlShadowValue* maybe_value = gl_shadow_cap(cap)) gl_shadow_set(*maybe_value, 0);
    gl_shadow_real.Disable(cap);
}

/* ---- deletes: a deleted object leaves its bindings; forget them ---- */

static void APIENTRY gl_shadow_DeleteBuffers(GLsizei n, const GLuint* buffers) {
    gl_shadow.array_buffer.known = false;
    gl_shadow_real.DeleteBuffers(n, buffers);
}

static void APIENTRY gl_shadow_DeleteTextures(GLsizei n, const GLuint* textures) {
    for (GlShadowValue& value : gl_shadow.texture_2d) value.known = false;
    gl_shadow_real.DeleteTextures(n, textures);
}

static void APIENTRY gl_shadow_DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    gl_shadow.draw_fbo.known = gl_shadow.read_fbo.known = false;
    gl_shadow_real.DeleteFramebuffers(n, framebuffers);
}

static void APIENTRY gl_shadow_DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    gl_shadow.renderbuffer.known = false;
    gl_shadow_real.DeleteRenderbuffers(n, renderbuffers);
}

static void APIENTRY gl_shadow_DeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    gl_shadow.vao.known = false;
    gl_shadow_real.DeleteVertexArrays(n, arrays);
}

static void APIENTRY gl_shadow_DeleteSamplers(GLsizei n, const GLuint* samplers) {
    for (GlShadowValue& value : gl_shadow.sampler) value.known = false;
    gl_shadow_real.DeleteSamplers(n, samplers);
}

static void APIENTRY gl_shadow_DeleteProgram(GLuint program) {
    gl_shadow.program.known = false;
    gl_shadow_real.DeleteProgram(program);
}

/* ---- queries: answer from memory when the value is known ---- */

static void APIENTRY gl_shadow_GetIntegerv(GLenum pname, GLint* data) {
    int count = 1;
    GlShadowValue* maybe_value = gl_shadow_integer(pname == GL_FRAMEBUFFER_BINDING ? GL_DRAW_FRAMEBUFFER_BINDING : pname,
                                                   &count);
    if (maybe_value == nullptr) {
        gl_shadow_real.GetIntegerv(pname, data);
        return;
    }
    if (!maybe_value->known) {
        gl_shadow_asked++;
        gl_shadow_real.GetIntegerv(pname, maybe_value->i);
        maybe_value->known = true;
    } else {
        gl_shadow_answers++;
        if (gl_shadow_check) {
            GLint driver[4] = { 0, 0, 0, 0 };
            gl_shadow_real.GetIntegerv(pname, driver);
            if (memcmp(driver, maybe_value->i, sizeof(GLint) * (size_t)count) != 0) gl_shadow_report(pname);
        }
    }
    memcpy(data, maybe_value->i, sizeof(GLint) * (size_t)count);
}

static void APIENTRY gl_shadow_GetBooleanv(GLenum pname, GLboolean* data) {
    GlShadowValue* maybe_value = pname == GL_DEPTH_WRITEMASK ? &gl_shadow.depth_mask
                               : pname == GL_COLOR_WRITEMASK ? &gl_shadow.color_mask : nullptr;
    const int count = pname == GL_COLOR_WRITEMASK ? 4 : 1;
    if (maybe_value == nullptr) {
        gl_shadow_real.GetBooleanv(pname, data);
        return;
    }
    if (!maybe_value->known) {
        GLboolean driver[4] = { 0, 0, 0, 0 };
        gl_shadow_asked++;
        gl_shadow_real.GetBooleanv(pname, driver);
        gl_shadow_set(*maybe_value, driver[0] ? 1 : 0, driver[1] ? 1 : 0, driver[2] ? 1 : 0, driver[3] ? 1 : 0);
    } else {
        gl_shadow_answers++;
        if (gl_shadow_check) {
            GLboolean driver[4] = { 0, 0, 0, 0 };
            gl_shadow_real.GetBooleanv(pname, driver);
            for (int k = 0; k < count; k++) {
                if ((driver[k] != 0) != (maybe_value->i[k] != 0)) { gl_shadow_report(pname); break; }
            }
        }
    }
    for (int k = 0; k < count; k++) data[k] = maybe_value->i[k] ? GL_TRUE : GL_FALSE;
}

static void APIENTRY gl_shadow_GetFloatv(GLenum pname, GLfloat* data) {
    GlShadowValue* maybe_value = pname == GL_COLOR_CLEAR_VALUE ? &gl_shadow.clear_color
                               : pname == GL_BLEND_COLOR ? &gl_shadow.blend_color : nullptr;
    if (maybe_value == nullptr) {
        gl_shadow_real.GetFloatv(pname, data);
        return;
    }
    if (!maybe_value->known) {
        GLfloat driver[4] = { 0, 0, 0, 0 };
        gl_shadow_asked++;
        gl_shadow_real.GetFloatv(pname, driver);
        gl_shadow_setf(*maybe_value, driver[0], driver[1], driver[2], driver[3]);
    } else {
        gl_shadow_answers++;
        if (gl_shadow_check) {
            GLfloat driver[4] = { 0, 0, 0, 0 };
            gl_shadow_real.GetFloatv(pname, driver);
            if (memcmp(driver, maybe_value->f, sizeof(driver)) != 0) gl_shadow_report(pname);
        }
    }
    memcpy(data, maybe_value->f, sizeof(GLfloat) * 4);
}

static GLboolean APIENTRY gl_shadow_IsEnabled(GLenum cap) {
    GlShadowValue* maybe_value = gl_shadow_cap(cap);
    if (maybe_value == nullptr) return gl_shadow_real.IsEnabled(cap);
    if (!maybe_value->known) {
        gl_shadow_asked++;
        gl_shadow_set(*maybe_value, gl_shadow_real.IsEnabled(cap) ? 1 : 0);
    } else {
        gl_shadow_answers++;
        if (gl_shadow_check && (gl_shadow_real.IsEnabled(cap) != 0) != (maybe_value->i[0] != 0)) gl_shadow_report(cap);
    }
    return maybe_value->i[0] ? GL_TRUE : GL_FALSE;
}

/** GE_GLSHADOW_CHECK: one line a few seconds apart saying how the record is doing. */
static void gfx_gl_shadow_frame(void) {
    static unsigned frames = 0;
    if (!gl_shadow_check || (++frames % 600u) != 0) return;
    printf("ge: glshadow: %u queries answered from memory, %u asked of the driver, %u mismatches\n",
           gl_shadow_answers, gl_shadow_asked, gl_shadow_mismatches);
    fflush(stdout);
}

/** After glad has loaded a context's functions: start a fresh record and step in. */
static void gfx_gl_shadow_install(void) {
    const char* maybe_off = getenv("GE_GLSHADOW");
    const char* maybe_check = getenv("GE_GLSHADOW_CHECK");
    gl_shadow = GlShadowState();
    if (maybe_off != nullptr && maybe_off[0] == '0') {
        printf("ge: glshadow: off (GE_GLSHADOW=0); GL state queries go to the driver\n");
        return;
    }
    gl_shadow_check = (maybe_check != nullptr && maybe_check[0] != '\0' && maybe_check[0] != '0');
#define GL_SHADOW_HOOK(name) \
    do { \
        if (glad_gl##name != nullptr && glad_gl##name != gl_shadow_##name) { \
            gl_shadow_real.name = glad_gl##name; \
            glad_gl##name = gl_shadow_##name; \
        } \
    } while (0)
    /* Every function is hooked or none: a setter that went unrecorded would
     * leave a stale answer behind. BindSampler and DeleteSamplers arrived with
     * GL 3.3; a context without them never binds a sampler. */
    if (glad_glBindVertexArray == nullptr || glad_glBindFramebuffer == nullptr || glad_glBlendFuncSeparate == nullptr
        || glad_glBlendEquationSeparate == nullptr || glad_glBindRenderbuffer == nullptr
        || (glad_glBindSampler == nullptr) != (glad_glDeleteSamplers == nullptr)) {
        printf("ge: glshadow: off (this GL context lacks functions it tracks)\n");
        return;
    }
    GL_SHADOW_HOOK(UseProgram); GL_SHADOW_HOOK(BindVertexArray); GL_SHADOW_HOOK(BindBuffer);
    GL_SHADOW_HOOK(BindRenderbuffer); GL_SHADOW_HOOK(BindFramebuffer); GL_SHADOW_HOOK(ActiveTexture);
    GL_SHADOW_HOOK(BindTexture); GL_SHADOW_HOOK(BindSampler); GL_SHADOW_HOOK(BlendFunc);
    GL_SHADOW_HOOK(BlendFuncSeparate); GL_SHADOW_HOOK(BlendEquation); GL_SHADOW_HOOK(BlendEquationSeparate);
    GL_SHADOW_HOOK(BlendColor); GL_SHADOW_HOOK(DepthFunc); GL_SHADOW_HOOK(DepthMask); GL_SHADOW_HOOK(ColorMask);
    GL_SHADOW_HOOK(ClearColor); GL_SHADOW_HOOK(Viewport); GL_SHADOW_HOOK(Scissor); GL_SHADOW_HOOK(Enable);
    GL_SHADOW_HOOK(Disable); GL_SHADOW_HOOK(DeleteBuffers); GL_SHADOW_HOOK(DeleteTextures);
    GL_SHADOW_HOOK(DeleteFramebuffers); GL_SHADOW_HOOK(DeleteRenderbuffers); GL_SHADOW_HOOK(DeleteVertexArrays);
    GL_SHADOW_HOOK(DeleteSamplers); GL_SHADOW_HOOK(DeleteProgram); GL_SHADOW_HOOK(GetIntegerv);
    GL_SHADOW_HOOK(GetBooleanv); GL_SHADOW_HOOK(GetFloatv); GL_SHADOW_HOOK(IsEnabled);
#undef GL_SHADOW_HOOK
    printf("ge: glshadow: on%s; GL state queries are answered from memory\n", gl_shadow_check ? " (checking)" : "");
    fflush(stdout);
}
