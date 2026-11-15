/* FORK-082: Auto Exposure (eye adaptation) for the Tone Mapping grade.
 * Included by gfx_grade.inl; runs inside gfx_opengl_grade with its GL state
 * saved and the scene copy (mipmapped) on texture unit 0.
 *
 * Metering: the player's view is drawn into a 64x64 float buffer as
 * (luminance x weight, weight) in linear light, sampled from the copy's mips,
 * weighted 1.5 at the centre down to 0.5 at the edges; its 1x1 mip is the
 * weighted mean. The plain mean, not the log mean: GoldenEye's dark textures
 * dragged a log mean down by 1-2 stops more, so bright snow barely registered
 * against them.
 * Adaptation: a 1x1 float per player view (the grade's call order in the
 * frame, up to 4 for split screen), ping-ponged, holds the adapted log2
 * luminance; each frame it moves toward the metered value by
 * 1 - exp(-dt * rate): rate 2.5/s when the scene got brighter (about 0.4 s,
 * glare is corrected quickly), 0.6/s when darker (about 1.7 s), times
 * Adaptation Speed. A gap of more than a second (a level start, a menu)
 * resets it to the metered value. Nothing is read back to the CPU.
 * The grade then shifts exposure by (key - adapted) stops, clamped to
 * -2 .. +0.25 and scaled by Auto Exposure; the key, log2 0.125, sits between
 * the measured centre-weighted means of the dark levels (Dam, Bunker,
 * Caverns: -4.1 .. -4.7) and Surface's snow (-1.5), so snow comes down by
 * about a stop at 60% while dark levels lift a quarter stop at most. */

#define GL_EXP_SLOTS 4
#define GL_EXP_METER 64 /* 7 mip levels: level 6 is 1x1 */
static const float kExpKeyLog2 = -3.0f;

static bool gl_exp_failed = false;
static GLuint gl_exp_meter_prog = 0, gl_exp_adapt_prog = 0, gl_exp_meter_tex = 0, gl_exp_meter_fbo = 0;
static GLuint gl_exp_tex[GL_EXP_SLOTS][2], gl_exp_fbo[GL_EXP_SLOTS][2];
static int gl_exp_cur[GL_EXP_SLOTS];
static Uint64 gl_exp_last[GL_EXP_SLOTS];
static uint32_t gl_exp_frame = ~0u;
static int gl_exp_calls = 0;
static GLint gl_exp_u_scene = -1, gl_exp_u_rect = -1, gl_exp_u_lod = -1, gl_exp_u_meter = -1, gl_exp_u_prev = -1,
             gl_exp_u_dt = -1, gl_exp_u_speed = -1, gl_exp_u_reset = -1;

/* a full-view triangle from gl_VertexID: no attributes, so any VAO will do */
static const char* const kExpVs =
    "void main() {\n"
    "    vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID >> 1) * 4 - 1));\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";
static const char* const kExpMeterFs =
    "uniform sampler2D uScene;\n"
    "uniform vec4 uRectUv;\n"
    "uniform float uLod;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    vec2 f = gl_FragCoord.xy / 64.0;\n"
    "    vec3 c = pow(textureLod(uScene, uRectUv.xy + f * uRectUv.zw, uLod).rgb, vec3(2.2));\n"
    "    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "    vec2 d = f - 0.5;\n"
    "    float w = 1.5 - min(dot(d, d) * 4.0, 1.0);\n"
    "    outColor = vec4(l * w, w, 0.0, 1.0);\n"
    "}\n";
static const char* const kExpAdaptFs =
    "uniform sampler2D uMeter;\n"
    "uniform sampler2D uPrev;\n"
    "uniform float uDt, uSpeed;\n"
    "uniform int uReset;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    vec2 m = textureLod(uMeter, vec2(0.5), 6.0).rg;\n"
    "    float target = log2(max(m.x / max(m.y, 1e-4), 1e-4));\n"
    "    float prev = texelFetch(uPrev, ivec2(0), 0).r;\n"
    "    float rate = (target > prev ? 2.5 : 0.6) * uSpeed;\n"
    "    float a = uReset != 0 ? target : prev + (target - prev) * (1.0 - exp(-uDt * rate));\n"
    "    outColor = vec4(a, target, 0.0, 1.0);\n"
    "}\n";

static bool gl_exp_texture(GLuint* tex, GLuint* fbo, int size, bool mips) {
    glGenTextures(1, tex);
    glGenFramebuffers(1, fbo);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (mips) glGenerateMipmap(GL_TEXTURE_2D);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

static bool gl_exposure_setup(void) {
    if (gl_exp_adapt_prog != 0) return true;
    if (gl_exp_failed) return false;
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n%s", gl_glsl_version_str, gl_es ? "precision highp float;\n" : "");
    const std::string vs = std::string(hdr) + kExpVs;
    gl_exp_meter_prog = vi_compile(vs.c_str(), (std::string(hdr) + kExpMeterFs).c_str());
    gl_exp_adapt_prog = gl_exp_meter_prog ? vi_compile(vs.c_str(), (std::string(hdr) + kExpAdaptFs).c_str()) : 0;
    bool ok = gl_exp_adapt_prog != 0 && gl_exp_texture(&gl_exp_meter_tex, &gl_exp_meter_fbo, GL_EXP_METER, true);
    for (int s = 0; ok && s < GL_EXP_SLOTS; s++) {
        ok = gl_exp_texture(&gl_exp_tex[s][0], &gl_exp_fbo[s][0], 1, false) &&
             gl_exp_texture(&gl_exp_tex[s][1], &gl_exp_fbo[s][1], 1, false);
    }
    if (!ok) {
        sysLogPrintf(LOG_WARNING, "auto exposure: float render targets unavailable - off");
        gl_exp_failed = true;
        gl_exp_adapt_prog = 0;
        return false;
    }
    gl_exp_u_scene = glGetUniformLocation(gl_exp_meter_prog, "uScene");
    gl_exp_u_rect = glGetUniformLocation(gl_exp_meter_prog, "uRectUv");
    gl_exp_u_lod = glGetUniformLocation(gl_exp_meter_prog, "uLod");
    gl_exp_u_meter = glGetUniformLocation(gl_exp_adapt_prog, "uMeter");
    gl_exp_u_prev = glGetUniformLocation(gl_exp_adapt_prog, "uPrev");
    gl_exp_u_dt = glGetUniformLocation(gl_exp_adapt_prog, "uDt");
    gl_exp_u_speed = glGetUniformLocation(gl_exp_adapt_prog, "uSpeed");
    gl_exp_u_reset = glGetUniformLocation(gl_exp_adapt_prog, "uReset");
    return true;
}

/* meter the view (x0..x1, y0..y1 of fb) and adapt; returns the 1x1 texture
 * holding the adapted log2 luminance, or 0. Leaves units 0 and 2 changed:
 * the caller rebinds its own textures. */
static GLuint gl_exposure_update(const Framebuffer& fb, GLint x0, GLint y0, GLint x1, GLint y1, float speed) {
    if (!gl_exposure_setup()) return 0;
    if (gl_exp_frame != frame_count) {
        gl_exp_frame = frame_count;
        gl_exp_calls = 0;
    }
    const int slot = gl_exp_calls < GL_EXP_SLOTS ? gl_exp_calls : GL_EXP_SLOTS - 1;
    gl_exp_calls++;
    const Uint64 now = SDL_GetPerformanceCounter();
    double dt = gl_exp_last[slot] ? (double)(now - gl_exp_last[slot]) / (double)SDL_GetPerformanceFrequency() : 1e9;
    gl_exp_last[slot] = now;
    const bool reset = dt > 1.0;
    if (dt > 0.25) dt = 0.25;

    const float w = (float)(x1 - x0), h = (float)(y1 - y0);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_exp_meter_fbo);
    glViewport(0, 0, GL_EXP_METER, GL_EXP_METER);
    glUseProgram(gl_exp_meter_prog);
    glUniform1i(gl_exp_u_scene, 0);
    glUniform4f(gl_exp_u_rect, (float)x0 / (float)fb.width, (float)y0 / (float)fb.height, w / (float)fb.width,
                h / (float)fb.height);
    glUniform1f(gl_exp_u_lod, std::max(0.0f, std::log2(std::max(w, h) / (float)GL_EXP_METER)));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, gl_exp_meter_tex);
    glGenerateMipmap(GL_TEXTURE_2D);

    const int src = gl_exp_cur[slot], dst = 1 - src;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gl_exp_tex[slot][src]);
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, gl_exp_fbo[slot][dst]);
    glViewport(0, 0, 1, 1);
    glUseProgram(gl_exp_adapt_prog);
    glUniform1i(gl_exp_u_meter, 0);
    glUniform1i(gl_exp_u_prev, 2);
    glUniform1f(gl_exp_u_dt, (float)dt);
    glUniform1f(gl_exp_u_speed, speed);
    glUniform1i(gl_exp_u_reset, reset ? 1 : 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    gl_exp_cur[slot] = dst;
    return gl_exp_tex[slot][dst];
}
