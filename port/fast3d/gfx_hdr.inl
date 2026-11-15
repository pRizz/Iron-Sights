/* FORK-082: HDR Output, the GL side. Included by gfx_opengl.cpp after
 * gfx_grade.inl; the DXGI presenter is gfx_hdr_dxgi.cpp.
 *
 * While it is active, framebuffer 0 - "the window" to the rest of the renderer
 * - is an RGBA16F texture of the window's size (its fbo replaces GL's default
 * framebuffer in framebuffers[0]), and every other target allocates RGBA16F
 * too (gl_fb_color_format), so the grade's values above 1 survive MSAA
 * resolves, N64 AA and the Low Res scale. The values stay gamma-encoded and
 * relative to paper white (gfx_grade.inl). At swap, gfx_opengl_hdr_present
 * converts them to scRGB (linear Rec.709, 1.0 = 80 nits: lin * paper / 80)
 * into the texture shared with the DXGI swap chain, flipped to D3D's top-down
 * rows, and presents; gfx_sdl2.cpp skips SDL_GL_SwapWindow meanwhile.
 *
 * It runs only while the window's monitor is in Windows HDR mode (checked
 * every 2 s); otherwise, or when the presenter fails, the renderer keeps GL's
 * own window and reports why in g_PortGraphicsRender.hdr_state. */

extern "C" SDL_Window* gfx_sdl_get_window(void);

static bool gl_hdr_prog_failed = false; /* gl_hdr_on is declared in gfx_opengl.cpp */
static GLuint gl_hdr_fbo = 0, gl_hdr_tex = 0, gl_hdr_out_fbo = 0, gl_hdr_out_tex = 0;
static uint32_t gl_hdr_w = 0, gl_hdr_h = 0;
static GLuint gl_hdr_prog = 0, gl_hdr_vao = 0, gl_hdr_vbo = 0;
static GLint gl_hdr_u_src = -1, gl_hdr_u_scale = -1, gl_hdr_u_h = -1;
static bool gl_hdr_display = false;
static uint32_t gl_hdr_check_frame = 0;

static bool gl_hdr_program(void) {
    if (gl_hdr_prog != 0) return true;
    if (gl_hdr_prog_failed || gl_glsl_version < 130 || glGenVertexArrays == NULL) {
        gl_hdr_prog_failed = true;
        return false;
    }
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n", gl_glsl_version_str);
    const std::string vs = std::string(hdr) + "in vec2 aPos;\nvoid main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    const std::string fs = std::string(hdr) +
        "uniform sampler2D uSrc;\nuniform float uScale;\nuniform int uH;\nout vec4 outColor;\n"
        "void main() {\n"
        "    ivec2 p = ivec2(gl_FragCoord.xy);\n"
        "    vec3 v = texelFetch(uSrc, ivec2(p.x, uH - 1 - p.y), 0).rgb;\n"   /* D3D rows run top-down */
        "    outColor = vec4(pow(max(v, 0.0), vec3(2.2)) * uScale, 1.0);\n"
        "}\n";
    gl_hdr_prog = vi_compile(vs.c_str(), fs.c_str());
    if (gl_hdr_prog == 0) {
        gl_hdr_prog_failed = true;
        return false;
    }
    gl_hdr_u_src = glGetUniformLocation(gl_hdr_prog, "uSrc");
    gl_hdr_u_scale = glGetUniformLocation(gl_hdr_prog, "uScale");
    gl_hdr_u_h = glGetUniformLocation(gl_hdr_prog, "uH");
    static const float quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    /* leave the renderer's VAO and buffer bound: it sets attribute pointers into
     * whatever VAO is bound, and once overwrote this quad's (half a window drew) */
    GLint had_vao = 0, had_vbo = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &had_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &had_vbo);
    glGenVertexArrays(1, &gl_hdr_vao);
    glGenBuffers(1, &gl_hdr_vbo);
    glBindVertexArray(gl_hdr_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_hdr_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    const GLint pos = glGetAttribLocation(gl_hdr_prog, "aPos");
    glEnableVertexAttribArray((GLuint)pos);
    glVertexAttribPointer((GLuint)pos, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindVertexArray((GLuint)had_vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)had_vbo);
    return true;
}

/* the window-sized float target that stands in for framebuffer 0 */
static bool gl_hdr_window_target(uint32_t w, uint32_t h) {
    if (gl_hdr_fbo == 0) {
        glGenFramebuffers(1, &gl_hdr_fbo);
        glGenTextures(1, &gl_hdr_tex);
        glGenFramebuffers(1, &gl_hdr_out_fbo);
    }
    GLint had_tex = 0, had_fbo = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &had_tex);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &had_fbo);
    glBindTexture(GL_TEXTURE_2D, gl_hdr_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_hdr_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_hdr_tex, 0);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    /* a fresh shared texture each size: the presenter registers it */
    if (gl_hdr_out_tex != 0) glDeleteTextures(1, &gl_hdr_out_tex);
    glGenTextures(1, &gl_hdr_out_tex);
    ok = ok && gfx_hdr_dxgi_target(gfx_sdl_get_window(), w, h, gl_hdr_out_tex);
    if (ok) {
        glBindFramebuffer(GL_FRAMEBUFFER, gl_hdr_out_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_hdr_out_tex, 0);
    }
    glBindTexture(GL_TEXTURE_2D, (GLuint)had_tex);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)had_fbo);
    gl_hdr_w = w;
    gl_hdr_h = h;
    return ok;
}

static void gl_hdr_stop(void) {
    if (!gl_hdr_on) return;
    gl_hdr_on = false;
    framebuffers[0].fbo = 0;
    gfx_hdr_dxgi_shutdown();
    gl_hdr_w = gl_hdr_h = 0;
}

/* update_framebuffer_parameters(0): start, resize or stop HDR Output for this frame */
static void gl_hdr_sync(uint32_t w, uint32_t h) {
    const bool want = g_PortGraphicsRender.hdr_output && gfx_framebuffers_enabled && !gl_es;
    if (!want) {
        gl_hdr_stop();
        g_PortGraphicsRender.hdr_state = PORT_HDR_OFF;
        gl_hdr_check_frame = 0;
        return;
    }
    if (gfx_hdr_dxgi_failed() || gl_hdr_prog_failed) {
        gl_hdr_stop();
        g_PortGraphicsRender.hdr_state = PORT_HDR_UNSUPPORTED;
        return;
    }
    if (gl_hdr_check_frame == 0 || frame_count - gl_hdr_check_frame >= 120) {
        float peak = 0.0f, white = 0.0f;
        const bool was = gl_hdr_display;
        gl_hdr_display = gfx_hdr_dxgi_display(gfx_sdl_get_window(), &peak, &white);
        g_PortGraphicsRender.hdr_display_peak = (int)peak;
        if (gl_hdr_display != was || gl_hdr_check_frame == 0) {
            sysLogPrintf(LOG_NOTE, "HDR Output: monitor HDR %s, peak %.0f nits, Windows SDR white %.0f nits",
                         gl_hdr_display ? "on" : "off", peak, white);
        }
        gl_hdr_check_frame = frame_count ? frame_count : 1;
    }
    static int force = -1; /* GE_HDR_FORCE=1: run the presenter on an SDR monitor too (audits) */
    if (force < 0) force = getenv("GE_HDR_FORCE") != NULL;
    if (!gl_hdr_display && !force) {
        gl_hdr_stop();
        g_PortGraphicsRender.hdr_state = PORT_HDR_SDR_DISPLAY;
        return;
    }
    if (!gl_hdr_program() || ((w != gl_hdr_w || h != gl_hdr_h) && !gl_hdr_window_target(w, h))) {
        gl_hdr_stop();
        g_PortGraphicsRender.hdr_state = PORT_HDR_UNSUPPORTED;
        return;
    }
    gl_hdr_on = true;
    framebuffers[0].fbo = gl_hdr_fbo;
    g_PortGraphicsRender.hdr_state = PORT_HDR_ACTIVE;
}

static bool gl_target_is_float(size_t fb_id) {
    return fb_id == 0 ? gl_hdr_on : framebuffers[fb_id].color_fmt == GL_RGBA16F;
}

/* gfx_sdl2.cpp swap_buffers_begin: present through DXGI; false = GL swaps as usual */
extern "C" bool gfx_opengl_hdr_present(int vsync) {
    if (!gl_hdr_on) return false;
    if (!gfx_hdr_dxgi_lock()) {
        gl_hdr_stop();
        return false;
    }
    GLint viewport[4], prog, vao, vbo, tex0, active, draw_fbo;
    GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull = glIsEnabled(GL_CULL_FACE), stencil = glIsEnabled(GL_STENCIL_TEST), cmask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);    /* the frame's last draw may have left either on: */
    glDisable(GL_STENCIL_TEST); /* culling dropped half the quad */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl_hdr_out_fbo);
    glViewport(0, 0, (GLsizei)gl_hdr_w, (GLsizei)gl_hdr_h);
    glUseProgram(gl_hdr_prog);
    glBindVertexArray(gl_hdr_vao);
    glBindTexture(GL_TEXTURE_2D, gl_hdr_tex);
    glUniform1i(gl_hdr_u_src, 0);
    glUniform1f(gl_hdr_u_scale, (float)g_PortGraphicsRender.hdr_paper / 80.0f);
    glUniform1i(gl_hdr_u_h, (GLint)gl_hdr_h);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex0);
    glActiveTexture((GLenum)active);
    glBindVertexArray((GLuint)vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)vbo);
    glUseProgram((GLuint)prog);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
    if (blend) glEnable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE);
    if (stencil) glEnable(GL_STENCIL_TEST);
    if (!gfx_hdr_dxgi_present(vsync)) {
        gl_hdr_stop();
        return false;
    }
    return true;
}
