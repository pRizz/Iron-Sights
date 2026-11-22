/**
 * The picture digest (LP64-006 Task 7): one line a frame hashing everything
 * the renderer is asked to draw, in its own file, so two runs and two builds
 * can compare their pictures without comparing pixels
 * (scripts/port/lp64_equiv_streams.py's `gfx` finds the first difference).
 *
 * videoInit (video.c) passes the backend's table through portGfxDigestWrap in
 * a comparison build (GE_COMPARE). With GE_GFXDIGEST=FILE it returns a table
 * whose every entry hashes its arguments and then calls the backend's, and
 * writes FILE afresh, one line at each end_frame:
 *
 *   F n=N tick=T draws=D tris=R vtx=H8 tex=U:H8 state=H8 post=H8 [demo=K]
 *
 * N counts frames from 1 and T is g_GlobalTimer when the frame ends. D and R
 * count the draws (draw_triangles and retained_draw*) and their triangles.
 * Each H8 is a 32-bit FNV-1a hash, over 32-bit values each four bytes from
 * the lowest, of the frame's calls of one part, each call's entry number and
 * then its arguments:
 * - vtx: every float of every vertex buffer drawn, as its bits, and the
 *   retained geometry: a retained buffer is hashed once at retained_build
 *   (its stream, positions, normals and inputs), and each retained_draw*
 *   adds that hash for its descriptor with its matrices, fog and lights;
 * - tex: the bytes and sizes uploaded (U the uploads), and at each draw the
 *   contents of the textures its shader samples;
 * - state: depth, viewport, scissor, alpha, samplers, shader ids, tile and
 *   framebuffer selection, clears and the frame's framebuffer setup;
 * - post: the optional passes' arguments (copies, resolves, bloom, screen
 *   reflections, the level cube, glint, glass, tone mapping).
 * Bookkeeping hashes into no part: queries, shader lookups and info, new and
 * freed texture ids, framebuffer and retained buffer creation. A demo's
 * frames before its recording plays follow the host's pace, and the texture
 * cache's names, recycled ids and frees follow those frames (measured on
 * dam1), so only what is drawn with them reaches a part.
 * Content is hashed, never an address or a handle: a shader is its two
 * combiner ids, a texture the hash of its last upload, a retained draw its
 * descriptor's built contents, a framebuffer the order in which the backend
 * handed it out (the interpreter makes them at boot and for its own passes),
 * and a path only whether there is one.
 * demo=K ends the line only while a recording replays: K is the recording's
 * tick (portRamromReplayTick), as on the simulation trace's T line.
 *
 * GE_GFXDIGEST_NODRAW=1 calls the backend only for the entries whose result
 * the interpreter uses, or that set what one of those answers: shader
 * creation, lookup, info and clearing, texture and framebuffer ids and their
 * setup, clip parameters, the texture size limit and filter, retained_create,
 * retained_build and retained_destroy, start_draw_to_framebuffer, and the
 * frame's start and end. Draws, uploads, copies, binds, clears, post passes
 * and read-backs are hashed and skipped. An entry the backend leaves NULL
 * stays NULL either way.
 *
 * GE_GFXDIGEST_DETAIL=A-B adds, before the F line of each frame A to B, one
 * line a call:
 *
 *   C n=N k=K entry=NAME part=PART hash=H8
 *
 * K counts the frame's calls from 1, PART is vtx, tex, state, post or none,
 * and H8 hashes that call alone. A FILE that cannot be opened or written
 * stops the run with `ge: FATAL: GE_GFXDIGEST ...` and exit status 1.
 * Without GE_GFXDIGEST the backend's table is returned as it is. CMake compiles port_gfx_digest.cpp only with GE_COMPARE and
 * GE_VIDEO, so no shipped build holds it.
 */
#ifndef PORT_GFX_DIGEST_H
#define PORT_GFX_DIGEST_H

struct GfxRenderingAPI;

#ifdef __cplusplus
extern "C" {
#endif

/** INNER, or with GE_GFXDIGEST set the digest's table around it; each call starts the file afresh. */
struct GfxRenderingAPI *portGfxDigestWrap(struct GfxRenderingAPI *inner);

#ifdef __cplusplus
}
#endif

#endif
