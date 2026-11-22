/* Baked Lighting (FORK-082): the level side. port_graphics.h has the rules.
 *
 * Capture: load_bg_file already loads every room once at stage start
 * (bgRoomCalcBB); portBakeCaptureRoom copies each room's vertices (room-bounds
 * space, the file's colours) and walks its opaque and translucent lists for
 * triangles (G_VTX slot cache, G_TRI1 / G_TRI4): opaque triangles become
 * occluders, both kinds feed the area-weighted vertex normals. Rooms keep their
 * vertex order, so a room's colours map 1:1 onto its stream-in.
 *
 * Bake: with Baked Lighting on, portBakeLevelLoaded (or turning it on later)
 * starts port_baked_trace.c. Until it finishes the level shows its stock
 * colours; then every loaded room is refreshed, and rooms streaming in later
 * take the result in portBakeApplyRoom (bg.c, before the shot-light redarken,
 * so a shot-out light still darkens on top). Strength blends the file colour
 * toward the bake in linear light (100% = the bake, 200% = twice the change),
 * so changing it needs no new bake. Turning it off writes the file colours
 * back. Every rewrite reapplies the room's shot-out lights and drops the
 * room cache's copy. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <gbi_extension.h>
#include "game/bg.h"
#include "port_console.h"
#include "port_gfxwords.h"
#include "port_baked_light.h"
#include "port_graphics.h"

extern void portRoomCacheRoomFreed(s32 room);
extern void redarken_lights_in_room(s32 room_index);
extern coord3d *bondviewGetCurrentPlayersPosition(void);

static struct BakeLevel s_lvl;
static uint32_t s_cap_verts, s_cap_tris;
static uint32_t s_room_first[MAXROOMCOUNT], s_room_count[MAXROOMCOUNT];
static float* s_area; /* capture only: normal accumulation */
static s32 s_started, s_ready, s_dirty;
static s32 s_applied[MAXROOMCOUNT]; /* the strength now in the room's colours, 0 = file colours */
static float s_lin[256];
static char s_status[32];

static void* grow_array(void* p, uint32_t* cap, uint32_t need, size_t elem) {
    uint32_t c = *cap;
    if (need <= c) return p;
    while (c < need) c = c ? c * 2 : 4096;
    *cap = c;
    return realloc(p, (size_t) c * elem);
}

static void free_level(void) {
    free(s_lvl.pos);
    free(s_lvl.nrm);
    free(s_lvl.orig);
    free(s_lvl.baked);
    free(s_lvl.tris);
    free(s_area);
    memset(&s_lvl, 0, sizeof(s_lvl));
    s_area = NULL;
    s_cap_verts = s_cap_tris = 0;
    memset(s_room_count, 0, sizeof(s_room_count));
    memset(s_applied, 0, sizeof(s_applied));
    s_started = s_ready = s_dirty = 0;
}

void portBakeLevelUnload(void) {
    portBakeTraceCancel();
    free_level();
}

void portBakeLevelBegin(void) {
    portBakeLevelUnload();
    portBakeLinTable(s_lin);
}

/* ---- capture -------------------------------------------------------------- */

static void add_triangle(uint32_t first, uint32_t count, s32 a, s32 b, s32 c, s32 occluder) {
    if (a < 0 || b < 0 || c < 0 || (uint32_t) a >= count || (uint32_t) b >= count || (uint32_t) c >= count) return;
    const uint32_t v[3] = { first + (uint32_t) a, first + (uint32_t) b, first + (uint32_t) c };
    const float *p0 = &s_lvl.pos[v[0] * 3], *p1 = &s_lvl.pos[v[1] * 3], *p2 = &s_lvl.pos[v[2] * 3];
    const float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
    const float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
    const float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
    for (int j = 0; j < 3; j++) { /* cross product length = twice the area: area weighting for free */
        s_area[v[j] * 3 + 0] += n[0];
        s_area[v[j] * 3 + 1] += n[1];
        s_area[v[j] * 3 + 2] += n[2];
    }
    if (!occluder || (n[0] == 0.0f && n[1] == 0.0f && n[2] == 0.0f)) return;
    s_lvl.tris = (uint32_t*) grow_array(s_lvl.tris, &s_cap_tris, s_lvl.ntris + 1, sizeof(uint32_t) * 3);
    memcpy(&s_lvl.tris[s_lvl.ntris * 3], v, sizeof(v));
    s_lvl.ntris++;
}

static void walk_list(const Gfx* gdl, uint32_t max_cmds, uint32_t first, uint32_t count, s32 occluder) {
    s32 slot[16];
    for (int i = 0; i < 16; i++) slot[i] = -1;
    for (uint32_t c = 0; gdl && c < max_cmds; c++, gdl++) {
        const u32 w0 = GFX_W0(gdl), w1 = GFX_W1_DATA(gdl);
        const s8 op = (s8) (w0 >> 24);
        if (op == (s8) G_ENDDL) break;
        if (op == (s8) G_VTX) {
            const s32 v0 = (s32) ((w0 >> 16) & 0xF), n = (s32) ((w0 & 0xFFFF) / 16), base = (s32) ((w1 & 0xFFFFFF) / 16);
            for (s32 i = 0; i < n && v0 + i < 16; i++) slot[v0 + i] = base + i;
        } else if (op == (s8) G_TRI1) {
            const u32 a = ((w1 >> 16) & 0xFF) / 10, b = ((w1 >> 8) & 0xFF) / 10, d = (w1 & 0xFF) / 10;
            if (a < 16 && b < 16 && d < 16) add_triangle(first, count, slot[a], slot[b], slot[d], occluder);
        } else if (op == (s8) G_TRI4) {
            for (int k = 0; k < 4; k++) {
                const u32 x = (w1 >> (8 * k)) & 0xF, y = (w1 >> (8 * k + 4)) & 0xF, z = (w0 >> (4 * k)) & 0xF;
                if ((x | y | z) != 0) add_triangle(first, count, slot[x], slot[y], slot[z], occluder);
            }
        }
    }
}

void portBakeCaptureRoom(s32 room) {
    if (room <= 0 || room >= MAXROOMCOUNT || s_started) return;
    const s_room_info* info = &g_BgRoomInfo[room];
    const Vtx* vtx = info->vertices;
    if (!vtx || info->usize_point_index_binary <= 0) return;
    const uint32_t n = (uint32_t) info->usize_point_index_binary >> 4;
    const bg_room_data* rd = &ptr_bgdata_room_fileposition_list[room];
    uint32_t cap = s_cap_verts;
    const uint32_t first = s_lvl.nverts;
    s_lvl.pos = (float*) grow_array(s_lvl.pos, &cap, first + n, sizeof(float) * 3);
    cap = s_cap_verts;
    s_area = (float*) grow_array(s_area, &cap, first + n, sizeof(float) * 3);
    cap = s_cap_verts;
    s_lvl.orig = (uint8_t*) grow_array(s_lvl.orig, &cap, first + n, 3);
    s_cap_verts = cap;
    for (uint32_t i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            s_lvl.pos[(first + i) * 3 + k] = (float) vtx[i].v.ob[k] + rd->pos.f[k];
            s_lvl.orig[(first + i) * 3 + k] = vtx[i].v.cn[k];
            s_area[(first + i) * 3 + k] = 0.0f;
        }
    }
    s_lvl.nverts = first + n;
    s_room_first[room] = first;
    s_room_count[room] = n;
    walk_list((const Gfx*) info->ptr_expanded_mapping_info, 65536u, first, n, 1);
    walk_list((const Gfx*) info->ptr_secondary_expanded_mapping_info, 65536u,
              first, n, 0);
}

static struct BakeParams params(void) {
    struct BakeParams p;
    const char* e;
    float world = 220.0f; /* world units, roughly centimetres */
    if ((e = getenv("GE_BAKE_RADIUS")) != NULL) world = (float) atof(e);
    p.radius = world * get_room_data_float1();
    p.rays = (e = getenv("GE_BAKE_RAYS")) != NULL ? atoi(e) : 64;
    p.ao = (e = getenv("GE_BAKE_AO")) != NULL ? (float) atof(e) : 0.85f;
    p.bounce = (e = getenv("GE_BAKE_BOUNCE")) != NULL ? (float) atof(e) : 0.6f;
    if (p.rays < 4) p.rays = 4;
    if (p.radius < 1.0f) p.radius = 1.0f;
    return p;
}

static void start_bake(void) {
    if (s_started || s_lvl.nverts == 0) return;
    struct BakeParams p = params();
    s_lvl.baked = (uint8_t*) malloc((size_t) s_lvl.nverts * 3);
    memcpy(s_lvl.baked, s_lvl.orig, (size_t) s_lvl.nverts * 3);
    s_started = 1;
    portBakeTraceStart(&s_lvl, &p);
}

void portBakeLevelLoaded(void) {
    if (s_lvl.nverts == 0) return;
    s_lvl.nrm = (float*) malloc(sizeof(float) * 3 * s_lvl.nverts);
    for (uint32_t i = 0; i < s_lvl.nverts; i++) {
        const float* a = &s_area[i * 3];
        const float l = portBakeSqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
        for (int k = 0; k < 3; k++) s_lvl.nrm[i * 3 + k] = l > 1e-6f ? a[k] / l : 0.0f; /* the room lists' winding faces out */
    }
    free(s_area);
    s_area = NULL;
    if (portBakedLightingOn()) start_bake();
#ifdef GE_COMPARE
    /* LP64-006: a comparison build finishes the bake during the load, so the
     * frame that shows it is the first tick on every host, not the one the
     * workers' speed picks; the unbaked frames stay covered by the default
     * profile, which has Baked Lighting off. */
    while (s_started && !portBakeTraceStep(4000)) {
    }
#endif
}

/* ---- apply ---------------------------------------------------------------- */

static void write_room(s32 room, s32 strength) {
    s_room_info* info = &g_BgRoomInfo[room];
    Vtx* vtx = info->vertices;
    const uint32_t n = s_room_count[room], first = s_room_first[room];
    if (!vtx || n == 0 || ((uint32_t) info->usize_point_index_binary >> 4) != n) return;
    const float s = (float) strength / 100.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            const uint8_t o = s_lvl.orig[(first + i) * 3 + k];
            if (strength == 0) {
                vtx[i].v.cn[k] = o;
                continue;
            }
            float lin = s_lin[o] + (s_lin[s_lvl.baked[(first + i) * 3 + k]] - s_lin[o]) * s;
            lin = lin < 0.0f ? 0.0f : lin;
            vtx[i].v.cn[k] = portBakeEncode(lin);
        }
    }
    s_applied[room] = strength;
}

static s32 wanted_strength(void) {
    return (s_ready && portBakedLightingOn()) ? portBakedStrength() : 0;
}

void portBakeApplyRoom(s32 room) {
    if (room <= 0 || room >= MAXROOMCOUNT) return;
    s_applied[room] = 0; /* fresh from the file */
    if (wanted_strength() != 0) write_room(room, wanted_strength());
}

/* rewrite every loaded room whose colours differ from what is wanted */
static void refresh_loaded_rooms(void) {
    const s32 want = wanted_strength();
    for (s32 room = 1; room < g_MaxNumRooms && room < MAXROOMCOUNT; room++) {
        if (!g_BgRoomInfo[room].model_bin_loaded || s_applied[room] == want) continue;
        write_room(room, want);
        redarken_lights_in_room(room);
        portRoomCacheRoomFreed(room);
    }
}

/* the log line audits read: how far the bake moved the level's brightness */
static void log_bake(void) {
    double sum = 0.0;
    uint32_t dark = 0;
    for (uint32_t i = 0; i < s_lvl.nverts; i++) {
        const float o = s_lin[s_lvl.orig[i * 3 + 1]], b = s_lin[s_lvl.baked[i * 3 + 1]];
        const float r = o > 1e-4f ? b / o : 1.0f;
        sum += r;
        dark += r < 0.4f;
    }
    fprintf(stderr, "bake: %u verts %u tris radius %.1f mean ratio %.3f below 40%% %u, %u ms\n", s_lvl.nverts,
            s_lvl.ntris, params().radius, sum / s_lvl.nverts, dark, portBakeTraceMillis());
}

void portBakedLightingChanged(void) {
    s_dirty = 1;
}

extern s32 portWeaponExportEnabled(void);

void portBakeTick(void) {
    /* not for the weapon icon export's stage: it captures stock pixels */
    if (s_lvl.nverts == 0 || portWeaponExportEnabled()) return;
    if (s_dirty && portBakedLightingOn() && !s_started) start_bake();
    if (s_started && !s_ready && portBakeTraceStep(4000)) {
        s_ready = 1;
        s_dirty = 1;
#ifdef GE_COMPARE
        portConPrintf(1, "Baked Lighting: ready (%u vertices)\n", s_lvl.nverts); /* LP64-006: no host time on screen */
#else
        portConPrintf(1, "Baked Lighting: ready (%u vertices, %u ms)\n", s_lvl.nverts, portBakeTraceMillis());
#endif
        log_bake();
    }
    if (s_dirty) {
        s_dirty = 0;
        refresh_loaded_rooms();
    }
}

void portBakeViewmodelProbe(f32 out_rgb[3]) {
    out_rgb[0] = out_rgb[1] = out_rgb[2] = 1.0f;
    if (!s_ready || !portBakedLightingOn()) return;
    const coord3d* eye = bondviewGetCurrentPlayersPosition();
    const float p[3] = { eye->f[0] * get_room_data_float1(), eye->f[1] * get_room_data_float1(), eye->f[2] * get_room_data_float1() };
    float rgb[3];
    portBakeTraceProbe(p, rgb);
    const float s = (float) portBakedStrength() / 100.0f;
    for (int k = 0; k < 3; k++) out_rgb[k] = 1.0f + (rgb[k] - 1.0f) * s;
}

const char* portBakeStatusText(void) {
    if (!s_started || s_ready) return NULL;
    snprintf(s_status, sizeof(s_status), "baking %d%%", (int) (portBakeTraceProgress() * 100.0f));
    return s_status;
}
