/* FORK-066: the Glass Reflections pass. Included by gfx_opengl.cpp after
 * gfx_glint.inl. Right after a batch of glass triangles (a pane's see-through
 * triangles, a window door's window, or broken shards; G_NOOP 0x63 from
 * port_glass.c), gfx_pc.cpp hands them here and they are drawn again
 * additively (ONE, ONE), depth tested but not written, like the Gun Glint.
 * Per pixel: the view ray reflected off the face samples the level cube
 * (gfx_envcube.inl, camera-aligned faces in a 3x2 atlas, the same lookup as
 * gfx_screen_env.inl's warp), weighted by Schlick's Fresnel term, so glass
 * seen head on keeps nearly its own look and reflects more at a grazing
 * angle; an overhead light adds a Blinn-Phong highlight. The cube holds only
 * the level's rooms (no props, characters or glass) and is centred on the
 * camera, so a pane does not mirror exactly; kept light, that reads as glass.
 * Every GL state touched is restored. */

static GLuint gl_glass_prog = 0, gl_glass_vao = 0, gl_glass_vbo = 0;
static GLint gl_glass_a[6] = { -1, -1, -1, -1, -1, -1 };
static GLint gl_glass_u_cube = -1, gl_glass_u_has_cube = -1, gl_glass_u_split = -1;
static bool gl_glass_failed = false;
enum { kGlassFloats = 18 }; /* clip xyzw, eye xyz, normal xyz, light xyz, strength, f0, glint, power, lod */

static bool gl_glass_program(void) {
    if (gl_glass_prog != 0) return true;
    if (gl_glass_failed) return false;
    if (gl_glsl_version < 130 || glGenVertexArrays == NULL) {
        gl_glass_failed = true;
        return false;
    }
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "#version %s\n%s", gl_glsl_version_str, gl_es ? "precision highp float;\n" : "");
    const std::string vs = std::string(hdr) +
        "in vec4 aPos;\nin vec3 aEye;\nin vec3 aNormal;\nin vec3 aLight;\nin vec3 aLook;\nin vec2 aShape;\n"
        "out vec3 vEye;\nout vec3 vNormal;\nout vec3 vLight;\nout vec3 vLook;\nout vec2 vShape;\n"
        "void main() { vEye = aEye; vNormal = aNormal; vLight = aLight; vLook = aLook; vShape = aShape;\n"
        "              gl_Position = aPos; }\n";
    const std::string fs = std::string(hdr) +
        "uniform sampler2D uCube;\nuniform int uHasCube;\nuniform float uSplit;\n"
        "in vec3 vEye;\nin vec3 vNormal;\nin vec3 vLight;\nin vec3 vLook;\nin vec2 vShape;\nout vec4 outColor;\n"
        /* gfx_screen_env.inl's lookup: six camera-aligned faces in a 3x2 atlas */
        "vec2 cubeUv(vec3 d) {\n"
        "    vec3 a = abs(d);\n"
        "    vec3 right, up, look; float face;\n"
        "    if (a.x >= a.y && a.x >= a.z) {\n"
        "        if (d.x > 0.0) { face = 0.0; right = vec3(0,0,1); up = vec3(0,1,0); look = vec3(1,0,0); }\n"
        "        else { face = 1.0; right = vec3(0,0,-1); up = vec3(0,1,0); look = vec3(-1,0,0); }\n"
        "    } else if (a.y >= a.z) {\n"
        "        if (d.y > 0.0) { face = 2.0; right = vec3(1,0,0); up = vec3(0,0,1); look = vec3(0,1,0); }\n"
        "        else { face = 3.0; right = vec3(1,0,0); up = vec3(0,0,-1); look = vec3(0,-1,0); }\n"
        "    } else {\n"
        "        if (d.z > 0.0) { face = 4.0; right = vec3(-1,0,0); up = vec3(0,1,0); look = vec3(0,0,1); }\n"
        "        else { face = 5.0; right = vec3(1,0,0); up = vec3(0,1,0); look = vec3(0,0,-1); }\n"
        "    }\n"
        "    float l = dot(d, look);\n"
        "    vec2 st = clamp(vec2(dot(d, right), dot(d, up)) / l, -0.97, 0.97) * 0.5 + 0.5;\n"
        "    return vec2((mod(face, 3.0) + st.x) / 3.0, (floor(face / 3.0) + st.y) / 2.0);\n"
        "}\n"
        "void main() {\n"
        "    if (gl_FragCoord.x < uSplit) discard;\n"  /* glass_split: the left half stays stock */
        "    vec3 v = normalize(vEye);\n"     /* eye to surface; the camera looks down -z */
        "    vec3 n = normalize(vNormal);\n"  /* turned towards the viewer by gfx_pc.cpp */
        "    float c = clamp(dot(n, -v), 0.0, 1.0);\n"
        "    float f0 = vLook.y;\n"
        "    float fresnel = f0 + (1.0 - f0) * pow(1.0 - c, 5.0);\n"
        "    vec3 col = vec3(0.0);\n"
        "    if (uHasCube != 0) col += textureLod(uCube, cubeUv(reflect(v, n)), vShape.y).rgb * fresnel * vLook.x;\n"
        "    vec3 h = normalize(normalize(vLight) - v);\n"
        "    col += vec3(pow(max(dot(n, h), 0.0), vShape.x) * vLook.z);\n"
        "    outColor = vec4(col, 1.0);\n"
        "}\n";
    gl_glass_prog = vi_compile(vs.c_str(), fs.c_str());
    if (gl_glass_prog == 0) {
        gl_glass_failed = true;
        return false;
    }
    static const char* const names[6] = { "aPos", "aEye", "aNormal", "aLight", "aLook", "aShape" };
    for (int i = 0; i < 6; i++) gl_glass_a[i] = glGetAttribLocation(gl_glass_prog, names[i]);
    gl_glass_u_cube = glGetUniformLocation(gl_glass_prog, "uCube");
    gl_glass_u_has_cube = glGetUniformLocation(gl_glass_prog, "uHasCube");
    gl_glass_u_split = glGetUniformLocation(gl_glass_prog, "uSplit");
    glGenVertexArrays(1, &gl_glass_vao);
    glGenBuffers(1, &gl_glass_vbo);
    return true;
}

static void gfx_opengl_glass_draw(const float* buf, size_t vertex_count, bool cube, bool split) {
    if (vertex_count == 0) return;
    GLint prog, vao, vbo, src_rgb, dst_rgb, src_a, dst_a, eq_rgb, eq_a, depth_func, active, tex2;
    GLboolean blend = glIsEnabled(GL_BLEND), depth_mask;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo);
    if (!gl_glass_program()) return;
    glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_a);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_a);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eq_a);
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);

    const bool has_cube = cube && gl_cube_tex != 0;
    glUseProgram(gl_glass_prog);
    glActiveTexture(GL_TEXTURE2); /* units 0 and 1 keep the batch's textures */
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex2);
    glBindTexture(GL_TEXTURE_2D, has_cube ? gl_cube_tex : 0);
    glUniform1i(gl_glass_u_cube, 2);
    glUniform1i(gl_glass_u_has_cube, has_cube ? 1 : 0);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glUniform1f(gl_glass_u_split, split ? (float)viewport[0] + 0.5f * (float)viewport[2] : -1.0f);
    glBindVertexArray(gl_glass_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl_glass_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kGlassFloats * vertex_count, buf, GL_STREAM_DRAW);
    static const int sizes[6] = { 4, 3, 3, 3, 3, 2 };
    size_t offset = 0;
    for (int i = 0; i < 6; i++) {
        if (gl_glass_a[i] >= 0) {
            glEnableVertexAttribArray(gl_glass_a[i]);
            glVertexAttribPointer(gl_glass_a[i], sizes[i], GL_FLOAT, GL_FALSE, kGlassFloats * sizeof(float),
                                  (void*)(offset * sizeof(float)));
        }
        offset += sizes[i];
    }
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL); /* the batch may have written these depths */
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);

    glBindTexture(GL_TEXTURE_2D, (GLuint)tex2);
    glActiveTexture((GLenum)active);
    glDepthFunc((GLenum)depth_func);
    glDepthMask(depth_mask);
    glBlendEquationSeparate(eq_rgb, eq_a);
    glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
    if (!blend) glDisable(GL_BLEND);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUseProgram(prog);
}
