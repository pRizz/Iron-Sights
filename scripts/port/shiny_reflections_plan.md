# Shiny weapon reflections: candidate work

Status: candidates, not accepted plans. Recorded 2026-11-07 after
[FORK-058](../../FORK_CHANGES.md) (Shiny Gold Guns) shipped. Pick one up by
turning its section into a plan with milestones, and update this table.

| Candidate | Effort | Status |
| --- | --- | --- |
| 1. Position-driven sway | Hours | Tried and rejected 2026-11-08; archived |
| 2. Sky and floor-light tint | A day | Tried with 1, archived with it (not judged on its own) |
| 3a. Sphere map built from the screen | Days | Shipped 2026-11-08 (FORK-058 Richer Reflections) |
| 3. Screen-space reflections from the last frame | Days | Candidate; 3b if 3a is too soft |
| 4. Live environment capture at the player | Weeks | Spike done 2026-11-08: feasible, four fixes needed (below) |
| 5. Baked reflection probes across a mission | Weeks to months | Candidate, depends on 4 |

## Why the stock reflection ignores walking

The four shiny guns use N64 sphere mapping (`G_TEXTURE_GEN`): each vertex's
normal, seen from the camera, picks a spot on a small authored "chrome"
texture (I4 64x64 for the silver guns, CI8 32x32 for the gold). The inputs are
the surface's orientation and the camera's orientation (`gSPLookAt`, built in
`bondview2.c` from the camera's look direction). Position never enters it,
so the environment behaves as if infinitely far away: turning changes the
reflection, walking cannot. The texture is an abstract pattern, not the
level. A real reflection needs position awareness and the actual
surroundings.

Everything below applies inside the existing FORK-058 span (the G_NOOP `0x5E`
tags around the shiny gun's draw in `gunfire.c`), only to reflection-mapped
triangles, so the native hands keep the stock path
([arm contracts](arm_rendering.md)). Candidates 3 to 5 replace or blend with
the authored texture, so they may suit the silver guns too, which the owner
preferred stock with the tone curve alone.

## 1. Position-driven sway (tried, rejected)

**Outcome (2026-11-08):** the owner play-tested it and rejected it: the
reflection's change with movement looked and felt unrealistic, at both speeds
tried. The code is archived on the `archive/shiny-walk-sway` branch (commit
`d973a2c`), never merged into `main`.

What it did: the camera position went to the renderer with the span (`0x5F`
tags) and swayed the reflection coordinates around the stock pattern: ground
position along the camera's right axis moved it sideways, along its forward
axis up and down. Three versions: an unbounded offset (the pattern's phase
drifted onto its dark bands: Gold PP7 median brightness 80 to 27 on Runway),
a bounded `0.35 x sin(distance / 150)` (the whole reflection changed with
small steps) and `0.15 x sin(distance / 1000)` without height (gentle, still
unrealistic). It applied to all four guns, the silver ones without the tuned
look (`silver_motion`).

**Lesson:** sliding the authored pattern cannot read as a reflection,
however it is paced, because nothing in it corresponds to the surroundings:
the eye sees a texture moving, not a room passing. Movement-correct
reflections need real scene content (candidates 3 to 5).

## 2. Sky and floor-light tint (archived with 1)

The level's sky colour (`g_CurrentEnvironment`, `0x60` tag) tinted the
reflective shade, weighted by how lit the floor under the player was (the
game's own `tileColor` darkening alpha, `propobj.c`
`set_color_shading_from_tile`), and lit areas brightened it slightly
(`env_tint` 0.25, `env_bright` 0.2); a black sky (indoor levels) tinted
nothing. It shipped to play-test together with 1 and was archived with it on
the same branch, without a separate verdict, so it could be revived alone.

## 3a. Sphere map built from the screen (shipped)

Added 2026-11-08 after candidate 1 failed. The shiny gun draws after the world
and before the HUD, so when its span opens the target already holds the scene:
the backend copies it (resolving MSAA) into a mipmapped texture
(`port/fast3d/gfx_screen_env.inl`). At the span's first shiny batch a warp pass
builds a 64x64 sphere map in the game's own `G_TEXTURE_GEN` layout (texel
`(s, t)` = eye-space normal `(2s - 1, 2t - 1)` along the camera's right and up
axes): each texel's reflection of the view ray, mirrored forward when it
points behind the camera (shade `screen_back`), samples the screen copy at a
blurred mip level. The game's per-vertex reflection lookup, lighting and floor
shading are unchanged; shiny batches just sample this texture on both units.

First try: mixing the raw scene in replaced the gold with the walls' colours
and went dark (mean brightness 25-96 against 80-133 stock on Runway, Facility
and Bunker). Current: the authored texture times the scene relative to the
screen's average brightness (its 1x1 mip), mixed by `screen` (0.7), so the
metal keeps its colour, detail and level-to-level brightness (within about 10%
of stock on those three levels, with higher contrast) and the surroundings
show as light, dark and hue on it. Knobs in `shiny_weapons.ini`: `screen`,
`screen_gain`, `screen_back`, `screen_fov` (60), `screen_blur`, `screen_flip`.
All four shiny guns (the silver ones without the tone curve; `silver_screen`). Each new map is blended into the last (`screen_smooth` 0.2) and sampled at blur 8 with `screen` 0.5, which cut the frame-to-frame flicker about in half. Limits: nothing behind the player except the
mirrored front; soft by design.

## 3. Screen-space reflections from the last frame

For each shiny pixel, compute the reflection direction from the interpolated
normal, project it onto the previous frame's colour buffer, and sample it,
blurred. The gun then reflects the room actually around the player, on every
level with no per-level data, and the reflection changes naturally while
walking. What is off screen (behind the player) is unavailable: mirror the
lookup and fade back to the authored texture at the edges.

- Builds on the bloom path (`port/fast3d/gfx_bloom.inl`): a per-span
  offscreen buffer, state save and restore, post-process passes in the GL
  backend. Needs a copy of the previous frame's colour (resolve before the
  weapon layer), per-pixel normals for the reflective triangles (a varying or
  a normal buffer), and a composite that blends with the authored texture.
- Risks: the weapon draws in its own projection and depth layer, so
  reflection rays need the world camera's projection; the HUD must not be in
  the sampled frame; WebGL2 support for the copies.
- First milestone: reflect a blurred copy of the previous frame with a fixed
  mirrored lookup on the Golden Gun only, behind a `shiny_weapons.ini` knob,
  and capture it on three levels (one outdoor, one indoor, one dark).

## 4. Live environment capture at the player

Render a small cube map (or dual paraboloid) from the camera position every
few frames and use it as the reflection. It is correct in every direction,
including behind the player. The game only builds display lists for what the
camera sees, so this would re-render the experimental retained room cache
(`port/fast3d/gfx_roomcache.inl`, `[display] room_cache` mode 2, off by
default since a 2026-09-11 A/B). That cache keeps rooms the game has drawn as
GPU vertex buffers, drawable with any matrix. Unseen rooms, props and
characters are missing, and the cache has to be reliable first.

- Feasibility spike before any commitment: enable mode 2 on three levels and
  measure how many rooms around the player are cached, how often captures
  fall back to the interpreter, and the cost of six small extra renders.

**Spike results (2026-11-08).** Debug hook `GE_ENVCUBE_DUMP=n`
(`gfx_envcube_spike.inl`, kept out of the repository) renders every
captured room into six camera-aligned 128x128 faces at the n-th weapon span
and dumps them.

- Mode 2 renders correctly: walking the KF7 ADS audit, Facility and Bunker
  matched `room_cache = 0` pixel for pixel and Dam differed by a mean of
  0.5-1.1 / 255; no fallbacks. About 90-125 rooms were captured over ~900
  frames per level; the game draws 4-50 rooms a frame.
- Cached rooms do render into cube faces from the camera position: Facility's
  "ahead" face showed the same X-braced wall as the game's view; Dam's faces
  showed its rock walls and structures.
- Each room has its own modelview (4 of 5 rooms differed on Facility, 9 of 11
  on Dam), so reusing one room's matrix for all misplaced most of them (half
  black faces, jumbled Dam rooms). Needed: store each room's placement
  (its modelview relative to the frame's camera when it last drew) and
  rebuild it with the current camera.
- No sky: the sky is not a room. Clear the faces to the level's sky colour,
  or draw the sky separately.
- Colours came out grey, not Facility's teal: the retained draws' combiner
  constants and fog were zeroed in the spike; pass the captured ones.
- Timing: 8-12 ms for six faces, but that included reading them back and
  writing files; the render cost alone is still unmeasured.

Next, if pursued: per-room placement, sky clear, captured constants, then
time six faces without the dump and feed the cube into 3a's warp pass in
place of the screen copy.

## 5. Baked reflection probes across a mission

At level load, or the first time a room is seen, capture probes at points
through the level and blend the nearest as the player walks, with box
projection so reflections line up with the room's walls. This is the
"map of the mission": the most accurate, cheapest per frame once baked, and
the usual approach in PC and console games. It inherits every dependency of
candidate 4, plus probe placement (room centres from the BG room table),
storage, invalidation (broken lights already invalidate rooms in the cache),
and what to show for rooms not yet seen. Consider only if candidate 3 is not
enough.
