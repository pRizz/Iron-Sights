/* FORK-082: the Graphics page's scene grade (Tone Mapping and its sliders) and
 * HDR Output's extended-range encode. Included by gfx_opengl.cpp after
 * gfx_hdr.inl.
 *
 * gfx_pc.cpp calls grade at G_NOOP 0x66 (bondview2.c, once the first-person
 * guns, hands and arms have drawn and before the watch, HUD and fades), so the
 * grade reaches the level, characters and the viewmodel while 2D keeps its
 * stock colours. The bound target is copied (resolving MSAA) into a mipmapped
 * texture and the current viewport (the player's view: one rect per player in
 * split screen) is drawn back through the grade shader; everything outside the
 * rect is untouched. Glow first draws a bright pass at half size and mipmaps it.
 *
 * Per pixel, with the N64 image as gamma 2.2:
 *  1. Sharpen: contrast-adaptive sharpening of the 4 neighbours (after AMD's
 *     CAS): strong in flat, mid-tone texture, backing off near clipping and
 *     on hard edges, so no halos.
 *  2. Glow: the bright pass (linear light above 0.4 luminance and above its
 *     surroundings, so a whole snowfield does not glow) sampled at four
 *     mip levels of the half-size buffer and added, so lamps, sky and flashes
 *     bleed a soft halo; a curve rolls the result off afterwards.
 *  3. Brightness (an exposure gain) and Contrast (a power around mid grey).
 *  4. Intensity mixes in a filmic highlight roll-off: an ACES-fitted curve on a
 *     highlight range recovered from the image (inverse Reinhard on luminance,
 *     white to 2.5x), exposure 0.53 so mid grey stays put, normalised to keep
 *     white. Below a luminance of 0.1-0.3 it takes the brighter of curve and
 *     image, so shadows never sink (the plain curve darkened a quarter-bright
 *     pixel by ~20%, and GoldenEye's scenes sit mostly in that range).
 *     HDR Output (a float target) uses a soft shoulder into the display's peak
 *     instead, with values relative to paper white (gfx_hdr.inl converts them
 *     to scRGB), so the HUD drawn afterwards sits at paper white.
 *  5. Vibrance: more saturation for the less saturated colours.
 *  Auto Exposure (gfx_exposure.inl) scales Brightness by the eye-adaptation
 *  shift before step 3.
 *  6. Saturation (0 = black and white) and Temperature (white-balance gains,
 *     luminance kept), in linear light.
 *  7. Clarity: local contrast - the pixel's luminance against a 16x16 mip
 *     average of the image, added back in gamma space, limited to +-0.25 and
 *     faded toward black, and only halfway toward white, so bright surfaces
 *     such as snow keep their texture.
 *  8. Split tone (shadows toward one colour, highlights toward the other,
 *     brightness kept), Fade (blacks lifted to 0.12, white lowered 5%),
 *     Vignette (corners darkened up to 60%, by the view's own rect) and Film
 *     Grain (a hash per pixel and frame, strongest in mid tones).
 * With Tone Mapping off and HDR Output on no pass runs: the stock image already
 * sits at paper white. All GL state touched is saved and restored. */

static bool gl_grade_failed = false;
static GLuint gl_grade_prog = 0, gl_grade_bright_prog = 0, gl_grade_vao = 0, gl_grade_vbo = 0;
static GLuint gl_grade_copy = 0, gl_grade_copy_fbo = 0, gl_grade_glow = 0, gl_grade_glow_fbo = 0;
static uint32_t gl_grade_w = 0, gl_grade_h = 0;
static GLenum gl_grade_fmt = 0;
static GLint gl_grade_u_scene = -1, gl_grade_u_glowtex = -1, gl_grade_u_hdr = -1, gl_grade_u_exposure = -1,
             gl_grade_u_expand = -1, gl_grade_u_bright = -1, gl_grade_u_contrast = -1, gl_grade_u_filmic = -1,
             gl_grade_u_vibrance = -1, gl_grade_u_clarity = -1, gl_grade_u_sharpen = -1, gl_grade_u_glow = -1,
             gl_grade_u_peak = -1, gl_grade_u_split = -1, gl_grade_u_texel = -1, gl_grade_u_size = -1,
             gl_grade_u_bp_scene = -1, gl_grade_u_bp_texel = -1, gl_grade_u_sat = -1, gl_grade_u_tone = -1,
             gl_grade_u_fade = -1, gl_grade_u_vignette = -1, gl_grade_u_grain = -1, gl_grade_u_frame = -1,
             gl_grade_u_temp = -1, gl_grade_u_shadow = -1, gl_grade_u_high = -1, gl_grade_u_rect = -1,
             gl_grade_u_adapt = -1, gl_grade_u_auto = -1, gl_grade_u_keylog = -1;

static const char* const kGradeFs =
    "uniform sampler2D uScene;\n"
    "uniform sampler2D uGlowTex;\n"
    "uniform sampler2D uAdapt;\n"     /* Auto Exposure: the adapted log2 luminance (gfx_exposure.inl) */
    "uniform float uAuto, uKeyLog;\n"
    "uniform int uHdr;\n"
    "uniform float uExposure, uExpand, uBright, uContrast, uFilmic, uVibrance, uClarity, uSharpen, uGlow;\n"
    "uniform float uPeak, uSplit;\n"
    "uniform float uSat, uTone, uFade, uVignette, uGrain, uFrame;\n"
    "uniform vec3 uTemp, uShadowTint, uHighTint;\n"
    "uniform vec4 uRect;\n"   /* the view: centre x, y and half width, height */
    "uniform vec2 uTexel;\n"
    "uniform ivec2 uSize;\n"
    "out vec4 outColor;\n"
    "const vec3 LUM = vec3(0.2126, 0.7152, 0.0722);\n"
    "vec3 aces(vec3 x) { return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14); }\n"
    "vec3 con(vec3 y) { return 0.18 * pow(max(y, 0.0) / 0.18 + 1e-6, vec3(uContrast)); }\n"
    "vec3 recover(vec3 lin) {\n"
    "    float l = dot(lin, LUM);\n"
    "    return lin * (l / max(1.0 - uExpand * l, 0.02)) / max(l, 1e-4);\n"
    "}\n"
    "vec3 fetch(ivec2 p) { return texelFetch(uScene, clamp(p, ivec2(0), uSize - 1), 0).rgb; }\n"
    "vec3 sharpened(ivec2 p, vec3 c) {\n"
    "    if (uSharpen <= 0.0) return c;\n"
    "    vec3 n = fetch(p + ivec2(0, 1)), s = fetch(p - ivec2(0, 1)), e = fetch(p + ivec2(1, 0)), w = fetch(p - ivec2(1, 0));\n"
    "    vec3 mn = min(c, min(min(n, s), min(e, w))), mx = max(c, max(max(n, s), max(e, w)));\n"
    "    vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, 1e-4), 0.0, 1.0));\n"
    "    vec3 k = -amp * mix(0.125, 0.2, uSharpen);\n"
    "    return max((c + k * (n + s + e + w)) / (1.0 + 4.0 * k), 0.0);\n"
    "}\n"
    "vec3 vibrant(vec3 x) {\n"
    "    float l = dot(x, LUM);\n"
    "    float mx = max(x.r, max(x.g, x.b)), mn = min(x.r, min(x.g, x.b));\n"
    "    float sat = (mx - mn) / max(mx, 1e-4);\n"
    "    return max(mix(vec3(l), x, 1.0 + uVibrance * (1.0 - sat)), 0.0);\n"
    "}\n"
    "void main() {\n"
    "    ivec2 p = ivec2(gl_FragCoord.xy);\n"
    "    vec3 c0 = fetch(p);\n"
    "    if (gl_FragCoord.x < uSplit) { outColor = vec4(c0, 1.0); return; }\n"   /* Compare: left half stock */
    "    vec2 uv = gl_FragCoord.xy * uTexel;\n"
    "    vec3 lin = pow(sharpened(p, c0), vec3(2.2));\n"
    "    if (uGlow > 0.0) {\n"
    "        vec3 g = textureLod(uGlowTex, uv, 1.0).rgb * 0.4 + textureLod(uGlowTex, uv, 2.0).rgb * 0.3\n"
    "               + textureLod(uGlowTex, uv, 3.0).rgb * 0.2 + textureLod(uGlowTex, uv, 4.0).rgb * 0.1;\n"
    "        lin += g * uGlow * 1.5;\n"
    "    }\n"
    "    float bright = uBright;\n"
    "    if (uAuto > 0.0) {\n"   /* eye adaptation: toward the key, more room down than up */
    "        bright *= exp2(clamp(uKeyLog - texelFetch(uAdapt, ivec2(0), 0).r, -2.0, 0.25) * uAuto);\n"
    "    }\n"
    "    vec3 ylin = con(lin * bright);\n"
    "    if (uHdr != 0) {\n"
    "        vec3 h = mix(ylin, con(recover(lin) * bright), uFilmic);\n"
    /* paper-white relative; a soft shoulder from 0.8 into the display's peak */
    "        float k = min(0.8, uPeak * 0.5), room = max(uPeak - k, 1e-3);\n"
    "        lin = min(h, vec3(k)) + room * (1.0 - exp(-max(h - k, 0.0) / room));\n"
    "    } else {\n"
    "        float white = aces(vec3(uExposure / (1.0 - uExpand))).g;\n"
    "        vec3 f = min(aces(con(recover(lin) * uExposure * bright)) / white, vec3(1.0));\n"
    "        ylin = min(ylin, vec3(1.0));\n"
    "        vec3 tm = mix(max(f, ylin), f, smoothstep(0.1, 0.3, dot(ylin, LUM)));\n"
    "        lin = mix(ylin, tm, uFilmic);\n"
    "    }\n"
    "    lin = vibrant(lin);\n"
    "    lin = max(mix(vec3(dot(lin, LUM)), lin, uSat), 0.0) * uTemp;\n"   /* saturation, then white balance */
    "    vec3 g = pow(lin, vec3(1.0 / 2.2));\n"
    "    if (uClarity > 0.0) {\n"
    "        float lg = dot(g, LUM);\n"
    "        float d = clamp(dot(c0, LUM) - dot(textureLod(uScene, uv, 4.0).rgb, LUM), -0.25, 0.25);\n"
    "        float t = clamp(lg, 0.0, 1.0) * 2.0 - 1.0;\n"
    "        g *= max(lg + uClarity * d * (1.0 - t * t * (t > 0.0 ? 0.5 : 1.0)), 0.0) / max(lg, 1e-4);\n"
    "    }\n"
    "    if (uTone > 0.0) {\n"   /* split tone: shadows toward one colour, highlights the other, brightness kept */
    "        float l = dot(g, LUM);\n"
    "        vec3 t = g * mix(uShadowTint, uHighTint, smoothstep(0.15, 0.85, l));\n"
    "        g = mix(g, t * (l / max(dot(t, LUM), 1e-4)), uTone);\n"
    "    }\n"
    "    g = mix(vec3(uFade * 0.12), vec3(1.0 - uFade * 0.05), g);\n"   /* fade: lifted blacks */
    "    if (uVignette > 0.0) {\n"
    "        vec2 q = (gl_FragCoord.xy - uRect.xy) / uRect.zw;\n"
    "        g *= 1.0 - uVignette * 0.6 * smoothstep(0.3, 1.6, dot(q, q));\n"
    "    }\n"
    "    if (uGrain > 0.0) {\n"
    "        float n = fract(sin(dot(gl_FragCoord.xy + uFrame * vec2(37.0, 17.0), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;\n"
    "        float l = clamp(dot(g, LUM), 0.0, 1.0);\n"
    "        g += n * uGrain * 0.08 * (0.3 + 2.8 * l * (1.0 - l));\n"
    "    }\n"
    "    outColor = vec4(max(g, 0.0), 1.0);\n"
    "}\n";

/* Glow's bright pass, at half size: linear light above 0.4 luminance and above
 * 1.25x its surroundings (a 32x32 mip average), so lamps, flashes and sky edges
 * glow while a wide bright area such as a snowfield does not haze over */
static const char* const kGradeBrightFs =
    "uniform sampler2D uScene;\n"
    "uniform vec2 uTexel;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    const vec3 LUM = vec3(0.2126, 0.7152, 0.0722);\n"
    "    vec2 uv = gl_FragCoord.xy * uTexel;\n"
    "    vec3 c = pow(texture(uScene, uv).rgb, vec3(2.2));\n"
    "    float l = dot(c, LUM);\n"
    "    float around = dot(pow(textureLod(uScene, uv, 5.0).rgb, vec3(2.2)), LUM);\n"
    "    outColor = vec4(c * (max(l - max(0.4, around * 1.25), 0.0) / max(l, 1e-4)), 1.0);\n"
    "}\n";

#include "gfx_exposure.inl" /* FORK-082: Auto Exposure */

static bool gl_grade_program(void) {
    if (gl_grade_prog != 0) return true;
    if (gl_grade_failed) return false;
    if (gl_glsl_version < 130 || glGenVertexArrays == NULL || glGenerateMipmap == NULL) {
        sysLogPrintf(LOG_WARNING, "tone mapping: needs GLSL 1.30, vertex arrays and mipmaps - off");
        gl_grade_failed = true;
        return false;
    }
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n%s", gl_glsl_version_str, gl_es ? "precision highp float;\n" : "");
    const std::string vs = std::string(hdr) + "in vec2 aPos;\nvoid main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    gl_grade_prog = vi_compile(vs.c_str(), (std::string(hdr) + kGradeFs).c_str());
    gl_grade_bright_prog = gl_grade_prog ? vi_compile(vs.c_str(), (std::string(hdr) + kGradeBrightFs).c_str()) : 0;
    if (gl_grade_bright_prog == 0) {
        gl_grade_failed = true;
        return false;
    }
    const GLuint g = gl_grade_prog;
    gl_grade_u_scene = glGetUniformLocation(g, "uScene");
    gl_grade_u_glowtex = glGetUniformLocation(g, "uGlowTex");
    gl_grade_u_hdr = glGetUniformLocation(g, "uHdr");
    gl_grade_u_exposure = glGetUniformLocation(g, "uExposure");
    gl_grade_u_expand = glGetUniformLocation(g, "uExpand");
    gl_grade_u_bright = glGetUniformLocation(g, "uBright");
    gl_grade_u_contrast = glGetUniformLocation(g, "uContrast");
    gl_grade_u_filmic = glGetUniformLocation(g, "uFilmic");
    gl_grade_u_vibrance = glGetUniformLocation(g, "uVibrance");
    gl_grade_u_clarity = glGetUniformLocation(g, "uClarity");
    gl_grade_u_sharpen = glGetUniformLocation(g, "uSharpen");
    gl_grade_u_glow = glGetUniformLocation(g, "uGlow");
    gl_grade_u_peak = glGetUniformLocation(g, "uPeak");
    gl_grade_u_split = glGetUniformLocation(g, "uSplit");
    gl_grade_u_texel = glGetUniformLocation(g, "uTexel");
    gl_grade_u_size = glGetUniformLocation(g, "uSize");
    gl_grade_u_sat = glGetUniformLocation(g, "uSat");
    gl_grade_u_tone = glGetUniformLocation(g, "uTone");
    gl_grade_u_fade = glGetUniformLocation(g, "uFade");
    gl_grade_u_vignette = glGetUniformLocation(g, "uVignette");
    gl_grade_u_grain = glGetUniformLocation(g, "uGrain");
    gl_grade_u_frame = glGetUniformLocation(g, "uFrame");
    gl_grade_u_temp = glGetUniformLocation(g, "uTemp");
    gl_grade_u_shadow = glGetUniformLocation(g, "uShadowTint");
    gl_grade_u_high = glGetUniformLocation(g, "uHighTint");
    gl_grade_u_rect = glGetUniformLocation(g, "uRect");
    gl_grade_u_adapt = glGetUniformLocation(g, "uAdapt");
    gl_grade_u_auto = glGetUniformLocation(g, "uAuto");
    gl_grade_u_keylog = glGetUniformLocation(g, "uKeyLog");
    gl_grade_u_bp_scene = glGetUniformLocation(gl_grade_bright_prog, "uScene");
    gl_grade_u_bp_texel = glGetUniformLocation(gl_grade_bright_prog, "uTexel");
    static const float quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    /* leave the renderer's VAO and buffer bound: it sets attribute pointers into
     * whatever VAO is bound, and once overwrote this quad's (half a window drew) */
    GLint had_vao = 0, had_vbo = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &had_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &had_vbo);
    glGenVertexArrays(1, &gl_grade_vao);
    glGenBuffers(1, &gl_grade_vbo);
    glBindVertexArray(gl_grade_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_grade_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    const GLint pos = glGetAttribLocation(gl_grade_prog, "aPos");
    glEnableVertexAttribArray((GLuint)pos);
    glVertexAttribPointer((GLuint)pos, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindVertexArray((GLuint)had_vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)had_vbo);
    return true;
}

/* a mipmapped colour texture with its fbo */
static bool gl_grade_target(GLuint* tex, GLuint* fbo, uint32_t w, uint32_t h, GLenum fmt) {
    if (*fbo == 0) {
        glGenFramebuffers(1, fbo);
        glGenTextures(1, tex);
    }
    glBindTexture(GL_TEXTURE_2D, *tex);
    const GLenum layout = fmt == GL_RGB8 ? GL_RGB : GL_RGBA; /* ES takes only matching combinations */
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, layout, fmt == GL_RGBA16F ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

/* the scene copy (the bound target's size and format: RGB8, or RGBA16F while
 * HDR Output runs - GLES and WebGL resolve MSAA only between identical formats)
 * and Glow's half-size buffer */
static bool gl_grade_buffers(uint32_t w, uint32_t h, GLenum fmt) {
    if (w == 0 || h == 0) return false;
    if (gl_grade_copy_fbo != 0 && w == gl_grade_w && h == gl_grade_h && fmt == gl_grade_fmt) return true;
    const bool ok = gl_grade_target(&gl_grade_copy, &gl_grade_copy_fbo, w, h, fmt) &&
                    gl_grade_target(&gl_grade_glow, &gl_grade_glow_fbo, (w + 1) / 2, (h + 1) / 2, GL_RGBA8);
    gl_grade_w = w;
    gl_grade_h = h;
    gl_grade_fmt = fmt;
    if (!ok) {
        sysLogPrintf(LOG_WARNING, "tone mapping: incomplete copy framebuffer - off");
        gl_grade_failed = true;
    }
    return ok;
}

/* copy the target, build its mips and (with Glow) the bright pass's */
static void gl_grade_prepare(GLuint draw_fbo, const Framebuffer& fb, bool glow) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, draw_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl_grade_copy_fbo);
    glBlitFramebuffer(0, 0, fb.width, fb.height, 0, 0, fb.width, fb.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, gl_grade_copy);
    glGenerateMipmap(GL_TEXTURE_2D);
    if (!glow) return;
    const uint32_t hw = (fb.width + 1) / 2, hh = (fb.height + 1) / 2;
    glBindFramebuffer(GL_FRAMEBUFFER, gl_grade_glow_fbo);
    glViewport(0, 0, hw, hh);
    glUseProgram(gl_grade_bright_prog);
    glUniform1i(gl_grade_u_bp_scene, 0);
    glUniform2f(gl_grade_u_bp_texel, 1.0f / (float)hw, 1.0f / (float)hh);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindTexture(GL_TEXTURE_2D, gl_grade_glow);
    glGenerateMipmap(GL_TEXTURE_2D);
}

static void gl_grade_uniforms(const struct GfxGradeParams* p, bool hdr, const Framebuffer& fb, GLint x0, GLint y0,
                              GLint x1, GLint y1) {
    const GLint split_x = (x0 + x1) / 2;
    glUniform1f(gl_grade_u_sat, p->saturation);
    glUniform1f(gl_grade_u_tone, p->split_tone);
    glUniform1f(gl_grade_u_fade, p->fade);
    glUniform1f(gl_grade_u_vignette, p->vignette);
    glUniform1f(gl_grade_u_grain, p->grain);
    glUniform1f(gl_grade_u_frame, (float)(frame_count % 4096u));
    glUniform3f(gl_grade_u_temp, p->temperature[0], p->temperature[1], p->temperature[2]);
    glUniform3f(gl_grade_u_shadow, p->shadow_tint[0], p->shadow_tint[1], p->shadow_tint[2]);
    glUniform3f(gl_grade_u_high, p->high_tint[0], p->high_tint[1], p->high_tint[2]);
    glUniform4f(gl_grade_u_rect, (float)(x0 + x1) * 0.5f, (float)(y0 + y1) * 0.5f, (float)(x1 - x0) * 0.5f,
                (float)(y1 - y0) * 0.5f);
    glUniform1i(gl_grade_u_scene, 0);
    glUniform1i(gl_grade_u_glowtex, 1);
    glUniform1i(gl_grade_u_hdr, hdr ? 1 : 0);
    glUniform1f(gl_grade_u_exposure, p->exposure);
    glUniform1f(gl_grade_u_expand, p->expand);
    glUniform1f(gl_grade_u_bright, p->brightness);
    glUniform1f(gl_grade_u_contrast, p->contrast);
    glUniform1f(gl_grade_u_filmic, p->filmic);
    glUniform1f(gl_grade_u_vibrance, p->vibrance);
    glUniform1f(gl_grade_u_clarity, p->clarity);
    glUniform1f(gl_grade_u_sharpen, p->sharpen);
    glUniform1f(gl_grade_u_glow, p->glow);
    glUniform1f(gl_grade_u_peak, p->peak);
    glUniform1f(gl_grade_u_split, p->split ? (float)split_x : -1.0f);
    glUniform2f(gl_grade_u_texel, 1.0f / (float)fb.width, 1.0f / (float)fb.height);
    glUniform2i(gl_grade_u_size, (GLint)fb.width, (GLint)fb.height);
}

static void gfx_opengl_grade(const struct GfxGradeParams* p) {
    if (gl_grade_failed || !gfx_framebuffers_enabled || current_framebuffer >= framebuffers.size()) return;
    if (!p->tonemap) return; /* HDR alone: the stock image already sits at paper white */
    const Framebuffer& fb = framebuffers[current_framebuffer];
    const bool hdr = gl_target_is_float(current_framebuffer);

    GLint viewport[4], scissor_box[4], active, tex0, tex1, tex2, prog, vao, vbo, read_fbo, draw_fbo, samp0 = 0,
          samp1 = 0, samp2 = 0;
    GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull = glIsEnabled(GL_CULL_FACE), stencil = glIsEnabled(GL_STENCIL_TEST), depth_mask, cmask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex2);
    if (glBindSampler != NULL) glGetIntegerv(GL_SAMPLER_BINDING, &samp2);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex1);
    if (glBindSampler != NULL) glGetIntegerv(GL_SAMPLER_BINDING, &samp1);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
    if (glBindSampler != NULL) {
        /* the browser build's sampler objects would override the mip filtering */
        glGetIntegerv(GL_SAMPLER_BINDING, &samp0);
        glBindSampler(0, 0);
        glBindSampler(1, 0);
        glBindSampler(2, 0);
    }

    /* the player's view, clipped to the target */
    GLint x0 = std::max(viewport[0], 0), y0 = std::max(viewport[1], 0);
    GLint x1 = std::min(viewport[0] + viewport[2], (GLint)fb.width), y1 = std::min(viewport[1] + viewport[3], (GLint)fb.height);
    const bool ok = x1 > x0 && y1 > y0 && gl_grade_program() && gl_grade_buffers(fb.width, fb.height, hdr ? GL_RGBA16F : GL_RGB8);
    if (ok) {
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindVertexArray(gl_grade_vao);
        gl_grade_prepare((GLuint)draw_fbo, fb, p->glow > 0.0f);
        glBindTexture(GL_TEXTURE_2D, gl_grade_copy);
        const GLuint adapt = p->auto_exposure > 0.0f ? gl_exposure_update(fb, x0, y0, x1, y1, p->adapt_speed) : 0;

        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)draw_fbo);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x0, y0, x1 - x0, y1 - y0);
        glViewport(0, 0, fb.width, fb.height);
        glUseProgram(gl_grade_prog);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, adapt);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gl_grade_glow);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gl_grade_copy);
        gl_grade_uniforms(p, hdr, fb, x0, y0, x1, y1);
        glUniform1i(gl_grade_u_adapt, 2);
        glUniform1f(gl_grade_u_auto, adapt != 0 ? p->auto_exposure : 0.0f);
        glUniform1f(gl_grade_u_keylog, kExpKeyLog2);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if (glBindSampler != NULL) {
        glBindSampler(0, (GLuint)samp0);
        glBindSampler(1, (GLuint)samp1);
        glBindSampler(2, (GLuint)samp2);
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    glActiveTexture(active);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUseProgram(prog);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
    glDepthMask(depth_mask);
    glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}
