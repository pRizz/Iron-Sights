/* FORK-058: optional bloom for the shiniest parts of a shiny weapon.
 * Included by gfx_opengl.cpp (it needs the backend's framebuffer table and
 * program state). gfx_pc.cpp opens a span at the shiny weapon's G_NOOP tag;
 * every draw in it is drawn again into gl_bloom_fbo with the same program and
 * depth state: shiny batches in colour, the others black (blend ZERO, ZERO),
 * so the buffer holds the reflective pixels that stay visible. At the span's
 * end a soft-threshold bright pass downsamples it to half size, a separable
 * Gaussian blurs it `passes` times with a growing step, and the result is
 * added onto the current target. All GL state touched is saved and restored. */

static bool gl_bloom_capturing = false, gl_bloom_shiny = false, gl_bloom_drawn = false, gl_bloom_failed = false;
static GLuint gl_bloom_fbo = 0, gl_bloom_tex = 0, gl_bloom_depth = 0;
static GLuint gl_bloom_half_fbo[2] = { 0, 0 }, gl_bloom_half_tex[2] = { 0, 0 };
static uint32_t gl_bloom_w = 0, gl_bloom_h = 0;
static GLuint gl_bloom_prog[3] = { 0, 0, 0 }; /* bright pass, blur, add */
static GLint gl_bloom_u_tex[3], gl_bloom_u_thr, gl_bloom_u_step, gl_bloom_u_gain, gl_bloom_u_texel;
static GLuint gl_bloom_vao = 0, gl_bloom_vbo = 0;

static bool gl_bloom_programs(void) {
    if (gl_bloom_prog[0] != 0) return true;
    if (gl_bloom_failed) return false;
    if (gl_glsl_version < 130 || glGenVertexArrays == NULL) {
        sysLogPrintf(LOG_WARNING, "shiny bloom: needs GLSL 1.30 and vertex arrays - off");
        gl_bloom_failed = true;
        return false;
    }
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n%s", gl_glsl_version_str, gl_es ? "precision mediump float;\n" : "");
    const std::string vs = std::string(hdr) + "in vec2 aPos;\nout vec2 vUv;\n"
                           "void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    /* bright pass: one bilinear tap at the 2x2 centre averages the block */
    const std::string fs0 = std::string(hdr) + "uniform sampler2D uTex;\nuniform float uThr;\nin vec2 vUv;\nout vec4 outColor;\n"
        "void main() {\n"
        "    vec3 c = texture(uTex, vUv).rgb;\n"
        "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
        "    float k = smoothstep(uThr, min(uThr + 0.25, 1.0), l);\n"
        "    outColor = vec4(c * k, 1.0);\n"
        "}\n";
    const std::string fs1 = std::string(hdr) + "uniform sampler2D uTex;\nuniform vec2 uStep;\nin vec2 vUv;\nout vec4 outColor;\n"
        "void main() {\n"
        "    vec3 s = texture(uTex, vUv).rgb * 0.2270270;\n"
        "    s += (texture(uTex, vUv + uStep * 1.3846154).rgb + texture(uTex, vUv - uStep * 1.3846154).rgb) * 0.3162162;\n"
        "    s += (texture(uTex, vUv + uStep * 3.2307692).rgb + texture(uTex, vUv - uStep * 3.2307692).rgb) * 0.0702703;\n"
        "    outColor = vec4(s, 1.0);\n"
        "}\n";
    const std::string fs2 = std::string(hdr) + "uniform sampler2D uTex;\nuniform float uGain;\nuniform vec2 uTexel;\nout vec4 outColor;\n"
        "void main() { outColor = vec4(texture(uTex, gl_FragCoord.xy * uTexel).rgb * uGain, 1.0); }\n";
    gl_bloom_prog[0] = vi_compile(vs.c_str(), fs0.c_str());
    gl_bloom_prog[1] = gl_bloom_prog[0] ? vi_compile(vs.c_str(), fs1.c_str()) : 0;
    gl_bloom_prog[2] = gl_bloom_prog[1] ? vi_compile(vs.c_str(), fs2.c_str()) : 0;
    if (gl_bloom_prog[2] == 0) {
        for (GLuint& p : gl_bloom_prog) {
            if (p) glDeleteProgram(p);
            p = 0;
        }
        gl_bloom_failed = true;
        return false;
    }
    for (int i = 0; i < 3; i++) gl_bloom_u_tex[i] = glGetUniformLocation(gl_bloom_prog[i], "uTex");
    gl_bloom_u_thr = glGetUniformLocation(gl_bloom_prog[0], "uThr");
    gl_bloom_u_step = glGetUniformLocation(gl_bloom_prog[1], "uStep");
    gl_bloom_u_gain = glGetUniformLocation(gl_bloom_prog[2], "uGain");
    gl_bloom_u_texel = glGetUniformLocation(gl_bloom_prog[2], "uTexel");
    static const float quad[] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f };
    glGenVertexArrays(1, &gl_bloom_vao);
    glGenBuffers(1, &gl_bloom_vbo);
    glBindVertexArray(gl_bloom_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_bloom_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    for (int i = 0; i < 3; i++) {
        const GLint loc = glGetAttribLocation(gl_bloom_prog[i], "aPos");
        if (loc >= 0) {
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        }
    }
    return true;
}

static GLuint gl_bloom_texture(uint32_t w, uint32_t h) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

/* the capture buffer (with depth) at the target's size, plus two at half size */
static bool gl_bloom_buffers(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return false;
    if (gl_bloom_fbo != 0 && w == gl_bloom_w && h == gl_bloom_h) return true;
    if (gl_bloom_fbo != 0) {
        glDeleteFramebuffers(1, &gl_bloom_fbo);
        glDeleteFramebuffers(2, gl_bloom_half_fbo);
        glDeleteTextures(1, &gl_bloom_tex);
        glDeleteTextures(2, gl_bloom_half_tex);
        glDeleteRenderbuffers(1, &gl_bloom_depth);
    }
    gl_bloom_w = w;
    gl_bloom_h = h;
    gl_bloom_tex = gl_bloom_texture(w, h);
    glGenRenderbuffers(1, &gl_bloom_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, gl_bloom_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glGenFramebuffers(1, &gl_bloom_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_bloom_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_bloom_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl_bloom_depth);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(2, gl_bloom_half_fbo);
    for (int i = 0; i < 2; i++) {
        gl_bloom_half_tex[i] = gl_bloom_texture((w + 1) / 2, (h + 1) / 2);
        glBindFramebuffer(GL_FRAMEBUFFER, gl_bloom_half_fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_bloom_half_tex[i], 0);
        ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    if (!ok) {
        sysLogPrintf(LOG_WARNING, "shiny bloom: incomplete framebuffer - off");
        gl_bloom_failed = true;
    }
    return ok;
}

static GLuint gl_bloom_target(void) {
    return current_framebuffer < framebuffers.size() ? framebuffers[current_framebuffer].fbo : 0;
}

static void gfx_opengl_bloom_begin(void) {
    gl_bloom_capturing = false;
    if (gl_bloom_failed || current_framebuffer >= framebuffers.size()) return;
    const Framebuffer& fb = framebuffers[current_framebuffer];
    GLint tex, vao, vbo, rbo;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &rbo);
    const bool ok = gl_bloom_programs() && gl_bloom_buffers(fb.width, fb.height);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    if (ok) {
        GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST), depth_mask;
        GLfloat clear[4];
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        glBindFramebuffer(GL_FRAMEBUFFER, gl_bloom_fbo);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_TRUE);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glDepthMask(depth_mask);
        if (scissor) glEnable(GL_SCISSOR_TEST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, gl_bloom_target());
    gl_bloom_capturing = ok;
    gl_bloom_drawn = false;
}

static void gfx_opengl_bloom_mark(bool shiny) {
    gl_bloom_shiny = shiny;
}

/* gfx_opengl_draw_triangles, after the real draw: the same vertices again */
static void gl_bloom_mirror(size_t vertex_count) {
    if (!gl_bloom_capturing) return;
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLint src_rgb, dst_rgb, src_a, dst_a;
    if (!gl_bloom_shiny) {
        glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_a);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_a);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ZERO); /* an occluder: black where it is visible */
    }
    glBindFramebuffer(GL_FRAMEBUFFER, gl_bloom_fbo);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_bloom_target());
    if (!gl_bloom_shiny) {
        glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
        if (!blend) glDisable(GL_BLEND);
    } else {
        gl_bloom_drawn = true;
    }
}

static void gl_bloom_pass(GLuint fbo, uint32_t w, uint32_t h, int prog, GLuint src) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glUseProgram(gl_bloom_prog[prog]);
    glUniform1i(gl_bloom_u_tex[prog], 0);
    glBindTexture(GL_TEXTURE_2D, src);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void gfx_opengl_bloom_end(float threshold, float strength, float radius, int passes) {
    if (!gl_bloom_capturing) return;
    gl_bloom_capturing = false;
    if (!gl_bloom_drawn || strength <= 0.0f) return;

    GLint viewport[4], scissor_box[4], active, tex0, prog, vao, vbo, src_rgb, dst_rgb, src_a, dst_a, eq_rgb, eq_a;
    GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull = glIsEnabled(GL_CULL_FACE), stencil = glIsEnabled(GL_STENCIL_TEST), depth_mask, cmask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_a);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_a);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eq_a);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindVertexArray(gl_bloom_vao);

    const uint32_t hw = (gl_bloom_w + 1) / 2, hh = (gl_bloom_h + 1) / 2;
    glUseProgram(gl_bloom_prog[0]);
    glUniform1f(gl_bloom_u_thr, threshold);
    gl_bloom_pass(gl_bloom_half_fbo[0], hw, hh, 0, gl_bloom_tex);
    if (passes < 1) passes = 1;
    for (int i = 0; i < passes; i++) {
        const float step = radius * (float)(i + 1);
        glUseProgram(gl_bloom_prog[1]);
        glUniform2f(gl_bloom_u_step, step / (float)hw, 0.0f);
        gl_bloom_pass(gl_bloom_half_fbo[1], hw, hh, 1, gl_bloom_half_tex[0]);
        glUniform2f(gl_bloom_u_step, 0.0f, step / (float)hh);
        gl_bloom_pass(gl_bloom_half_fbo[0], hw, hh, 1, gl_bloom_half_tex[1]);
    }
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(gl_bloom_prog[2]);
    glUniform1f(gl_bloom_u_gain, strength);
    glUniform2f(gl_bloom_u_texel, 1.0f / (float)gl_bloom_w, 1.0f / (float)gl_bloom_h);
    gl_bloom_pass(gl_bloom_target(), gl_bloom_w, gl_bloom_h, 2, gl_bloom_half_tex[0]);

    glBindTexture(GL_TEXTURE_2D, tex0);
    glActiveTexture(active);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUseProgram(prog);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
    glBlendEquationSeparate(eq_rgb, eq_a);
    glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
    glDepthMask(depth_mask);
    glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
    if (!blend) glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE);
    if (stencil) glEnable(GL_STENCIL_TEST);
}
