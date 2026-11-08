# Shiny weapon reflections: candidate work

Status: candidates, not accepted plans. Recorded 2026-11-07 after
[FORK-058](../../FORK_CHANGES.md) (Shiny Gold Guns) shipped. Pick one up by
turning its section into a plan with milestones, and update this table.

| Candidate | Effort | Status |
| --- | --- | --- |
| 1. Position-driven sway | Hours | Tried and rejected 2026-11-08; archived |
| 2. Sky and floor-light tint | A day | Tried with 1, archived with it (not judged on its own) |
| 3. Screen-space reflections from the last frame | Days | Candidate |
| 4. Live environment capture at the player | Weeks | Candidate, needs a feasibility spike |
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
