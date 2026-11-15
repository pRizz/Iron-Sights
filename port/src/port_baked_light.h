/* FORK-082: Baked Lighting - ray-traced ambient occlusion and bounce light
 * baked into the level's room vertex colours (port_baked_light.c) by a CPU
 * ray tracer (port_baked_trace.c). Internal to those two files. */
#ifndef PORT_BAKED_LIGHT_H
#define PORT_BAKED_LIGHT_H

#include <stdint.h>

/* The level's geometry in room-bounds space (room-local position + room pos,
 * the space the game's own collision uses), captured once per stage. */
struct BakeLevel {
    uint32_t nverts;
    float* pos;          /* 3 per vertex */
    float* nrm;          /* 3 per vertex, unit or zero (no triangle uses it) */
    uint8_t* orig;       /* 3 per vertex: the file's colour */
    uint8_t* baked;      /* 3 per vertex: the result, valid once done */
    uint32_t ntris;      /* occluders: the opaque lists only */
    uint32_t* tris;      /* 3 global vertex indices per triangle */
};

/* the bake's tuning: radius in room-bounds units */
struct BakeParams {
    float radius;
    int rays;
    float ao;      /* 0..1: how much occlusion darkens */
    float bounce;  /* 0..1: how much light nearby surfaces add */
};

/* port_baked_trace.c: start baking `lvl` (the BVH is built synchronously,
 * the per-vertex work runs on worker threads, or in portBakeTraceStep slices
 * where threads are unavailable). lvl must stay alive until done or cancelled. */
void portBakeTraceStart(struct BakeLevel* lvl, const struct BakeParams* p);
/* run a slice of the work on this thread (no-op while workers run it);
 * returns 1 once every vertex is baked */
int portBakeTraceStep(unsigned budget_us);
/* stop the workers and free the BVH (blocks until they exit) */
void portBakeTraceCancel(void);
/* 0..1 progress of the current bake */
float portBakeTraceProgress(void);
/* the viewmodel probe: from `pos` (room-bounds space) cast rays in every
 * direction; returns the average baked / original brightness ratio of the
 * surfaces hit (rgb) weighted by distance, 1 when nothing is near or the BVH
 * is gone. Safe while the workers run (it reads only immutable data). */
void portBakeTraceProbe(const float pos[3], float out_rgb[3]);
/* The float maths for port_baked_light.c: the game's include/math.h shadows
 * the C library's there, and its powf is not the hosted one (it returned
 * 16481 for 1^2.2), so that file takes these SDL-backed helpers instead. */
void portBakeLinTable(float lin[256]); /* gamma 2.2 decode of 0..255 */
uint8_t portBakeEncode(float lin);    /* linear -> gamma 2.2 byte, clamped */
float portBakeSqrt(float v);
/* milliseconds the last finished bake took */
unsigned portBakeTraceMillis(void);

#endif
