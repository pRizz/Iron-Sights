/* FORK-058 prototype (shiny reflections candidate 4): a camera-centred cube map
 * of the level, rendered from the room cache's retained GPU geometry
 * (gfx_roomcache.inl), for the shiny guns' scene reflection in place of the
 * screen copy. Included by gfx_pc.cpp after gfx_roomcache.inl. With
 * room_cache = 0, Richer Reflections runs the cache in capture-only mode 3
 * (roc_hooks.c): rooms are recorded once, never replayed, and the frame draws
 * as before; room_cache = 2 shares its captures.
 *
 * Placement: GoldenEye's room modelview is only the room's offset; the
 * camera's rotation and position ride in its projection (bondview2.c), which
 * roc_env_note splits from the perspective. Each room's full camera-space
 * modelview is then MV = M x V (row vectors: the room's model transform, then
 * the camera's view). Rooms are kept in one
 * reference space: the first room seen seeds it (M = its MV, V = identity);
 * whenever a room with a known M draws, that frame's V = M^-1 x MV, and every
 * other room drawn that frame gets M = MV x V^-1. A room not drawn this frame
 * (behind the player, out of view) still places as M x V with this frame's V.
 * A frame with no known room reseeds the space.
 *
 * Faces are camera-aligned (the reflection lookup works in camera axes) and
 * share the game's own near and far planes (from its projection), so the
 * game's fog parameters mean the same distances. Lit and translucent draws,
 * props and characters are not drawn; empty space shows the level's sky colour
 * (G_NOOP 0x60 from port_shiny.c). GE_ENVCUBE_DUMP=n writes the n-th cube's
 * atlas as envcube.bmp. */

#include <chrono>

static float roc_env_model[ROC_MAX_ROOMS][4][4];
static uint32_t roc_env_known[ROC_MAX_ROOMS]; /* == roc_env_gen when the room's M is known */
static uint32_t roc_env_gen = 1;
static float roc_env_view[4][4];   /* this frame's V (reference space -> camera space) */
static bool roc_env_view_valid = false;
static int roc_env_pending[64];    /* rooms drawn this frame before V was known */
static float roc_env_pending_mv[64][4][4];
static int roc_env_pending_count = 0;
/* the frame's fog and projection planes, from the last replayed room */
static float roc_env_fogp[4], roc_env_fog_rgb[3], roc_env_near = 4.0f, roc_env_far = 40000.0f;
static uint32_t roc_env_rooms_drawn = 0;
/* FORK-066: the frame being drawn, and the cube built in it (size, result):
 * the glass spans and the shiny gun share one cube per frame */
static uint32_t roc_env_frame_no = 1, roc_env_cube_frame = 0;
static int roc_env_cube_size = 0;
static bool roc_env_cube_ok = false;
static float roc_env_cam_t[3]; /* diagnostics: the translation found in the projection */

static void roc_env_mul(float out[4][4], const float a[4][4], const float b[4][4]) {
    float t[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(out, t, sizeof(t));
}

/* a general 4x4 inverse (cofactors); false when singular */
static bool roc_env_invert(float out[4][4], const float m[4][4]) {
    const float* a = &m[0][0];
    float inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    const float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    if (fabsf(det) < 1e-20f) return false;
    for (int i = 0; i < 16; i++) (&out[0][0])[i] = inv[i] / det;
    return true;
}

/* roc_replay_gpu: room ri drew with this modelview, fog and projection */
static void roc_env_note(int ri) {
    if (ri < 0 || ri >= ROC_MAX_ROOMS) return;
    const float (*mv_room)[4] = rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1];
    roc_env_rooms_drawn++;
    roc_env_fogp[0] = (float)rsp.fog_mul / 255.0f;
    roc_env_fogp[1] = (float)rsp.fog_offset / 255.0f;
    roc_env_fogp[2] = 0.001f;
    roc_env_fogp[3] = rdp.fog_color.a / 255.0f;
    roc_env_fog_rgb[0] = rdp.fog_color.r / 255.0f;
    roc_env_fog_rgb[1] = rdp.fog_color.g / 255.0f;
    roc_env_fog_rgb[2] = rdp.fog_color.b / 255.0f;
    /* GoldenEye loads the camera's rotation into the projection (bondview2.c,
     * bg.c): C = R x Pp, with Pp = diag(sx, sy, A, 0), Pp[2][3] = -1, Pp[3][2] = B,
     * and the room's modelview is only its offset from the camera. Split R out:
     * column 3 of C is -R's column 2, columns 0 and 1 are R's scaled by sx, sy. */
    float mv[4][4];
    {
        const float (*c)[4] = rsp.P_matrix;
        float rot[4][4] = {};
        float sx = 0.0f, sy = 0.0f, a = 0.0f;
        for (int i = 0; i < 3; i++) {
            sx += c[i][0] * c[i][0];
            sy += c[i][1] * c[i][1];
        }
        sx = sqrtf(sx);
        sy = sqrtf(sy);
        bool ok = sx > 1e-6f && sy > 1e-6f;
        for (int i = 0; ok && i < 3; i++) {
            rot[i][0] = c[i][0] / sx;
            rot[i][1] = c[i][1] / sy;
            rot[i][2] = -c[i][3];
            a += c[i][2] * rot[i][2];
        }
        /* any camera translation t rides in row 3: C[3] = (t0 sx, t1 sy, t2 A + B, -t2) */
        if (ok) {
            rot[3][0] = c[3][0] / sx;
            rot[3][1] = c[3][1] / sy;
            rot[3][2] = -c[3][3];
        }
        rot[3][3] = 1.0f;
        const float b = c[3][2] - rot[3][2] * a;
        roc_env_cam_t[0] = rot[3][0];
        roc_env_cam_t[1] = rot[3][1];
        roc_env_cam_t[2] = rot[3][2];
        if (ok && fabsf(a - 1.0f) > 1e-6f && fabsf(a + 1.0f) > 1e-6f) {
            const float n = b / (a - 1.0f), f = b / (a + 1.0f);
            if (n > 0.0f && f > n) {
                roc_env_near = n;
                roc_env_far = f;
            }
        }
        if (!ok) {
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) rot[i][j] = i == j ? 1.0f : 0.0f;
            }
        }
        roc_env_mul(mv, mv_room, rot); /* the full camera-space modelview */
    }
    float inv[4][4];
    if (!roc_env_view_valid && roc_env_known[ri] == roc_env_gen && roc_env_invert(inv, roc_env_model[ri])) {
        roc_env_mul(roc_env_view, inv, mv); /* V = M^-1 x MV */
        roc_env_view_valid = true;
        float vinv[4][4];
        if (roc_env_invert(vinv, roc_env_view)) {
            for (int p = 0; p < roc_env_pending_count; p++) {
                roc_env_mul(roc_env_model[roc_env_pending[p]], roc_env_pending_mv[p], vinv);
                roc_env_known[roc_env_pending[p]] = roc_env_gen;
            }
        }
        roc_env_pending_count = 0;
    }
    if (roc_env_view_valid) {
        float vinv[4][4];
        if (roc_env_invert(vinv, roc_env_view)) {
            roc_env_mul(roc_env_model[ri], mv, vinv); /* M = MV x V^-1 */
            roc_env_known[ri] = roc_env_gen;
        }
    } else if (roc_env_pending_count < 64) {
        roc_env_pending[roc_env_pending_count] = ri;
        memcpy(roc_env_pending_mv[roc_env_pending_count], mv, sizeof(roc_env_pending_mv[0]));
        roc_env_pending_count++;
    }
}

static void roc_env_frame_reset(void) {
    roc_env_frame_no++; /* FORK-066: this frame's cube is not built yet */
    roc_env_view_valid = false;
    roc_env_pending_count = 0;
    roc_env_rooms_drawn = 0;
}

/* no known room drew this frame: start a new reference space from the first pending room */
static void roc_env_reseed(void) {
    if (roc_env_view_valid || roc_env_pending_count == 0) return;
    roc_env_gen++;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) roc_env_view[i][j] = i == j ? 1.0f : 0.0f;
    }
    roc_env_view_valid = true;
    for (int p = 0; p < roc_env_pending_count; p++) {
        memcpy(roc_env_model[roc_env_pending[p]], roc_env_pending_mv[p], sizeof(roc_env_model[0]));
        roc_env_known[roc_env_pending[p]] = roc_env_gen;
    }
    roc_env_pending_count = 0;
}

/* the tag decode, at a shiny weapon span's start: render the cube; false when
 * it could not (no cached room placed this frame) */
static bool roc_env_cube(int size, const uint8_t sky[3]) {
    if (!gfx_rapi->env_cube_begin || gfx_room_cache != 2 || roc_stage < 0 || roc_stage == 0x5A) return false;
    if (roc_env_cube_frame == roc_env_frame_no && roc_env_cube_size == size) {
        return roc_env_cube_ok; /* FORK-066: built earlier this frame (a glass span or the gun) */
    }
    roc_env_reseed();
    if (!roc_env_view_valid) return false; /* no room placed yet: a later span may try again */
    roc_env_cube_frame = roc_env_frame_no;
    roc_env_cube_size = size;
    roc_env_cube_ok = false;
    /* face bases in camera space: right, up, back (each face looks down -back);
     * gfx_screen_env.inl samples with the same table */
    static const float faces[6][3][3] = {
        { { 0, 0, 1 }, { 0, 1, 0 }, { -1, 0, 0 } },   /* 0 +x: right */
        { { 0, 0, -1 }, { 0, 1, 0 }, { 1, 0, 0 } },   /* 1 -x: left */
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, -1, 0 } },   /* 2 +y: up */
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },   /* 3 -y: down */
        { { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } },  /* 4 +z: behind */
        { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } },    /* 5 -z: ahead */
    };
    const float n = roc_env_near, f = roc_env_far;
    const float proj[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, (n + f) / (n - f), -1 },
                               { 0, 0, 2 * n * f / (n - f), 0 } };
    static const float sel_gpu[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    static const float sel_vertex[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    static const float nodecal[4] = { 0, 0, 0, 0 };
    static int dump_at = -2, cubes = 0;
    if (dump_at == -2) {
        const char* e = getenv("GE_ENVCUBE_DUMP");
        dump_at = e != NULL ? atoi(e) : -1;
    }
    cubes++;
    gfx_flush();
    const auto t0 = std::chrono::steady_clock::now();
    gfx_rapi->env_cube_begin(size, sky[0] / 255.0f, sky[1] / 255.0f, sky[2] / 255.0f);
    uint32_t rooms = 0, skipped = 0, unplaced = 0;
    uint32_t n_lit = 0, n_xlu = 0, n_nodesc = 0, n_notex = 0, n_drawn = 0;
    /* the six faces' rotation x projection */
    float face_proj[6][4][4];
    for (int face = 0; face < 6; face++) {
        float rot[4][4] = {};
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) rot[i][j] = faces[face][j][i];
        }
        rot[3][3] = 1.0f;
        roc_env_mul(face_proj[face], rot, proj);
    }
    for (int ri = 0; ri < ROC_MAX_ROOMS; ri++) {
        RocRoom& r = roc_rooms[ri];
        if (r.state != 2 || r.gpu_buffer < 0 || r.kind != 0) continue;
        if (roc_env_known[ri] != roc_env_gen) {
            unplaced++;
            continue;
        }
        bool live = roc_textures_still_live(r);
        if (!live) { /* as roc_walk_begin: every draw's textures, or skip the room */
            live = true;
            for (size_t di = 0; live && di < r.draws.size(); di++) {
                for (int t = 0; t < 2 && live; t++) live = roc_validate_draw_tex(r, r.draws[di], t);
            }
            if (live) {
                r.tex_gen = ge_texcache_gen;
                r.del_gen = ge_texcache_delete_gen;
                r.pinned = false;
                roc_pin(r, true);
            }
        }
        if (!live) {
            skipped++;
            continue;
        }
        rooms++;
        /* the room's modelview this frame, from its reference-space placement, per face */
        float mv[4][4], mvp[6][16];
        roc_env_mul(mv, roc_env_model[ri], roc_env_view);
        for (int face = 0; face < 6; face++) {
            float mvp4[4][4];
            roc_env_mul(mvp4, mv, face_proj[face]);
            memcpy(mvp[face], mvp4, sizeof(mvp4));
        }
        for (size_t di = 0; di < r.draws.size(); di++) {
            const RocDraw& d = r.draws[di];
            if (d.lit || d.xlu || d.gpu_desc < 0) {
                (d.lit ? n_lit : d.xlu ? n_xlu : n_nodesc)++;
                continue;
            }
            if ((d.state.tex_used[0] && d.tex_node[0] == nullptr) || (d.state.tex_used[1] && d.tex_node[1] == nullptr)) {
                n_notex++;
                continue;
            }
            n_drawn++;
            roc_apply_state(d); /* once per draw: the faces differ only in viewport and matrix */
            const float fx[4] = { 1.0f, 0.0f, d.z01 ? 0.5f : 1.0f, d.z01 ? 0.5f : 0.0f };
            for (int face = 0; face < 6; face++) {
                gfx_rapi->env_cube_face(face);
                gfx_rapi->retained_draw(d.gpu_desc, mvp[face], fx, roc_env_fogp, d.fog_on ? sel_gpu : sel_vertex,
                                        roc_env_fog_rgb, d.cull_mode, NULL, NULL, nodecal);
            }
        }
    }
    gfx_rapi->env_cube_end(cubes == dump_at ? "envcube.bmp" : NULL);
    /* the interpreter re-derives everything on its next triangle */
    ge_state_serial++;
    rdp.textures_changed[0] = rdp.textures_changed[1] = true;
    rdp.viewport_or_scissor_changed = true;
    memset(&rendering_state.viewport, 0xff, sizeof(rendering_state.viewport));
    memset(&rendering_state.scissor, 0xff, sizeof(rendering_state.scissor));
    static double cpu_ms = 0.0;
    cpu_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (gfx_shiny::tuning().log && cubes % 60 == 0) {
        fprintf(stderr, "envcube: CPU %.3f ms per cube (average of 60)\n", cpu_ms / 60.0);
    }
    if (cubes % 60 == 0) cpu_ms = 0.0;
    if (gfx_shiny::tuning().log && (cubes % 120 == 1 || cubes == dump_at)) {
        fprintf(stderr, "envcube %d: %u rooms placed and drawn, %u unplaced, %u without textures; "
                        "%u drew this frame; near %.1f far %.0f; draws %u (skipped: lit %u, xlu %u, no desc %u, "
                        "no tex %u); projection translation %.1f %.1f %.1f\n",
                cubes, rooms, unplaced, skipped, roc_env_rooms_drawn, roc_env_near, roc_env_far, n_drawn, n_lit,
                n_xlu, n_nodesc, n_notex, roc_env_cam_t[0], roc_env_cam_t[1], roc_env_cam_t[2]);
    }
    roc_env_cube_ok = rooms > 0;
    return roc_env_cube_ok;
}
