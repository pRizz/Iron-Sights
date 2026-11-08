/* FORK-058: the Gun Glint pass. Included by gfx_opengl.cpp after
 * gfx_screen_env.inl. The ordinary guns are prelit (no vertex normals) and their
 * metal textures are nearly black, while the game draws
 * texture x shade, so a highlight added to the shade could not show; instead,
 * right after a metal batch, gfx_pc.cpp hands its triangles here and they are
 * drawn again additively (ONE, ONE), depth tested but not written. The
 * highlight is Blinn-Phong per pixel: per corner the camera-space position,
 * the face normal and the light direction come in, so a flat face shows a
 * spot where the light's reflection meets the eye, sliding as the angle
 * changes (one value per face washed whole faces grey). Every GL state
 * touched is restored. */

static GLuint gl_glint_prog = 0, gl_glint_vao = 0, gl_glint_vbo = 0;
static GLint gl_glint_a[6] = { -1, -1, -1, -1, -1, -1 };
static bool gl_glint_failed = false;
enum { kGlintFloats = 15 }; /* clip xyzw, eye xyz, normal xyz, light xyz, strength, power */

static bool gl_glint_program(void) {
    if (gl_glint_prog != 0) return true;
    if (gl_glint_failed) return false;
    if (gl_glsl_version < 130 || glGenVertexArrays == NULL) {
        gl_glint_failed = true;
        return false;
    }
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n%s", gl_glsl_version_str, gl_es ? "precision highp float;\n" : "");
    const std::string vs = std::string(hdr) +
        "in vec4 aPos;\nin vec3 aEye;\nin vec3 aNormal;\nin vec3 aLight;\nin vec2 aShape;\n"
        "out vec3 vEye;\nout vec3 vNormal;\nout vec3 vLight;\nout vec2 vShape;\n"
        "void main() { vEye = aEye; vNormal = aNormal; vLight = aLight; vShape = aShape; gl_Position = aPos; }\n";
    const std::string fs = std::string(hdr) +
        "in vec3 vEye;\nin vec3 vNormal;\nin vec3 vLight;\nin vec2 vShape;\nout vec4 outColor;\n"
        "void main() {\n"
        "    vec3 h = normalize(normalize(vLight) + normalize(-vEye));\n"
        "    float s = pow(max(dot(normalize(vNormal), h), 0.0), vShape.y) * vShape.x;\n"
        "    outColor = vec4(vec3(s), 1.0);\n"
        "}\n";
    gl_glint_prog = vi_compile(vs.c_str(), fs.c_str());
    if (gl_glint_prog == 0) {
        gl_glint_failed = true;
        return false;
    }
    static const char* const names[5] = { "aPos", "aEye", "aNormal", "aLight", "aShape" };
    for (int i = 0; i < 5; i++) gl_glint_a[i] = glGetAttribLocation(gl_glint_prog, names[i]);
    glGenVertexArrays(1, &gl_glint_vao);
    glGenBuffers(1, &gl_glint_vbo);
    return true;
}

static void gfx_opengl_glint_draw(const float* buf, size_t vertex_count) {
    if (vertex_count == 0) return;
    GLint prog, vao, vbo, src_rgb, dst_rgb, src_a, dst_a, eq_rgb, eq_a, depth_func;
    GLboolean blend = glIsEnabled(GL_BLEND), depth_mask;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    if (!gl_glint_program()) return;
    glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_a);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_a);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eq_a);
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);

    glUseProgram(gl_glint_prog);
    glBindVertexArray(gl_glint_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_glint_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kGlintFloats * vertex_count, buf, GL_STREAM_DRAW);
    static const int sizes[5] = { 4, 3, 3, 3, 2 };
    size_t offset = 0;
    for (int i = 0; i < 5; i++) {
        if (gl_glint_a[i] >= 0) {
            glEnableVertexAttribArray(gl_glint_a[i]);
            glVertexAttribPointer(gl_glint_a[i], sizes[i], GL_FLOAT, GL_FALSE, kGlintFloats * sizeof(float),
                                  (void*)(offset * sizeof(float)));
        }
        offset += sizes[i];
    }
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL); /* the batch just wrote these depths */
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);

    glDepthFunc((GLenum)depth_func);
    glDepthMask(depth_mask);
    glBlendEquationSeparate(eq_rgb, eq_a);
    glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
    if (!blend) glDisable(GL_BLEND);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUseProgram(prog);
}
