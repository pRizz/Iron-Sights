# Load before play

A stage pays for its loading while it loads, not when the player first looks
at something. A loading screen that lasts a little longer is better than a
frame that stalls during play: a stall is seen and felt, a longer load is
hardly noticed. This guide is the rule, the tools that measure it and the
mechanisms that keep it.

The rule came from Runway (FORK-090). Opening the shutter at the start made
the outdoor rooms visible for the first time, and in that one frame the
renderer decoded 60 HD textures from the pack, inflating each PNG with the
reference decompressor `puff`: a 350 ms frame, against 25 ms with original
textures. A per-frame benchmark cannot see this, because every frame of a
benchmark window has already loaded what it draws.

## The rule

- Work done on first use is loading. Reading or decoding a file, building or
  uploading a texture, compiling or linking a shader, making GL objects, and
  anything that waits for the driver belong to the stage load (or the boot),
  not to play.
- When something can only be known on first use, record it so a later run
  does it while loading (the shader list, `tools/shaders_base.txt`; the HD
  prefetch list, `port/fast3d/texture_packs/hd_stage_prefetch.json`).
- When neither works, do it in the background and show the old result until
  it is ready (custom HD pack images; HD images a stage was not known to
  draw). Never stall a frame of play for it.
- Leaving loading on first use needs a reason, written in the inventory
  entry: memory the 32-bit Windows build cannot spare, or behaviour the
  original game has on purpose (the CLASSIC preset's Load Hitches row, which
  imitates the N64's cart reads, is one).

## What the game does now

- **HD textures** are read on worker threads while the stage loads
  (`port/fast3d/hd_texture_prefetch.cpp`): the stage's mission folder in the
  pack, the in-game effects, and the other folders' textures the stage was
  seen to draw. `boss.c` waits for them at the end of the load. A download
  image not prefetched is read in the background during play and swapped in
  when ready.
- **Shaders.** The prewarm compiles every listed program while a stage (the
  title at boot included) loads, instead of 3 ms a frame during play; a
  combiner program made while loading builds its room-cache twin too. The
  list is `tools/shaders_base.txt` plus the install's own `Data/shaders.txt`.
- **GL texture names** come 64 at a time and are stocked up while loading: a
  threaded driver answers `glGenTextures` only after its queue drains.
- **Warm-up frames.** A stage's first three frames are drawn in full and
  presented black, each finished in the driver. A program's first real draw,
  the driver's background compiles and the level cube's first build (Richer
  Reflections, Glass Reflections) cost there, behind the load's black screen.

`GE_LOAD_AHEAD=0` turns all of this off and loads on first use as before, for
before/after measurements of one executable. A mask keeps some parts: 1 HD
prefetch, 2 prewarm at load and room-cache twins, 4 texture names ahead, 8
warm-up frames and the loading finish (unset is 15, all of them).

## Seeing late loads

Every timed load event after a stage's load is a *late load*
(`port/src/port_late_loads.c`). The game prints one line for each that takes
`GE_LATEWARNMS` (default 3) or more, at most 20 a stage:

```
ge: late load: uploads 64.9 ms during play (stage 35, tick 303): fmt 2 siz 0 1024x1024 name 286 us | hd 1024x1024 lookup 63513 us
```

`GE_LATELOG=<file>` writes every late event. Kinds: `rooms` (room geometry),
`models` (a model file), `tex` (a texture inflated from the ROM), `shaders`
(a combiner program, or `retained` for its room-cache twin) and `uploads` (a
texture decoded and uploaded, with its GL name wait and any HD read). The
hitch log (`GE_HITCHMS`) and the benchmark frame log (`GE_BENCH`) count the
same events per frame.

## The audit

```sh
python scripts/port/late_load_audit.py
```

Each mission and multiplayer-only arena runs in its own folder with an
isolated copy of the game, unpaced, with the HD pack from
`build_win/Data/TexturePacks` linked in (`--no-pack` for original textures).
`GE_LOADTOUR` (`port/src/port_load_tour.c`) puts Bond on one pad of every
room in turn and turns him a full circle, so each room comes into view.
The report (`build_win/late-load-audit/<time>/report.md`) has a row per
stage: the load, late events by kind, the worst frame of play that loaded
something, the worst frame of play for any reason, and the frames whose
load work reached `--budget-ms` (default 4). It also lists the costliest
late events and every shader compiled during play.

- `--check` exits 1 when a frame of play loaded `--budget-ms` or more, or a
  shader compiled during play.
- `--env GE_LOAD_AHEAD=0` audits the old behaviour for comparison.
- `--learn-prefetch` records which HD textures from other folders each stage
  drew and writes them to `hd_stage_prefetch.json`. Rerun it after the pack's
  scope (`texture_packs/hd_level_sources.json`) changes.
- `--stage <name>` (repeatable) audits only those.

Shaders the report lists go into `tools/shaders_base.txt` (or rerun
`scripts/port/record_shaders.sh`). A tour ends early when a room it enters
finishes the mission (Dam's bungee); the Rooms column says how far it got.
Delete the audit folders when done (`build_win/late-load-audit`).

`python scripts/port/test_late_load_audit.py` tests the report without a ROM.

## Known remaining first-use costs

- With the HD pack, a frame that first draws several newly uploaded large
  textures can wait 100-250 ms in the NVIDIA driver, untimed: on Dam when the
  gun first shows after the intro, and in the audit's tour where it jumps into
  a large new area (the "Worst frame in play" column). With original textures
  it did not show, nor on Runway and Frigate with only the prefetch switched
  off (`GE_LOAD_AHEAD=14`), where the same places stalled about as long
  decoding the images instead. It is not the uploads (a `glFinish` after
  each took at most 3.5 ms), not address space (a large-address-aware copy
  stalls the same), not program first draws and not the level cube's storage.
  The suspected cause, not confirmed without GPU tools, is the driver making
  the new textures resident on their first draw. Uploading and drawing each
  prefetched image during the load is the next thing to try.
- A room that comes into view for the first time is captured by the room
  cache, and the level cube draws it with its programs for the first time:
  about 17 ms of untimed render work in the Runway shutter frame, the same
  with original textures. Drawing every room during the warm-up frames was
  tried and does not help: the game unloads rooms it has not seen for a while,
  and the room cache forgets them with it.
- A prefetched HD image still uploads to GL when first drawn: 1-13 ms for the
  pack's largest (1024 px) images.
- Models (a guard's body and head, a weapon) and their textures are read
  from the ROM the first time a stage spawns them, 1 to 3 ms each, as the
  original game does.
- Native textures are inflated from the ROM when a room or model first uses
  them (about 0.1 ms each) and uploaded at first draw.
