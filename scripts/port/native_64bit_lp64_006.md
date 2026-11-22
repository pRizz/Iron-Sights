# LP64-006 Equivalence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-21 from measurements on `main` at bfc0271
and accepted by the owner the same day with all thirteen recommendations
under "Decisions for the owner" at the end ("Accept all thirteen").
Nothing is built; execution starts at Task 1. The status table of
[native_64bit_plan.md](native_64bit_plan.md) records what is built and
verified as the tasks land.

**Goal:** Evidence that the 64-bit game is the 32-bit game: the same
simulation, the same picture and the same sound for the surface the
design names, with every difference explained under its rules; the
sanitizer, soak, test, save and Arm checks of verification items 8 to 12;
and pool sizes set from the soak.

**Architecture:** The milestone is comparison-driven. Three streams are
written by the game itself, each to its own file: one line a game tick for
the simulation, one line a frame for what the renderer is asked to draw,
and the mixed sound. A run mode called lockstep makes a stage run depend
only on its seed, its route and its tick count, which neither paced runs
nor `GE_FIXEDTICK` give today (measured below). One new tool,
`lp64_equiv.py`, runs a fixed list of cases on a build, first checks that
two runs of one build agree, then names the first tick, frame or sample at
which two builds part. The attract demos keep their recorded pace and stay
with `lp64_play.py`. Comparison builds share one test-only CMake option
that removes the C library's rounding from the question; no shipped build
compiles any of it. Each difference is fixed at its declaration or
recorded with its cause and the run that removes it.

**Tech Stack:** C and C++ (GCC 13.3, glibc 2.39, Ubuntu 24.04
containers), CMake, Python 3 standard library, Bash test harnesses, Docker
Desktop on Apple Silicon, SDL 2.30 (offscreen video, dummy audio), Mesa's
llvmpipe.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md): milestone LP64-006
("Verification items 5 to 12 pass, or each difference is explained under
the rules above. Budgets set from the soak."), "Verification" with its
three rules and "What counts as measured", "Memory budgets", "Working
machine" and its gallery rule, "Decisions". Read it first, then
`AGENTS.md`, [the LP64-005 plan](native_64bit_lp64_005.md) for the task
check and the rules this plan continues, and in `FORK_CHANGES.md`
(FORK-089) the passages "LP64-005 hand-over to LP64-006" and "For the
owner, at LP64-005's close". Search that file; its lines are very long.

## What was measured for this plan (2026-11-20 and 21, `main` at bfc0271)

All runs used the Mac's containers (`lp64_env.py`, GCC 13.3, glibc 2.39):
aarch64 native, x86_64 translated by Rosetta 2, i686 emulated by
qemu-i386. Nothing ran on real x86 hardware or a GPU. Four trees were
configured again under `build/lp64` (`boot`, `boot-x64`, `ref-i686`,
`video`) and two for single checks (`lto`, `gate`). Probe scripts lived
under `build/lp64/scratch` and were deleted afterwards; every figure is
also in the planning session's measurement notes
(`lp64_006_measurements.md`, outside Git).

### `weapons-dam`

- Twelve runs on the plain aarch64 headless executable, six at once
  while two other trees compiled: 11 passed, 1 failed. The failure is a
  new window: no shot between change 13 (`weapon 2827`, right 11 and
  left 11, at tick 2658) and change 14 (tick 2778), where a passing run
  logs five shots at ticks 2700 to 2724. The failing run logged 106
  shots in all against 124, drew its first weapon at tick 1157 against
  1153, and reached `moved` at tick 2564 against 2405.
- The 32-bit build misses too. The i686 headless executable passed the
  case once in the whole `input` set (29 of 29 cases, 374 s, four at
  once), then failed 2 of 6 runs made six at once beside an i686 soak:
  no shot after change 30 (`weapon 20`) in one, none between change 12
  and change 13 in the other. Shot totals across the six: 105 to 124.
- So the miss is not tied to change 29, to a two-handed pair or to 64
  bits. Every failure so far came while other runs shared the machine.
- By the enumeration in `src/bondconstants.h` (`ITEM_UNARMED` is 0), item
  19 is `ITEM_GOLDENGUN` and 20 `ITEM_SILVERWPPK`; `ITEM_SPECTRE` is 12
  and `ITEM_MP5KSIL` 11. The case's docstring and the hand-over call
  change 29 "the dual Spectre" and change 30 "the M16"; by the
  enumeration they are a pair of Golden Guns and the silver PP7.

### Runs that should repeat, and do not

- A probe outside `lp64_play.py`: Dam with `GE_CHEATS=guns`, pad 0 from
  a route of ten weapon changes each followed by a held trigger, 2,700
  frames, six runs at once on the aarch64 headless executable. Paced as
  the play sets are: three runs, three different `ge: play:` sequences
  (124, 128 and 130 events). With `GE_FIXEDTICK=1` and still paced: three
  runs, two identical and one not (three shots one tick later, at 1999,
  2005 and 2011 against 1998, 2004 and 2010, and `moved` at tick 2428
  against 2491).
- Read: `osGetTime` (`port/src/libultra_shim.c`) is the host's clock, the
  pump (`port/src/pcsched.c`) sends retraces on that clock whether or not
  a game frame finished, and a headless build counts a route's ticks by
  pad reads (`port/src/inputrec.c`). `GE_FIXEDTICK` fixes only the fields
  a frame advances.
- The benchmark suite already runs fixed-tick, unpaced routes in a video
  build (`bench_plan.py`: `GE_FIXEDTICK=1`, `GE_NOSLEEP=1`,
  `GE_TICKHZ=0`, ending by tick through `GE_BENCH_END`), and so does the
  hand gallery. In a headless build `GE_NOSLEEP` lets the frame limit
  pass before the game has done anything (the LP64-005 plan measured it).
- The hand gallery's capture mode (`GE_HAND_GALLERY=1` with the tool's
  fixture settings) ran on the aarch64 video executable offscreen: 216
  captures (108 cases, two samples each) in 131 s and again in 99 s, and
  all 216 screenshots of the second run were byte-identical to the
  first's. A fixed-tick, unpaced video run repeats to the pixel there.

### Simulation (item 5)

- The 14 demos passed on all three headless executables: aarch64 in
  101 s and x86_64 in 101 s (seven at once), i686 in 241 s (four at
  once). `lp64_play.py traces --expect` gave `14 identical` for the i686
  run against aarch64 and against x86_64.
- With `GE_DEMOPOS=1` added (Bond's position each demo tick, three
  decimals): 6,674 ticks common to the i686 and the aarch64 logs, none
  differing; the same against x86_64; aarch64 against x86_64 identical in
  all 14; two i686 runs against each other, 6,673 common ticks, none
  differing.
- Lines tear. In those two i686 runs `traces --reference-check` failed
  two demos for a torn tick line (bunker2 at tick 111, silo2 at 422) and
  three position lines were cut by a line of the other stream. More
  output a tick means more tears.
- No per-tick trace exists outside a demo: the `ge: demo tick` line is
  printed inside the recording's replay (`src/game/ramromreplay.c`).
- Routes exist for Dam, Facility, Runway and Train, the 13 arenas'
  respawns and one Temple strafe (`lp64_play_input.py`); 16 missions and
  Cuba have none. The benchmark's generated route turns with mouse
  records, which a headless build ignores.
- The i686 headless build passed the `input` set, 29 of 29, so it
  replays every route that exists, judged by the events each case
  requires. Nothing compares those runs tick by tick.

### The C library (the comparison rule's maths)

- Neither build has a shared-maths option. `CMakeLists.txt` links libm;
  `src/libultra/gu/sinf.c` and `cosf.c` are in the tree and not compiled.
- Both headless executables import twelve libm functions: `atanf`,
  `ceilf`, `cosf`, `expf`, `fabsf`, `fmaxf`, `fminf`, `fmodf`, `powf`,
  `sinf`, `sqrtf`, `tanf`.
- A sweep of 1,048,576 inputs each, the same program built three ways
  (`-m32 -msse2 -mfpmath=sse`, x86_64, aarch64): `sinf`, `cosf`, `expf`
  and `powf` gave the same bits everywhere. `tanf` differed for 37,603
  inputs between i686 and x86_64, 11,746 between aarch64 and x86_64 and
  34,617 between i686 and aarch64; `atanf` for 24,837, 132 and 24,821.
  The i686 figures are this emulator's: if glibc's i386 `tanf` and
  `atanf` use x87 instructions (not checked), qemu computes them its own
  way.
- `tanf` and `atanf` are called from fork code that sets the field of
  view and draws (`lv.c`, `sky.c`, `gunfire.c`, `port_gun_kick.c`,
  `port_projectile_arc.c`, `gfx_shiny.cpp` and others). `atan2f` and
  `acosf` are the game's own (`src/game/math_atan2f.c`).

### Picture and sound (items 6 and 7)

- No render digest exists. `port/fast3d/gfx_pc.cpp` holds no OpenGL
  call; it draws through `struct GfxRenderingAPI`
  (`gfx_rendering_api.h`), and the one place that chooses the
  implementation is `port/src/video.c:5082` (`set.rapi =
  &gfx_opengl_api`), a deferred-tier file.
- `PORT_PLAY_HARNESS` (`port/include/port_playlog.h`) is 0 in a 32-bit
  build with video, so today's 32-bit video reference has no play log.
- A headless build reads no `midas.ini` (`port/src/headless_stubs.c`
  returns compiled-in defaults), so a profile with fork features on
  needs a video build. `port_options.c` has 173 option rows, 72 of them
  the fork's.
- The `sound` set ran twice on the aarch64 video executable: Dam
  recorded 41.57 s and then 39.50 s, the title 30.35 s and 31.03 s, and
  the files differ. The 64-bit build's recording does not agree with
  itself. `audio_pc.c` mixes a frame for each game field and then tops
  the device's queue up by its level.
- Not rerun: the 32-bit video reference. The LP64-005 record has a demo
  case's 3,000 frames with video taking 769 s there.

### Sanitizers, soak, tests, saves (items 8 to 11)

- `audit_buffers.py --mission-smoke` is headless only and gives a
  mission 120 s. Not rerun: at LP64-005's close the play sets with video
  passed 148 of 148 under the sanitizers on aarch64.
- A three-lap soak (`GE_SOAK=all:600:3`, `GE_MEMSTATS=1`) on the aarch64
  headless executable: 731 s, exit 0, `ge: soak: complete after 3 laps`,
  no pool line. Largest shares: texture pool 52.1 percent, room memory
  51.5, music heap 44.0, graphics buffer 28.1, vertex pool 14.3, stage
  bank 6.6. No pool's peak grew from lap to lap (room memory 271,696,
  243,296 and 271,696 bytes). Resident memory grew by 10,240 KB a lap
  (51,376, 61,616 and 71,856 KB) while private memory stayed at
  168,500 KB and the game heap's largest use at 5,156 KB; the cause was
  not looked for.
- `GE_SOAK` knows the 20 missions; `scripts/port/soak.py` is the Windows
  audit of the same harness.
- The 37 `scripts/port/test_*.sh` harnesses: 35 exit 0 in the aarch64
  container (60 s; a compiler there builds only 64 bits) and 35 in the
  x86_64 container at both widths (265 s). `test_web_touch.sh` needs
  `emcc` in both. `test_headless_configure.sh` configures a 32-bit tree
  and fails on aarch64. `test_start_input.sh` fails its 32-bit half in
  the `goldeneye` image: it includes `video.c`, and the image has no
  i386 `SDL2/_real_SDL_config.h`. 26 harnesses source
  `harness_widths.sh`, and 23 logs print both `PASS (32-bit)` and
  `PASS (64-bit)`. Three use `-m32` without the helper: `test_mema_order`
  and `test_monitor_script` (they run at the compiler's width, then at
  32), and `test_pointer_bits` (it reads CMake's flags). Four are named
  in no workflow: `test_hitmap`, `test_image_bank`, `test_room_seams`,
  `test_start_input`.
- A save: the i686 headless build, driven through the front end to Dam,
  wrote `Data/Saves/eeprom.bin` (512 bytes); the aarch64 build on the
  same route wrote the same bytes; started on a copy of the i686 folder
  it logged `ge: eeprom.bin loaded (512 bytes)`, passed the same eleven
  menus and left the file unchanged. That file holds no progress.
  `save_data` (`src/game/file.h`) has no pointer and no `long`.

### Arm (item 12) and the checks the hand-over lists

- aarch64 against x86_64: the 14 demo traces and the per-tick positions
  are identical (above).
- One link with `-flto=auto -Wlto-type-mismatch` (aarch64, 64-bit
  headless): it links, with 97 warnings in 18 files (49 in
  `image_bank.c`, 15 in `assets/oddtextures.h`, 6 each in `title.c` and
  `mpmenu.c`); 17 are return types; 7 name a strict-tier file
  (`viGetPerspNorm` between `fr.c` and `gunfire.c`, `tanf`, `fabsf`
  twice, `powf`, `get_player_mp_char_head` and `_body` between
  `bondview2.c` and `front.c`). The hand-over's text scan counted 51.
- The same tree's C files hold 241 implicit declarations of 201
  functions. Three are libm's (`tanf` in `sky.c:43`, `fabsf` in
  `bondview2.c:1561`, `powf` and `fabsf` in the SDL shim); GCC gives a
  built-in its own type. No function of the 201 is defined in `src/` or
  `port/` as returning a float (a text match on definitions).

### What could not be measured

- Anything in lockstep, any trace outside a demo, any digest, a sound
  capture that ignores wall time: none exists yet.
- The i686 build with video, and so the cost of a digest or a recording
  on the emulated reference. Task 7 measures it first and has a stop
  rule (decision 4).
- The sanitizer trees, the tours, the full play sets, the 4-player and
  menu routes, the profile with fork features on.
- Whether the 32-bit build's resident memory also grows in a soak:
  under qemu-i386 the game's statistics are the emulator's (a two-lap
  i686 soak reported 4,334,016 KB private, and resident memory of
  66,096 KB and then 72,140 KB).
- Real x86 hardware, a GPU, the render thread, `clang`, GCC 14.

## What each item's run proves here

"Both builds" in the design means i686 and x86_64. On this Mac neither
is native. A translated or emulated run executes the same program with
IEEE arithmetic done in software (qemu) or by translation (Rosetta 2);
in lockstep no result depends on the host's speed.

| Item | Runs on this Mac | Proves | Leaves open |
|---|---|---|---|
| 5 | i686 twice, x86_64, aarch64; headless comparison builds, and video ones for the second profile | the three programs compute the same simulation for the surface, given the same C-library results | x87 paths on real silicon; anything outside the surface |
| 6 | the same three, video comparison builds with the digest and no drawing; screenshots and the gallery under llvmpipe | the three programs ask for the same picture; the software renderer's pictures are classified | any GPU driver; a 64-bit build on a GPU (LP64-007) |
| 7 | the same runs, sound locked to game fields | bank and sequence conversion give the same samples | audio devices and their timing |
| 8 | aarch64, native, sanitizer builds headless and with video | no overrun or undefined operation in what ran | x86_64-only code paths (none known) |
| 9 | aarch64 headless and video, native | peaks and budgets at 64 bits | a GPU driver's memory |
| 10 | x86_64 container at both widths, aarch64 at 64; CI's x86_64 runner, which runs both widths natively | the ROM-free tests pass at both widths | nothing known |
| 11 | i686 and aarch64 headless | a save moves both ways | nothing known |
| 12 | aarch64 native against x86_64 translated | goal 5 for the surface | Apple's libm and compiler (the macOS project) |

Decision 5 asks the owner for one session on real x86 hardware for items
5 to 7; without it they close on the first three rows as written.

## The hand-over, item by item

Every bullet of "LP64-005 hand-over to LP64-006" and of "For the owner,
at LP64-005's close", with where this plan puts it.

| Hand-over item | Where it goes |
|---|---|
| Demo traces: all fourteen identical; pictures, sound and input recordings not compared | Measured again (identical). Tasks 5, 8 and 9 |
| Model opcodes and prop record types covered by fixtures only; rerun the census on the landed code | Task 16 Step 1, over every folder this plan's runs make |
| Pools: only multiplayer Statue's room memory raised; final sizes are this milestone's | Task 11; decision 8 |
| Recordings' lengths follow wall time | Task 9 (sound locked to game fields) |
| What video ran on: no GPU, no real x86, render thread off | Decisions 5, 6 and 12; Task 8 Step 5 (one case with the thread on) |
| The render thread's exit stall | Decision 12; Task 16 Step 4 |
| `weapons-dam` varies from run to run | Task 1, first; Task 2 Step 5 |
| The play harness in a 64-bit video build | LP64-007 (the hand-over says so). This plan adds to that harness; Task 17 lists what it added |
| Link once with `-flto -Wlto-type-mismatch` | Measured (97 warnings). Task 15 |
| Pointer arithmetic on a difference of two unsigned 32-bit values; reads through the wrong member of a union | Task 15; decision 11 |
| `OP17` converted with the Group record's map | Task 16 Step 2; decision 10 |
| Three `chrGetEquippedWeaponProp(...)->chr` reads without a NULL check | Task 16 Step 3; decision 10 |
| The index-0 load before a setup stop reads two bytes past a bank allocation at 64 bits | Task 16 Step 3; decision 10 |
| Four test drivers stub the peak hooks | A rule of Task 11 (Files) |
| The frame-end check's cost | LP64-007 (needs a GPU); decision 6 |
| The late-load comparison's scratch reader; the 32-bit comparison on real hardware | Task 17 Step 2 if the owner grants decision 5; otherwise LP64-007 |
| Stale wording in accepted plans (x86_64 "under emulation") | Not in this milestone: accepted plans stay as accepted. This plan says "translated" |
| No claim beyond the demo traces | This milestone |
| No GPU or real x86 hardware | Decisions 5 and 6 |
| The twelve stage numbers that stop by name | Unchanged (LP64-005's decision 4); outside the surface |
| Fix wave C's two paths verified by reading | Not in this milestone: both need a damaged ROM image, which no test may carry |
| Inherited reads unbounded at both widths | Not in this milestone: LP64-007's hardening and upstream |
| The 32-bit special-portal walk's link-order dependence | Not fixed here. If a trace difference leads there, it is explained under the design's rule (Task 6) |
| The test harnesses on macOS; `test_audio_buffers.c`'s `mmap` flags | Not in this milestone: the macOS project |
| Sound runs only in a video build | A constraint of Task 9 |
| `A_PAN` undefined; `clang`, GCC 14 and Windows x64 not built | Not in this milestone |
| LP64-004's decision 6: a second load of Dam keeps 334 changed portal fields at 32 bits | Task 6 (the `dam-twice` case); Review Focus 4 |
| Owner: decision 2 answered; `tours` under the sanitizers answered | Closed |
| Owner: five inherited defects kept at 32 bits; `indy_commands.c`; replayed pad input reaching the watch rows; `GE_KILLBOND`; `-D_scheduler_h_` in CMake | Still the owner's. This plan changes none of them; Task 17 carries the list forward |
| Owner: the recording-length comparison unmet | Task 9 |
| Owner: the gallery pending in its deferred tier | Cleared by Task 17's regeneration |

## What LP64-006 owns, and what it leaves to LP64-007

LP64-006 owns: the comparison of simulation, picture and sound across
the three builds for the design's surface; the tools that make those
runs repeatable; every 64-bit defect the comparison finds; the sanitizer
smoke with video; the soak and the pool sizes it sets; the ROM-free
tests at both widths; saves across widths; and the Arm check.

LP64-007 owns: packages, a 64-bit build on a GPU and what that measures
(the frame-end check's cost, shader and upload timing), whether a shipped
64-bit build keeps the play harness, hardening against crafted files,
and the suspend-point notice.

## File map

New files, by the task that creates them. Everything is deferred tier
or outside the gallery's inputs.

| File | Task | What it is |
|---|---|---|
| `scripts/port/test_playlog_gun.c` | 1 | the gun-state line and the stall |
| `port/src/port_lockstep.c`, `port/include/port_lockstep.h` | 2 | lockstep's switch and clock |
| `scripts/port/test_lp64_compare.sh` and its drivers `test_lockstep.c`, `test_simtrace.c`, `test_compare_math.c`, `test_audio_lock.c` | 2, 3, 4, 9 | the ROM-free tests of the harness, at both widths |
| `port/src/port_simtrace.c`, `port/include/port_simtrace.h` | 3 | the simulation trace |
| `scripts/port/lp64_equiv_streams.py` | 3, 7, 9 | reads a stream and finds the first difference |
| `port/src/port_compare_math.c`, `scripts/port/lp64_libm_sweep.c` | 4 | the shared `tanf` and `atanf`; the sweep |
| `scripts/port/lp64_equiv.py`, `lp64_equiv_cases.py`, `lp64_equiv_profile.py`, `lp64_equiv_expected.txt` | 5 | the runner, the 38 cases, the profiles, the exceptions |
| `port/src/port_gfx_digest.cpp`, `port/include/port_gfx_digest.h`, `scripts/port/test_gfx_digest.cpp`, `test_gfx_digest.sh` | 7 | the digest wrapper |
| `scripts/port/lp64_equiv_pictures.py` | 8 | screenshots and the gallery's captures |
| `port/src/port_audio_lock.c`, `port/include/port_audio_lock.h` | 9 | sound locked to game fields |
| `scripts/port/lp64_budgets.txt` | 11 | each pool's size and peak, by stage |
| `scripts/port/test_save_widths.c`, `test_save_widths.sh` | 13 | a save across widths |
| `scripts/port/lp64_lto.py`, `lp64_lto_allowed.txt` | 15 | the link-time mismatches |

Existing files that gain a hook: `src/game/lv.c` (one call),
`port/src/pcsched.c`, `port/src/libultra_shim.c`,
`src/game/frametiming.c`, `port/src/inputrec.c`, `port/src/video.c`,
`port/src/audio_pc.c`, `port/src/port_playlog.c`,
`port/include/port_playlog.h`, `CMakeLists.txt`. Existing files that a
fix or a pool change edits: `src/game/chraction.c`, the file of the
index-0 load (Task 16), `port/src/romconv_model_records.c`,
`include/PR/gu.h`, `port/src/port_lp64.c` and the source of each pool
Task 11 hooks or raises. None is in the strict tier.

## Global Constraints

The LP64-005 plan's constraints continue unchanged: the 32-bit program
does not change (`compare_win32.py commits HEAD WORKTREE` exits 0, or
every changed function is named and explained); fix the declaration, not
the symptom; 64-bit branches sit behind `IS_64_BIT`; runs are judged by
what they did; no `ge: lp64:` line but the informational ones; renderer
formats are fixed; no laundering casts; upstream-owned files get small
mechanical edits; no ROM-derived data in Git; every commit updates
`FORK_CHANGES.md` (FORK-089); delivery is direct to `main`, one push a
task after its review is clean; never `git commit --no-verify`; commit
messages end with `Co-Authored-By: Claude Fable 5.2
<noreply@anthropic.com>`. This plan adds:

- **No shipped build compiles the comparison harness.** The digest, the
  sound lock and the shared maths compile only with `-DGE_COMPARE=ON`.
  Lockstep and the simulation trace compile where `PORT_PLAY_HARNESS` is
  1, which gains `|| defined(GE_COMPARE)`; it stays 0 in a 32-bit build
  with video, so the Windows comparison stays identical. It is 1 in
  every 64-bit video build; whether LP64-007's 64-bit packages keep the
  play harness is LP64-007's to decide (see "What LP64-006 owns").
- **The reference agrees with itself first** (the design's rule). No
  stream of a case is compared across builds until two runs of each
  build give the same file. A file that is short, torn or missing is
  neither equal nor different: the case fails.
- **A stream holds no address and nothing of the host.** No pointer, no
  time, no path, no thread id. Floats are written as their bits.
- **A difference is explained only by the design's rule**: its first
  tick, frame or sample, its cause, and a run that matches with the
  cause removed. `lp64_equiv_expected.txt` holds one line for each;
  everything it does not list must be identical.
- **Emulation.** A difference seen only against the emulated i686 build
  or only on the translated x86_64 build is marked `host:` in that file
  until a run on real x86 hardware confirms it (decision 5).
- **`lp64_play.py` keeps refusing `GE_FIXEDTICK` and `GE_NOSLEEP`**, and
  demos never run in lockstep (decision 1). It may hold a case marked
  `lockstep=True`, which it runs with `GE_LOCKSTEP=1`; its `--env`
  refuses `GE_LOCKSTEP`, and Task 2's `AGENTS.md` edit rewrites the
  sentence that says its runs are paced in real time.
- **Strict-tier files** (`STRICT_FILES` and `port/fast3d/` in
  `hand_gallery.py`) change only on the branch `lp64/equiv-strict`.
- **Render performance, load before play, settings, input idioms**
  (`AGENTS.md`). No task adds per-frame work, first-use work, a setting,
  a row or a prompt to a shipped build. A pool size changed at 64 bits
  is not per-frame work; Task 11 still runs the late-load audit on the
  stages whose budgets it changes.
- New Python uses the standard library only; new source files stay under
  500 lines; shell scripts start with `#!/usr/bin/env bash` and
  `set -euo pipefail`; every waiting command has a deadline.

### The strict batch

No task edits a strict-tier file by plan. The digest wraps the renderer
from outside `port/fast3d/` (decision 3), and the comparison harness
hooks `lv.c`, `pcsched.c`, `libultra_shim.c`, `audio_pc.c` and `video.c`,
all deferred tier. Candidates the measurements point at: the seven
link-time type mismatches that name `gunfire.c`, `bondview2.c`,
`port_arm_mesh.c` or `port_native_arms.c` (Task 15), and whatever cause a
picture or simulation difference names in `gunfire.c`, `model.c`,
`bondview2.c` or the renderer (Tasks 6 and 8).

- A commit that touches a strict-tier file goes to `lp64/equiv-strict`,
  rebased on `main` after every push to `main`, as in the LP64-005 plan.
- A check on `main` that needs a strict-tier fix says so in its
  expectation line (`differs-from N strict: REASON`) until the branch
  lands.
- The branch lands once, in Task 17, through one regeneration on the
  Windows laptop: one trip. If the branch is empty the laptop still
  regenerates once, which clears the gallery left pending since
  LP64-005's close.

### Task check

Every task ends by running this and recording the results in FORK-089.
Steps 1 to 8 are the LP64-005 plan's task check as its Task 15 ran it.
Step 9 exists from Task 5; `SETS`, `STREAMS`, `RUN`, `OPTIONS` and
`SUFFIX` are what the task names. `RUN` is one of `sim`,
`video-default`, `video-all`, `snd-default` and `snd-all`, so no task's
folders overwrite another's; a task that names none uses `sim`.
`SUFFIX` is empty for `sim` and `-video` for the others; `OPTIONS` are
the run's (`--video`, `--profile default|all`).

**When it runs in full (owner, 2026-11-21, "Yes, three checkpoints").**
Steps 1 to 8 took about two and a half hours in Task 1, an hour of it the
video sets, against 35 minutes of the task's own work. They run in full at
three checkpoints: after Task 4, after Task 9, and before the final
whole-milestone review. Task 17 Step 3 still runs the whole check once
more. Between checkpoints:

- a task that changes game C or C++ runs the plain aarch64 play sets and
  the one other configuration its change touches (sanitizer, x86_64 or
  video); the video sets run only in Tasks 7 to 10 and at the checkpoints;
- a task that changes only tools, tests or documents runs no play sets:
  its executable is the one last checked, and its FORK-089 note says so;
- every task still runs `compare_win32.py`, the static checks, every
  harness line of the CI tests step in the x86_64 container, and step 9
  where the task names it. Reviews and CI after each push are unchanged.

A regression may therefore surface up to a few tasks after its cause; a
checkpoint that fails is bisected over the tasks since the last one
before work continues.

```bash
# 9. The comparison: each build agrees with itself, then the builds agree.
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/lp64_equiv.py run --rom /rom/baserom.u.z64 --set SETS OPTIONS --repeat 2 --output build/lp64/equiv/RUN/i686 build/lp64/cmp-i686SUFFIX/IronSights
python3 scripts/port/lp64_env.py run --arch x86_64 -- python3 scripts/port/lp64_equiv.py run --rom /rom/baserom.u.z64 --set SETS OPTIONS --repeat 2 --output build/lp64/equiv/RUN/x64 build/lp64/cmp-x64SUFFIX/IronSights
python3 scripts/port/lp64_env.py run --arch aarch64 -- python3 scripts/port/lp64_equiv.py run --rom /rom/baserom.u.z64 --set SETS OPTIONS --repeat 2 --output build/lp64/equiv/RUN/a64 build/lp64/cmp-a64SUFFIX/IronSights
python3 scripts/port/lp64_equiv.py compare --streams STREAMS --expect scripts/port/lp64_equiv_expected.txt build/lp64/equiv/RUN/i686 build/lp64/equiv/RUN/x64 build/lp64/equiv/RUN/a64
```

Expected: each `run` prints one `CASE: RAN` line a case, `OK: N of N
cases ran` and `OK: N of N cases agree with themselves`; `compare`
prints `OK: N cases, S streams, I identical, D as recorded`. Also run
every harness line of the CI tests step, in the x86_64 container. The
browser build is checked by CI after the push.

### Behaviour-sensitive sites

- `osGetTime` and `osGetCount` (`port/src/libultra_shim.c`): the host's
  clock. With `GE_LOCKSTEP` unset they must return exactly what they do
  today. (Task 2)
- `portPumpEvents` (`port/src/pcsched.c`): its pacing is the shipped
  game's frame timing. Lockstep is a branch taken only with the
  variable set, in harness builds. (Task 2)
- `PORT_PLAY_HARNESS`: 0 in a 32-bit video build. The 32-bit video
  comparison build turns it on through `GE_COMPARE`, never through a
  change to the condition's other terms. (Task 4)
- `tanf` and `atanf`: shipping builds keep libm's. Only a comparison
  build links the in-tree pair. (Task 4)
- `set.rapi = &gfx_opengl_api` (`video.c:5082`): unchanged unless the
  build is a comparison build and `GE_GFXDIGEST` is set. (Task 7)
- The audio pump (`audio_pc.c`, around line 1174): its queue top-up and
  trim are the shipped game's sound timing. The lock is a branch taken
  only in a comparison build with `GE_AUDIO_LOCK` set. (Task 9)
- Pool constants: each change sits behind `IS_64_BIT` with its measured
  peak beside it; the 32-bit value's tokens do not move. (Task 11)
- `load_bg_file` at 32 bits keeps the first load's portal edits
  (LP64-004's measurement). It is the reference and is not changed.
  (Task 6)

## Review Focus

Five ways this milestone can report equivalence that is not there, which
no task's ordinary tests would exercise. Each has a test in the task
that owns it.

1. **A comparison that cannot see.** Two files are equal because the
   trace hashes nothing that changed, a run ended before play, or a
   digest dropped the field that differs. Expected: `lp64_equiv.py
   sensitivity` runs one case with `GE_SEED=1` and with `GE_SEED=2` on
   one build and fails unless the simulation trace, the digest and the
   recording each differ; a unit test flips one bit of one guard's
   position and one vertex and requires the first difference at that
   tick and frame. (Tasks 3, 5, 7, 9.)
2. **A stream that carries an address, or an order that follows one.**
   Then two widths never agree, or the field is masked and real
   differences go with it. Expected: every case runs twice a build with
   address-space randomisation on and must agree with itself; the
   digest's unit test passes the same bytes at two addresses and
   requires one digest; Task 3 lists every sort and list order the trace
   walks and shows none compares pointers; Task 5 records each
   container's `/proc/sys/kernel/randomize_va_space` in FORK-089.
   (Tasks 3, 5, 7.)
3. **Comparison builds that are not the shipped program.** The shared
   maths changes results, so equivalence could hold only for a program
   nobody ships. Expected: the 14 demos on each comparison build give
   the `ge: demo tick` trace of the plain build (Task 4 Step 5), and
   every case is also compared with the option's maths off, the
   difference from that run recorded as the C library's (Task 6
   Step 4).
4. **Equal on the first load, different on the second.** The 32-bit
   build keeps 334 portal fields from a stage's first load; the 64-bit
   build loads fresh. A single run a stage cannot show it. Expected: the
   case `dam-twice` loads Dam, aborts and loads it again in one process
   and is compared like any other. (Tasks 5, 6.)
5. **Budgets from a soak that never met the worst case.** Standing
   still for 600 frames peaked room memory at 51.5 percent; the Aztec
   tour reached 68.2. Expected: the budget table takes each pool's
   largest peak over the soak, the tours, the routes and every
   multiplayer scenario, and `lp64_play.py peaks --budget` fails a later
   run that passes half a pool. (Task 11.)

---

### Task 1: Find why `weapons-dam` varies

The owner accepted this case's miss on condition that its cause is found
first.

**Files:**
- Modify: `port/src/port_playlog.c`, `port/include/port_playlog.h`,
  `scripts/port/lp64_play_input.py` (the docstring's item names),
  `scripts/port/test_lp64_play_input.py`,
  `scripts/port/test_inputrec_headless.sh` (runs the new driver),
  `FORK_CHANGES.md`
- Create: `scripts/port/test_playlog_gun.c`, run by
  `scripts/port/test_inputrec_headless.sh`

**Interfaces:**
- Consumes: `struct hand` (`src/game/bondview.h`: `weaponnum`,
  `weapon_firing_status`, `weapon_hold_time`, `weapon_action_state`,
  `weapon_current_animation`, `weapon_ammo_in_magazine`,
  `weapon_next_weapon`); `g_ClockTimer`, `g_GlobalTimer`;
  `portInputRecTickNow`.
- Produces: with `GE_PLAYLOG=2`, one line for player 0 at each `weapon`
  change and on each tick pad 0 holds `Z_TRIG`:
  `ge: play: gun tick=T clock=C global=G r=ITEM,STATE,ANIM,FIRING,HOLD,AMMO,NEXT l=...`.
  `GE_PLAYLOG=1` prints exactly what it prints today.
- Produces: `GE_PLAYSTALL=T:MS` (harness builds only): one sleep of MS
  milliseconds at input tick T, to force a missed retrace on purpose,
  through `void portPlaySleepMs(u32 ms);` so a test can stub the sleep.

- [ ] **Step 1: Write the failing test**

`test_playlog_gun.c`: `gun_line_has_both_hands` (a fabricated player
with right `11,3,7,1,40,30,-1` and left the same prints those fields in
that order); `level_one_prints_no_gun_line`; `stall_sleeps_once_at_its_tick`
(a stubbed `portPlaySleepMs` is called once, with 250, at tick 100 and
not at 99 or 101). Run: `bash scripts/port/test_inputrec_headless.sh`.
Expected: FAIL, `undefined reference to 'portPlayGunLine'`.

- [ ] **Step 2: Implement** `void portPlayGunLine(u32 tick);` and the
  stall in `port_playlog.c`, called from `portPlayPlayerTick`. Run the
  test. Expected: `test_inputrec_headless: PASS`.

- [ ] **Step 3: Catch a failing run with the gun state**

Run `weapons-dam` with `--env GE_PLAYLOG=2`, six at once, on the plain
aarch64 headless executable until one fails (1 of 12 here; give it 60
runs, then use Step 4's stall to provoke it) and on
`build/lp64/ref-i686` (2 of 6 here). Record, for each failing window
and the same window of a passing run: both hands' state at the `weapon`
line and at each tick of the held trigger, and `clock` and `global` at
each.

- [ ] **Step 4: Name the cause and prove it both ways**

The cause is named when a run that forces it fails every time and a run
that prevents it passes. First hypothesis, from the measurements: a
frame that spans two retraces advances the game's clock by two fields
for one pad read, so a draw or reload animation is still running when
the route's trigger window opens. Both widths miss under load, which
fits. Test it with `GE_PLAYSTALL` at the tick before each change
(expected: the miss at that change, ten runs of ten, at both widths).
If the gun state says otherwise (an empty magazine, a state that never
leaves), follow that instead, and say whether the two widths show the
same state.

- [ ] **Step 5: Correct the record**

`lp64_play_input.py`'s docstring names the items by `ITEM_IDS` (change
29 is a pair of `ITEM_GOLDENGUN`, change 30 `ITEM_SILVERWPPK`). FORK-089
gets the cause, the two logs' lines and the forced runs' counts. If the
cause is a 64-bit defect, fix it here with its own failing test. If it
is the case's timing, the case moves to lockstep in Task 2 Step 5 and
this task says so.

- [ ] **Step 6: Task check; commit**

After any fix to the case or the game, repeat: the `input` set on the
plain and sanitizer aarch64 and the plain x86_64 headless executables
and with video (aarch64), and `weapons-dam` 26 times on the plain
aarch64 headless executable (10 alone, then two rounds of six at once,
then four more alone). Expected: 26 of 26.

```bash
git commit -m "Log player 0's gun state and name why weapons-dam misses a shot"
```

---

### Task 2: Lockstep: a run that depends only on seed, route and tick

**Files:**
- Create: `port/src/port_lockstep.c`, `port/include/port_lockstep.h`,
  `scripts/port/test_lockstep.c`, `scripts/port/test_lp64_compare.sh`
- Modify: `port/src/pcsched.c`, `port/src/libultra_shim.c`,
  `src/game/frametiming.c` (`portFixedTick`), `port/src/inputrec.c`,
  `scripts/port/lp64_play_cases.py` (`Case.lockstep`),
  `scripts/port/lp64_play_run.py`, `scripts/port/test_lp64_play.py`,
  `.github/workflows/windows-release.yml`, `AGENTS.md`,
  `FORK_CHANGES.md`

**Interfaces:**
- `s32 portLockstep(void);` true when `GE_LOCKSTEP` is set (read once);
  always 0 where `PORT_PLAY_HARNESS` is 0.
- `u64 portLockstepMicroseconds(void);` the retraces delivered times
  1,000,000 divided by 60 (whole numbers), from which `osGetTime`
  answers instead of the host's clock.
- In lockstep: the pump delivers exactly one retrace for each game frame
  and never sleeps; a frame advances one field; `GE_FRAMES` counts game
  frames; the render thread is off; `GE_NOSLEEP` and `GE_TICKHZ` are
  ignored; a demo launch stops with `ge: FATAL: GE_LOCKSTEP cannot
  replay a demo`; the log holds `ge: lockstep: on` once.
- `lp64_play.py`: a case with `lockstep=True` gets `GE_LOCKSTEP=1`.
  `--env` refuses `GE_LOCKSTEP`. Demos never get it.

- [ ] **Step 1: Write the failing test**

`test_lockstep.c` (at both widths through `test_lp64_compare.sh`):
`time_is_retraces_times_a_field` (`portLockstepMicroseconds()` is
16,666 after one retrace and 1,000,000 after 60);
`unset_reads_the_host_clock` (with `GE_LOCKSTEP` unset, `osGetTime`
equals the stubbed host clock's value converted as today);
`one_retrace_a_frame` (a stub game loop that takes three pump calls a
frame sees one retrace message a frame over 100 frames).
`test_lp64_play.py`: `lockstep_case_sets_the_variable`,
`env_refuses_lockstep`, `demo_never_gets_lockstep`.
Run: `bash scripts/port/test_lp64_compare.sh`. Expected: FAIL,
`port_lockstep.h: No such file or directory`.

- [ ] **Step 2: Implement**

- [ ] **Step 3: Find every other reader of the host**

List each call of `osGetTime`, `osGetCount`, `portHostMicroseconds`,
`time`, `clock_gettime` and `SDL_GetTicks` that the simulation can reach
(not the profiler, not log text) and each read of memory the game did
not write. Each is routed through lockstep's clock or recorded as
unreachable in a comparison run. Record the list in FORK-089.

- [ ] **Step 4: Prove it repeats**

Run the planning probe again as a ROM run: Dam, `GE_CHEATS=guns`, ten
weapon changes with a held trigger, 2,700 frames, `GE_LOCKSTEP=1`,
`GE_PLAYLOG=2`; six at once, three rounds, on the plain aarch64 and the
i686 headless executables. Expected: the 18 logs' `ge: play:` lines are
identical within each build (today: 2 of 3 with `GE_FIXEDTICK`, 0 of 3
paced). Also `GE_PLAYSTALL=1500:250` changes nothing in lockstep. The
route is written once by `lp64_play_route.route_text` to
`build/lp64/lockstep/route.txt`; each run, with K its number:

```bash
python3 scripts/port/lp64_env.py run --arch aarch64 -- env GE_ROM=/rom/baserom.u.z64 GE_STAGE=dam GE_CHEATS=guns GE_INPUTPLAY=build/lp64/lockstep/route.txt GE_FRAMES=2700 GE_LOCKSTEP=1 GE_PLAYLOG=2 build/lp64/boot/IronSights > build/lp64/lockstep/a64-K.log 2>&1
```

and the same with `--arch i686`, `build/lp64/ref-i686/IronSights` and
`i686-K.log`.

- [ ] **Step 5: `weapons-dam`, if Task 1 named its timing**

Mark the case `lockstep=True`, measure its ticks again and pin them.
Run it 26 times as in Task 1 Step 6. Expected: 26 of 26. Otherwise this
step records that Task 1's fix stands without it.

- [ ] **Step 6: Task check; commit**

```bash
git commit -m "Add lockstep: one retrace a game frame and a clock that counts them"
```

---

### Task 3: A simulation trace for every tick, in its own file

**Files:**
- Create: `port/src/port_simtrace.c`, `port/include/port_simtrace.h`,
  `scripts/port/test_simtrace.c` (run by `test_lp64_compare.sh`),
  `scripts/port/lp64_equiv_streams.py`,
  `scripts/port/test_lp64_equiv_streams.py`
- Modify: `src/game/lv.c` (one call at the end of `lvlTick`),
  `scripts/port/test_lp64_compare.sh`,
  `.github/workflows/windows-release.yml`, `FORK_CHANGES.md`

**Interfaces:**
- `void portSimTraceTick(void);` With `GE_SIMTRACE=FILE` it appends one
  line a call to FILE and nothing to the log:
  `T n=N timer=G seed=S16 draws=D players=P pH... chrs=C:H8 props=R:H8 rooms=L:H8`
  where N counts calls from 1, G is `g_GlobalTimer`, S16 the random seed
  as 16 hex digits, D the draws since the last line
  (`g_portRandDrawCount`), each `pH` a player's position, facing and
  health as float bits, and each `count:hash` a count and an FNV-1a hash
  over values (for a guard: slot, action, flags, position bits, health
  bits; for a prop: list index, type, flags, position bits; for a room:
  room number and whether it is loaded). Expands to nothing where
  `PORT_PLAY_HARNESS` is 0. The leading `T` is literal (the record's
  kind, as `F` is the digest's); each `pH` is `pI=X,Y,Z,FACING,HEALTH`,
  eight hex digits each, players separated by one space. One line:
  `T n=1 timer=1 seed=00000000deadbeef draws=2 players=1
  p0=3f800000,00000000,c2c80000,40490fdb,3f800000 chrs=3:0badf00d
  props=12:01234567 rooms=4:89abcdef`, written on one line.
- `GE_SIMTRACE_DETAIL=A-B` adds, for calls A to B, one line a guard and
  one a moving prop, so a difference can be read to its record.
- `lp64_equiv_streams.py`: `first_difference(kind, path_a, path_b) ->
  Difference | None` for `kind` `sim` (Tasks 7 and 9 add `gfx` and
  `snd`); `Difference` has
  `index`, `field` and both values. A file with no line, a last line
  without a newline or a line that does not parse raises `Unreadable`.

- [ ] **Step 1: Write the failing tests**

`test_simtrace.c`: `line_has_every_field_in_order` (a fabricated tick
gives the exact line written in the test); `float_is_written_as_bits`
(0.1f prints `3dcccccd`); `one_bit_in_one_guard_changes_the_chr_hash`;
`no_pointer_reaches_the_file` (the same state at two heap addresses
gives the same line); `detail_range_is_inclusive`.
`test_lp64_equiv_streams.py`: `identical_files_have_no_difference`;
`names_the_first_differing_tick_and_field`; `short_file_is_unreadable`;
`line_cut_in_two_is_unreadable`.
Run both. Expected: FAIL (`port_simtrace.h: No such file or directory`;
`No module named 'lp64_equiv_streams'`).

- [ ] **Step 2: Implement the trace and the reader**

- [ ] **Step 3: Show no order follows an address**

List each list the trace walks (guard slots, the active prop list, the
loaded rooms) and how the game orders it; for each, the line of source
that fixes the order. A list ordered by a pointer comparison is walked
by index instead and named in FORK-089.

- [ ] **Step 4: First traces**

On the plain aarch64 and the i686 headless executables: `GE_STAGE=dam`
in lockstep, 1,800 frames, `GE_SIMTRACE` set, twice each. Expected: the
two files of each build are byte-identical, and all four hold the same
number of `T` lines (one for each `lvlTick`; record it). Say what the
i686 and aarch64 files' first difference is, if any; it is Task 6's to
explain.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Write one simulation line a tick to a trace file"
```

---

### Task 4: The comparison build and its maths

**Files:**
- Create: `port/src/port_compare_math.c`,
  `scripts/port/test_compare_math.c` (run by `test_lp64_compare.sh`),
  `scripts/port/lp64_libm_sweep.c`
- Modify: `CMakeLists.txt` (`option(GE_COMPARE ...)`),
  `scripts/port/test_lp64_compare.sh`,
  `port/include/port_playlog.h` (`PORT_PLAY_HARNESS`),
  `scripts/port/test_pointer_bits.sh`, `scripts/port/package_linux.py`,
  `scripts/port/package_windows.py`, `AGENTS.md`, `FORK_CHANGES.md`

**Interfaces:**
- `-DGE_COMPARE=ON`: defines `GE_COMPARE`, links `port_compare_math.c`,
  works at either width, headless or with video, and logs `ge: compare:
  on` once. CMake's help text says it is never a shipped build;
  `package_linux.py` and `package_windows.py` refuse a tree configured
  with it.
- `-DGE_COMPARE_LIBM=ON` (with `GE_COMPARE`): the same build with
  libm's `tanf` and `atanf`, logging `ge: compare: on (libm)`. It is
  the twin that shows what the C library changes.
- `port_compare_math.c` defines `float tanf(float)` and
  `float atanf(float)` using only `+ - * /` and comparisons on `double`,
  so every host rounds them alike. `sinf`, `cosf`, `expf` and `powf`
  stay libm's (decision 2).
- `lp64_libm_sweep.c`: prints, for each of the six functions, an FNV-1a
  hash of 1,048,576 results; built and run by the test at each width.
- Trees: `build/lp64/cmp-i686`, `cmp-x64`, `cmp-a64` (headless) and
  `cmp-i686-video`, `cmp-x64-video`, `cmp-a64-video`; the libm twins
  `cmp-i686-libm`, `cmp-x64-libm`, `cmp-i686-video-libm` and
  `cmp-x64-video-libm` are built by the tasks that use them.

- [ ] **Step 1: Write the failing tests**

`test_compare_math.c`: `tanf_is_within_two_ulp_of_double_tan` and
`atanf_is_within_two_ulp_of_double_atan` over 100,000 inputs;
`tanf_of_zero_is_zero`; `atanf_is_odd`. `test_pointer_bits.sh`:
`compare tree defines GE_COMPARE and links port_compare_math.c`;
`default tree does neither`; `a 32-bit video compare tree has
PORT_PLAY_HARNESS 1`.
Run: `bash scripts/port/test_lp64_compare.sh && bash
scripts/port/test_pointer_bits.sh`. Expected: FAIL at the first new
assertion.

- [ ] **Step 2: Implement the option and the two functions**

- [ ] **Step 3: The sweep, in three containers**

Build `lp64_libm_sweep.c` against a comparison tree's maths in the
i686, x86_64 and aarch64 containers and compare the six hashes.
Expected: all six equal across the three (today, without the option:
`tanf` and `atanf` differ, the other four are equal). If one of the
four differs on a host, it joins `port_compare_math.c` in this task.

- [ ] **Step 4: Build the six trees**

Expected: each links; `compare_win32.py commits HEAD WORKTREE` prints
`identical in loaded code and data`.

- [ ] **Step 5: The comparison build is still the game** (Review
  Focus 3)

Run `lp64_play.py run --set demos` on the plain `build/lp64/ref-i686`
(into `build/lp64/play/ref`) and on `cmp-i686`, `cmp-x64` and
`cmp-a64`, then `traces --expect scripts/port/lp64_demo_traces.txt
build/lp64/play/ref DIR` for each of the three. Expected: `14
identical` three times. A demo that parts from the plain reference is recorded with its
tick and the `tanf` or `atanf` call that the trace of random draws
names; it does not block the task, and Task 6 compares with the maths
off as well.

- [ ] **Step 6: Task check; commit**

```bash
git commit -m "Add the comparison build with one tanf and atanf for every host"
```

---

### Task 5: The cases and their runner

**Files:**
- Create: `scripts/port/lp64_equiv.py`, `scripts/port/lp64_equiv_cases.py`,
  `scripts/port/lp64_equiv_profile.py`,
  `scripts/port/lp64_equiv_expected.txt`,
  `scripts/port/test_lp64_equiv.py`
- Modify: `.github/workflows/windows-release.yml` (the test),
  `AGENTS.md`, `FORK_CHANGES.md`

**Interfaces:**
- Consumes: `lp64_play_run.py` (one executable copy, a folder a case),
  `lp64_play_judge.problems`, `lp64_play_route.route_text`,
  `settings_placement.option_keys` and `read_upstream`,
  `lp64_equiv_streams.first_difference`.
- The cases (38):

  | Set | Cases | How it runs | Ticks |
  |---|---|---|---|
  | `demos` | the 14 names | paced by the recording, as `lp64_play.py` runs them, with `GE_SIMTRACE` | 3,000 frames |
  | `solo` | the 20 missions and `cuba` | lockstep; `GE_CHEATS=invuln`; a generated route: stick forward 150 ticks, C-left or C-right 45 ticks by the benchmark's turn list, `Z_TRIG` held 30 of every 120 ticks from tick 400 | 3,900 |
  | `mp` | `temple-4p` | lockstep; four players; the same route for pad 0 | 3,900 |
  | `menus` | `walk` | lockstep; from boot: START through the front end to Dam, the watch's five tabs, the abort, back to the title | as measured in Step 3 |
  | `reload` | `dam-twice` | lockstep; Dam, the watch's abort, Dam again from the front end, the `solo` route in the second load | as measured in Step 3 |

- `lp64_equiv.py run --rom ROM --set NAME[,NAME...] [--profile
  default|all] [--video [--draw] [--shots TICK]] [--repeat N] [--jobs N]
  [--only CASE] [--maths-off] [--timeout-scale F] [--env NAME=VALUE]...
  [--saves DIR] --output DIR EXECUTABLE`: runs each case N
  times into `DIR/runK/`, writing `CASE.log`, `CASE.sim`, and with
  `--video` `CASE.gfx` and `CASE.wav`. Prints `CASE: RAN (T ticks, S
  shots)` or `CASE: FAIL (reason)`, `OK: N of N cases ran`, then, with
  `--repeat` above 1, `OK: N of N cases agree with themselves` or
  `FAIL: CASE STREAM differs from itself at I`. A case fails without
  `ge: lockstep: on` (demos excepted), its frame-limit line, its subject
  lines (a stage load; for a route with fire, 30 shots or more and
  `moved`), or with any line `lp64_play_judge.problems` fails. It
  refuses an executable whose log lacks the exact line `ge: compare:
  on`, refuses `ge: compare: on (libm)` without `--maths-off`, and with
  `--maths-off` refuses one whose log lacks `ge: compare: on (libm)`.
  `--env` passes a variable to every case and refuses those the runner
  sets: `GE_LOCKSTEP`, `GE_FIXEDTICK`, `GE_NOSLEEP`, `GE_SIMTRACE`,
  `GE_GFXDIGEST` and `GE_GFXDIGEST_NODRAW` (`GE_GFXDIGEST_DETAIL` and
  `GE_SIMTRACE_DETAIL` pass), `GE_AUDIO_LOCK`, `GE_AUDIO_CAPTURE` and
  `GE_SEED`. `--saves DIR` copies DIR into the folder of each case of
  set `saves` (Task 13) before it runs; that set refuses to run without
  it.
- `lp64_equiv.py compare --streams sim[,gfx,snd] [--expect FILE] DIR...`:
  compares each later DIR with the first, case by case, run 1 with
  run 1. Prints `CASE STREAM A:B identical (N)` or `... first differs at
  I (FIELD)`; with `--expect`, exit 0 only if every line is as FILE
  says. FILE lists exceptions only: `PAIR CASE PROFILE STREAM
  differs-from I explained: CAUSE; removed-by: RUN`, or `open:`,
  `strict:`, `host:` with a reason. A listed exception that now matches
  fails too.
- `lp64_equiv.py sensitivity [--video] --rom ROM --output DIR
  EXECUTABLE`: one `solo` case at `GE_SEED=1` and `GE_SEED=2`; exit 0
  only if every stream the run wrote differs, printing `OK: STREAMS
  differ between seeds` (for example `OK: sim differs between seeds`,
  `OK: sim, gfx and snd differ between seeds`).
- `lp64_equiv_profile.py`: `profile_ini('default')` is `[display]
  fullscreen=windowed`; `profile_ini('all')` sets every fork option key
  to the value in its table `ALL_ON`; `EXCLUDED` maps a key left out to
  its reason (`crash_reporting`: it would call the network;
  `texture_pack`: no pack is in the containers, and packs are
  ROM-derived). `ALL_ON` sets `font_style` to the Iron Sights style,
  with that reason beside it: the Enhanced pack is downloaded only after
  the player consents (`AGENTS.md`). Only video builds read it.

- [ ] **Step 1: Write the failing test**

`test_lp64_equiv.py`, with stand-in executables as `test_lp64_play.py`
has: `run_fails_a_case_without_the_lockstep_line`;
`run_fails_a_route_that_fired_nothing`;
`repeat_reports_a_case_that_differs_from_itself`;
`compare_names_first_difference_and_field`;
`compare_fails_an_exception_that_now_matches`;
`compare_fails_a_difference_not_listed`;
`sensitivity_fails_when_two_seeds_agree`;
`route_turns_with_c_buttons_only` (no `M` record in a `solo` route);
`all_profile_covers_every_fork_key` (each key of `option_keys` not in
`read_upstream` is in `ALL_ON` or `EXCLUDED`; today 72 keys);
`all_profile_keeps_fonts_local_and_packs_out` (`font_style` is the Iron
Sights style; `texture_pack` is in `EXCLUDED`);
`demo_cases_never_get_lockstep`; `env_refuses_runner_variables`;
`plain_check_refuses_a_libm_build`; `saves_set_needs_saves_folder`.
Run: `python3 scripts/port/test_lp64_equiv.py`. Expected: FAIL, `No
module named 'lp64_equiv'`.

- [ ] **Step 2: Implement the runner, the cases and the profile**

- [ ] **Step 3: Measure the two long routes and pin them**

Build `walk` and `dam-twice` as `lp64_play.py` builds a route of steps
(a recording run finds each wait's tick), in lockstep on `cmp-a64`. Pin
the ticks as constants with the date and commit. Expected: each of the
two runs twice with identical `.sim` files.

- [ ] **Step 4: Every case runs and agrees with itself, 64 bits**

Run: `lp64_equiv.py run --set demos,solo,mp,menus,reload --repeat 2`
on `cmp-a64`, then `sensitivity`.
Expected: `OK: 38 of 38 cases ran`, `OK: 38 of 38 cases agree with
themselves`, and `OK: sim differs between seeds`. A case that does not
agree with itself is this task's defect: find the host value Task 2
Step 3 missed. Record each container's
`/proc/sys/kernel/randomize_va_space` in FORK-089 (Review Focus 2).

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Add the equivalence runner, its 38 cases and the two settings profiles"
```

---

### Task 6: Simulation equivalence (item 5)

**Files:**
- Modify: whatever each cause names (a strict-tier file on
  `lp64/equiv-strict`); `scripts/port/lp64_equiv_expected.txt`;
  `scripts/port/abi_probe/probe.c`;
  `scripts/port/lp64_overlays_reviewed.txt`; `FORK_CHANGES.md`

**Interfaces:**
- Consumes: Tasks 2 to 5; `lp64_play.py callers` for the caller of a
  random draw; `GE_SIMTRACE_DETAIL`.
- Produces: `lp64_equiv_expected.txt` with no `open:` line for stream
  `sim` in the default profile except `open: owner` lines from Step 5;
  the wall time of one full pass on each
  host, recorded. The second profile needs a video build, so its
  simulation is compared in Task 8 Step 2.

- [ ] **Step 1: The reference agrees with itself**

Run task check step 9 (`RUN` `sim`) for i686 only, all five sets,
`--repeat 2`. Expected: `OK: 38 of 38 cases agree with themselves`. A
demo whose stream does not repeat is a defect this task finds; if it
cannot be made to repeat, the task stops and brings the demo, its first
differing tick and what was ruled out to the owner.

- [ ] **Step 2: Compare, earliest difference first**

Run step 9 whole (`RUN` `sim`) with `--streams sim`. For each case
that differs between i686 and x86_64: rerun both with
`GE_SIMTRACE_DETAIL` around the tick (`--env GE_SIMTRACE_DETAIL=A-B`),
read the record that differs,
and classify as the LP64-005 plan's
Task 5 did: a 64-bit defect (write the check that shows it, fix it,
rerun all 38); the C library (Step 4); the reference's own state
(`dam-twice`, Step 3); or not repeatable.

- [ ] **Step 3: `dam-twice`** (Review Focus 4)

If the second load differs and the first does not, the cause on trial
is the 32-bit build's kept portal fields. It is explained when a
scratch 32-bit comparison build that reads the background's tables from
the file again at each load, as the oracle build's fresh conversion
does, matches the 64-bit trace. Record the first tick and say what the player would see. Nothing
shipped changes; whether the 32-bit game should reload fresh goes to the
owner with that evidence.

- [ ] **Step 4: With the maths off**

Run the 38 cases on `cmp-i686-libm` and `cmp-x64-libm` with
`--maths-off`. Expected: identical, or each difference's first tick is
recorded as `explained: libm tanf/atanf; removed-by: GE_COMPARE` once
the caller is shown to be one of the two.

- [ ] **Step 5: Stop rule**

A difference that two working sessions have not classified goes to the
owner by name, with its tick, the record that differs and what was
ruled out; its line reads `open: owner`.

- [ ] **Step 6: Task check; commit** (one commit a cause)

```bash
git commit -m "Record where each case's simulation matches the 32-bit build"
```

---

### Task 7: A digest of what the renderer is asked to draw

**Files:**
- Create: `port/src/port_gfx_digest.cpp`, `port/include/port_gfx_digest.h`,
  `scripts/port/test_gfx_digest.cpp`, `scripts/port/test_gfx_digest.sh`
- Modify: `port/src/video.c` (the choice of `set.rapi`),
  `CMakeLists.txt` (the file compiles only with `GE_COMPARE` and
  `GE_VIDEO`), `scripts/port/lp64_equiv_streams.py` (`gfx`),
  `scripts/port/test_lp64_equiv_streams.py`,
  `.github/workflows/windows-release.yml`, `AGENTS.md`,
  `FORK_CHANGES.md`. `test_gfx_digest.sh` runs at both widths through
  `harness_widths.sh`.

**Interfaces:**
- Consumes: `struct GfxRenderingAPI` (`port/fast3d/gfx_rendering_api.h`,
  read only).
- `struct GfxRenderingAPI *portGfxDigestWrap(struct GfxRenderingAPI
  *inner);` returns a table whose every entry hashes its arguments and
  then calls `inner`'s. With `GE_GFXDIGEST_NODRAW=1` it calls `inner`
  only for entries that return information the interpreter uses
  (shader creation, lookup and info, texture and framebuffer ids, clip
  parameters, the texture size limit, `retained_create` and
  `retained_build`, which return handles the interpreter keeps, and
  `start_draw_to_framebuffer`, which returns a `bool`) and skips
  drawing, uploads, copies, post passes and read-backs. An entry `inner`
  leaves NULL stays NULL.
- With `GE_GFXDIGEST=FILE`, one line at each `end_frame`:
  `F n=N tick=T draws=D tris=R vtx=H8 tex=U:H8 state=H8 post=H8`:
  `vtx` over every float of every vertex buffer drawn, as bits; `tex`
  over the bytes and sizes uploaded; `state` over depth, viewport,
  scissor, alpha, sampler and shader ids; `post` over the optional
  passes' arguments. Buffers are hashed by content, never by address:
  a retained buffer is hashed once at `retained_build`, and each
  `retained_draw*` adds that hash for its handle to `vtx`. `T` is
  `g_GlobalTimer` when the frame ends.
- `GE_GFXDIGEST_DETAIL=A-B` adds one line a call for frames A to B.

- [ ] **Step 1: Write the failing test**

`test_gfx_digest.cpp`, against a stand-in inner table that counts
calls: `same_calls_same_line`; `one_vertex_bit_changes_vtx_only`;
`one_texel_changes_tex_only`; `same_bytes_at_two_addresses_same_digest`
(Review Focus 2); `nodraw_skips_draw_and_upload_and_forwards_shader_info`;
`null_entries_stay_null`; `every_entry_of_the_table_is_wrapped` (a
static assertion on the struct's size against the wrapper's list, so a
new entry fails the build until it is hashed);
`retained_draw_hashes_the_built_contents`. `test_lp64_equiv_streams.py`:
`gfx_names_first_frame_and_field`.
Run: `bash scripts/port/test_gfx_digest.sh`. Expected: FAIL,
`port_gfx_digest.h: No such file or directory`.

- [ ] **Step 2: Implement the wrapper and the hook**

- [ ] **Step 3: Drawing and not drawing give one digest**

On `cmp-a64-video`: `lp64_equiv.py run --set solo --only dam --video`
with and without `--draw`, twice each. Expected: four identical
`dam.gfx` files. A difference means the interpreter reads something
back from the renderer; name the entry and forward it.

- [ ] **Step 4: Cost on the emulated reference, and the stop rule**

Run `--set solo --only dam --video` once on `cmp-i686-video` with
`--timeout-scale 10`. Record wall seconds. If the 3,900 ticks take more
than 1,200 s, stop and bring the figure to the owner (decision 4); the
plan's later steps assume less.

- [ ] **Step 5: Sensitivity**

`lp64_equiv.py sensitivity --video` on `cmp-a64-video`. Expected:
`OK: sim, gfx and snd differ between seeds` (`snd` from Task 9; until then `sim` and
`gfx`).

- [ ] **Step 6: Task check; commit**

```bash
git commit -m "Hash what the renderer is asked to draw, a line a frame"
```

---

### Task 8: Picture equivalence (item 6)

**Files:**
- Create: `scripts/port/lp64_equiv_pictures.py`,
  `scripts/port/test_lp64_equiv_pictures.py`
- Modify: whatever each cause names (strict tier on the branch);
  `scripts/port/lp64_equiv_expected.txt`; `FORK_CHANGES.md`

**Interfaces:**
- Consumes: Task 7; `hand_gallery.py`'s `FIXTURE_INI`, `CASES` and its
  capture variables, by import (the file is not edited and `generate`
  is never called here).
- `lp64_equiv_pictures.py shots DIR_A DIR_B`: for each case's
  screenshot at its pinned tick, `CASE: identical` or `CASE: N of M
  pixels differ, largest channel difference D`.
- `lp64_equiv_pictures.py gallery --rom ROM --output DIR EXECUTABLE`:
  runs the gallery's capture mode offscreen and writes the 216
  screenshots' hashes to `DIR/gallery.txt`; `gallery-compare A B` prints
  how many of 216 differ.
- The rule (the design's): equal digests and different pixels are the
  rasterizer's and are recorded with their size; different digests are
  a port defect or an explained difference.

- [ ] **Step 1: Write the failing test**

`shots_reports_pixel_count_and_largest_difference` on two generated
4-by-4 images; `gallery_compare_counts_differing_cases`;
`missing_shot_fails_not_differs`. Expected: FAIL, `No module named
'lp64_equiv_pictures'`.

- [ ] **Step 2: The primary check**

Run step 9 with `--streams sim,gfx`, both profiles (`RUN`
`video-default` with `--video --profile default`, then `video-all` with
`--video --profile all`), all 38 cases, `--repeat 2`. Expected: each
build agrees with itself; `sim` under the second profile is identical
between i686 and x86_64 or classified as in Task 6; and for each case
`gfx` is identical or classified the same way (`--env
GE_GFXDIGEST_DETAIL=A-B` names the call). A difference in
`vtx` only, with `sim` identical, points at float code in the renderer
or in a fork feature's drawing; name the function. Then run the `solo`
set on `cmp-i686-video-libm` and `cmp-x64-video-libm` with
`--maths-off`: each first differing frame there is recorded as the C
library's once `GE_GFXDIGEST_DETAIL` shows a value that `tanf` or
`atanf` fed.

- [ ] **Step 3: The secondary check, screenshots**

Run the 21 `solo` cases and `temple-4p` with `--video --draw --shots
1800` on `cmp-i686-video` and `cmp-x64-video`, default profile, then
`shots`. Expected: each case identical, or its pixel count recorded as
the rasterizer's because its `gfx` is identical.

- [ ] **Step 4: The secondary check, the gallery**

`gallery` on `cmp-x64-video` twice (expected: 216 of 216 equal, as
measured on aarch64) and once on `cmp-i686-video`, then
`gallery-compare`. Expected: 0 of 216 differ, or each differing case is
named and classified by the rule. This run is long on the emulated
build (decision 4 has the estimate); it runs once.

- [ ] **Step 5: The render thread**

Lockstep turns the thread off, and a paced stage run does not repeat,
so this uses a demo, which its recording paces: `dam1` on
`cmp-a64-video` with `--env GE_RENDERTHREAD=1` and without. Expected: for
every tick both `gfx` files hold, the lines are equal, and at least
nine tenths of the ticks are in both; record the result either way
(decision 12).

- [ ] **Step 6: Task check; commit**

```bash
git commit -m "Record where each case's picture matches the 32-bit build"
```

---

### Task 9: Sound that depends only on the game (item 7)

**Files:**
- Create: `port/src/port_audio_lock.c`, `port/include/port_audio_lock.h`,
  `scripts/port/test_audio_lock.c` (run by `test_lp64_compare.sh`)
- Modify: `port/src/audio_pc.c`, `CMakeLists.txt`,
  `scripts/port/test_lp64_compare.sh`,
  `scripts/port/lp64_equiv_streams.py` (`snd`),
  `scripts/port/test_lp64_equiv_streams.py`,
  `scripts/port/lp64_equiv_expected.txt`, `FORK_CHANGES.md`

**Interfaces:**
- `s32 portAudioLockFrames(s32 field_now);` in a comparison build with
  `GE_AUDIO_LOCK=1`, the audio frames due: the fields since the last
  call, 0 on the first; -1 when the lock is off. It includes no SDL
  header, so a test can call it. Without `GE_COMPARE`,
  `port_audio_lock.h` defines `portAudioLockFrames(f)` as `(-1)`, so
  the shipped `audio_pc.c` compiles the call to today's path.
- The pump (`audio_pc.c`) asks it first. When it answers 0 or more, the
  pump mixes exactly that many frames of the game's frame size and does
  nothing by the queue's level or the clock (no top-up, no trim, no
  clock path); the log holds `ge: audio: locked to game fields` once.
  `GE_AUDIO_CAPTURE` then records a file whose length is a function of
  the fields played. When it answers -1 the pump is today's.
- `first_difference('snd', a, b)`: the first differing sample, with its
  audio frame and that frame's field; a file that is not 22,050 Hz
  stereo, or two files of different lengths with equal common part,
  reports the length as the difference.

- [ ] **Step 1: Write the failing test**

`test_lp64_equiv_streams.py`: `snd_names_first_sample_frame_and_field`;
`snd_reports_length_when_one_is_longer`. `test_audio_lock.c`:
`one_frame_a_field` (fields 5, 6, 6, 9 give 0, 1, 0, 3);
`off_without_the_variable` (-1); `off_outside_a_comparison_build` (the
header included without `GE_COMPARE` gives -1).
Expected: FAIL, `port_audio_lock.h: No such file or directory`.

- [ ] **Step 2: Implement the lock**

- [ ] **Step 3: The recording agrees with itself**

`lp64_equiv.py run --set solo --only dam --video --repeat 2` on
`cmp-a64-video`. Expected: two byte-identical `dam.wav` files (today
two paced recordings of one build differ in length, 41.57 s and
39.50 s). Peak at least 1,000, as the `sound` set requires.

- [ ] **Step 4: Compare**

Step 9 with `--streams snd`, all 38 cases, both profiles (`RUN`
`snd-default` with `--video --profile default`, then `snd-all` with
`--video --profile all`). Expected: identical between i686 and
x86_64, or the first differing sample is traced to its voice (`GE_SNDDBG`) and classified: bank or
sequence conversion (a 64-bit defect, fixed with a fixture in
`test_romconv.sh`), a float in the resampler (shown with both values),
or the simulation (the case's `sim` already differs at that field).

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Lock the comparison build's sound to game fields and compare it"
```

---

### Task 10: The mission smoke with video under the sanitizers (item 8)

**Files:**
- Modify: `scripts/port/audit_buffers.py`,
  `scripts/port/test_audit_buffers.py`, `AGENTS.md`, `FORK_CHANGES.md`

**Interfaces:**
- `audit_buffers.py --mission-smoke EXE --rom ROM --video [--frames N]`:
  the 20 missions on a `GE_VIDEO` executable with
  `lp64_play_run.VIDEO_VARIABLES`, a `HOME` and icon cache in each
  folder and the per-case `midas.ini` that `lp64_play_run` writes
  (`[display] fullscreen=windowed`), a wall-clock limit of
  `lp64_play_run.timeout_seconds(frames)` in place of the headless
  120 s, and a pass only if the log also holds `ge: video: window + GL
  context up` and 30 pictures or more.

- [ ] **Step 1: Write the failing test**

`video_smoke_sets_offscreen_variables`;
`video_smoke_fails_under_thirty_pictures`. Expected: FAIL,
`unrecognized arguments: --video`.

- [ ] **Step 2: Implement**

- [ ] **Step 3: Run it**

Build `build/lp64/video-san` (`-DGE_POINTER_BITS=64 -DGE_VIDEO=ON
-DGE_SANITIZE=ON`) and run `--mission-smoke ... --video --frames 1800
--jobs 4` in the aarch64 container. Expected: 20 of 20 `PASS`, no
sanitizer marker.

- [ ] **Step 4: The comparison cases under the sanitizers**

Build `cmp-a64-san` and `cmp-a64-video-san`; run all 38 cases once on
each (`--video` on the second). Expected: `OK: 38 of 38 cases ran` with
no sanitizer marker. A report is a defect of the code it names; fix it
with a failing test first, as LP64-005 did.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Run the mission smoke with video under the sanitizers"
```

---

### Task 11: The soak, and budgets set from it (item 9)

**Files:**
- Modify: `scripts/port/lp64_play_peaks.py`, `scripts/port/lp64_play.py`
  (`peaks --budget`), `scripts/port/test_lp64_play_peaks.py`, the
  source of each pool the table raises, `port/src/port_lp64.c` and the
  files that gain a peak hook, the four drivers that stub the hooks
  (`scripts/port/test_audio_synth.c`, `test_lp64_poison.c`,
  `test_mema_order.c`, `test_memory_banks.c`), `AGENTS.md`,
  `FORK_CHANGES.md`
- Create: `scripts/port/lp64_budgets.txt`

**Interfaces:**
- Consumes: `portLp64PoolUse` and the `ge: lp64: peak` lines;
  `GE_SOAK`, `GE_MEMSTATS`.
- Pool sizes are a stage's: `memallocstringtable` (`src/boss.c`) gives
  each level its graphics buffer, vertex pool, texture pool and room
  memory (the soak saw room memory from 276,480 to 716,800 bytes). So
  peaks are kept by stage. `lp64_play.py peaks --by-stage DIR...`
  attributes each `ge: lp64: peak` line to the level id of the last
  `ge: load_bg_file levelid=` line before it (0 for the front end). It
  reads the `runK/` subfolders of an `lp64_equiv.py` folder as well as
  `lp64_play.py`'s flat ones.
- `lp64_budgets.txt`: one line a pool and stage, `POOL LEVEL SIZE PEAK
  CASE`, written from the runs below.
- `lp64_play.py peaks --budget FILE DIR...`: exit 1, naming the pool
  and level, when a pair in the logs is not in FILE, when its size
  differs from FILE's, or when its peak is above half its size.
- The rule (decision 8): at 64 bits every pool's size in every stage is
  at least twice its largest measured peak there; none is lowered.

- [ ] **Step 1: Write the failing test**

`by_stage_attributes_a_peak_to_the_level_before_it`;
`budget_fails_a_pool_past_half`; `budget_fails_an_unlisted_pair`;
`budget_fails_a_changed_size`; `budget_passes_at_exactly_half`;
`by_stage_reads_run_subfolders`.
Expected: FAIL, `unrecognized arguments: --by-stage`.

- [ ] **Step 2: Every pool the design names has a peak line**

The design lists room display lists, the graphics buffer, model and
setup allocations, texture pools, the permanent bank and the music
heap. Hooked today: stage bank, room memory, graphics buffer, vertex
pool, music heap, texture pool. Add `portLp64PoolUse` for the permanent
bank, `gun.c`'s three model rooms, the wallet's room and the
synthesizer's parameter pool, or record why one cannot overflow. Update
the four drivers' stubs in the same commit.

- [ ] **Step 3: The soak**

On the plain aarch64 executables, headless and with video:
`GE_SOAK=all:600:4` with `GE_MEMSTATS=1` (a three-lap headless run took
731 s); the `tours` and `stages` sets (all 13 arenas, 2 and 4 players,
Temple's seven other scenarios); the 38 comparison cases; the `attract`
set. Expected: every run passes with no `ge: lp64: pool` line.

- [ ] **Step 4: Explain the memory statistics**

Resident memory grew 10,240 KB a lap in the planning run while private
memory and every pool stayed level. The i686 build cannot answer here:
under qemu-i386 the statistics are the emulator's (private memory
4,334,016 KB in the planning run). So find it in the 64-bit build: in
harness builds, at each lap's end, log each mapping of
`/proc/self/smaps` whose resident size grew since the last lap. Name
what touches it afresh (the converters' new memory is the first place
to look). A leak is fixed with a check that a fifth lap adds nothing;
pages the allocator keeps are recorded with the figure. Whether the
32-bit build grows alike is a question for decision 5's session.

- [ ] **Step 5: Set the budgets** (Review Focus 5)

`lp64_play.py peaks --by-stage` over every folder of Step 3. For each
pool and stage whose largest peak is above half its size, raise it at
64 bits to twice that peak, rounded up to its alignment, with the peak
and its case in a comment beside the figure. From the figures so far
that is at least room memory in Aztec (68.2 percent in its tour) and
the texture pool in one soak stage (53.1 percent with video); the
by-stage table will name the rest. Write `lp64_budgets.txt`.
Run: `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt
DIR...`. Expected: `OK: N pool and stage pairs within budget`.

- [ ] **Step 6: What a raised pool costs**

Rerun Step 3's soak and the comparison cases' `sim` and `gfx` on
`cmp-a64` and `cmp-a64-video`. Expected: still within budget; `sim`
unchanged; `gfx` unchanged against the i686 digest, which is the
reference. A raise whose `gfx` then differs from the i686 digest (a
larger room budget can keep a room the smaller one dropped) is
withheld: the pool keeps its pre-raise size in that stage, and the
figure goes to the owner with the rooms named. Run
`late_load_audit.py` on the stages whose budgets changed and compare
with the report from before.

- [ ] **Step 7: Task check; commit**

```bash
git commit -m "Set the 64-bit pool sizes from the soak, twice each peak"
```

---

### Task 12: The ROM-free tests at both widths (item 10)

**Files:**
- Modify: `scripts/port/test_harness_widths.sh`,
  `scripts/port/test_start_input.sh`, those of the five harnesses below
  that gain a second width,
  `.github/workflows/windows-release.yml`, `FORK_CHANGES.md`

**Interfaces:**
- `test_harness_widths.sh` gains a list check: every
  `scripts/port/test_*.sh` that compiles a file under `src/` or `port/`
  either sources `harness_widths.sh`, or runs at the compiler's own
  width and then at 32, or is on a short list with its reason
  (`test_pointer_bits`: reads CMake's flags; `test_web_touch`,
  `test_web_menu_pointer`: the browser's compiler); and every harness
  is named in a workflow or on a second list with its reason. Five more
  compile `port/` code at the host's width only today:
  `test_baked_trace`, `test_enhanced_fonts`, `test_hd_textures`,
  `test_open_fonts`, `test_headless_configure`. Each runs at both
  widths through `harness_widths.sh` where it compiles game C whose
  layout depends on the width, and is otherwise on the short list with
  its reason.

- [ ] **Step 1: Write the failing check**

Expected today: FAIL, naming nine: `test_hitmap.sh`,
`test_image_bank.sh`, `test_room_seams.sh` and `test_start_input.sh` as
run by no workflow, and the five above as running at one width.

- [ ] **Step 2: Put the four in CI's tests step; give
  `test_start_input.sh`'s 32-bit half the SDL shim** (`-Iscripts/port/
  sdl_shim`, as `port_baked_trace.c` gets) or skip that half by name
  where the i386 SDL headers are missing; give each of the five its
  second width or its short-list line.

- [ ] **Step 3: Count**

Run all 37 in the x86_64 container with `MIDAS_SAVE_TEST_I686=1` and in
the aarch64 container. Expected: 36 exit 0 in x86_64 (`test_web_touch`
needs `emcc` and runs in the browser workflow) and 35 in aarch64
(`test_headless_configure` configures a 32-bit tree). Record the table:
harness, 32-bit result, 64-bit result, where CI runs it.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Check that every ROM-free harness runs at both widths and in CI"
```

---

### Task 13: A campaign save moves between widths (item 11)

**Files:**
- Create: `scripts/port/test_save_widths.c`,
  `scripts/port/test_save_widths.sh`
- Modify: `scripts/port/abi_probe/probe.c`,
  `scripts/port/lp64_equiv_cases.py` (set `saves`),
  `.github/workflows/windows-release.yml`, `FORK_CHANGES.md`

**Interfaces:**
- `test_save_widths.c` includes `src/game/file2.c`'s save functions and
  `libultra_shim.c`'s EEPROM, as the typed-read tests include game
  files. With `write DIR` it builds a campaign from numbers in the test
  (folder 1: twelve missions completed on Agent with times, three on
  007, two cheats earned, both volumes changed; folder 3: one mission)
  and saves it; with `read DIR` it loads and prints every field. It
  links `port/src/port_lockstep.c`, which `libultra_shim.c` calls since
  Task 2.
- The probe pins `sizeof(save_data) == 0x60` and `sizeof(smallSave) ==
  32` at both widths.
- Set `saves`: two cases that boot the game on a save folder made by the
  other width's test program and walk to mission select. The folder
  reaches each case through `lp64_equiv.py run --saves DIR` (Task 5).
  The set is outside the 38 cases of the surface.

- [ ] **Step 1: Write the failing harness**

`test_save_widths.sh`: at each width `write` then `read`; then each
width reads the other's folder. Assertions: the two `eeprom.bin` files
are byte-identical; each of the four `read` outputs equals the text
written in the test. Expected: FAIL, `test_save_widths.c: No such file`.

- [ ] **Step 2: Implement the driver and the probe assertions**

Expected: `test_save_widths: PASS (32-bit)`, `PASS (64-bit)`, `PASS
(32 reads 64, 64 reads 32)` in the x86_64 container.

- [ ] **Step 3: With the game**

Run set `saves` on `cmp-a64` with `--saves` naming the 32-bit
program's folder and on `cmp-i686` with `--saves` naming the 64-bit
one's. Expected: each log holds `ge:
eeprom.bin loaded (512 bytes)` and the same `ge: play: menu` events as
the other width; each route picks the thirteenth mission (Depot), which
only a folder with twelve completed offers, and logs its `ge: play:
stage-load`; the file is unchanged afterwards.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Show that a campaign save written at one width loads at the other"
```

---

### Task 14: The Arm check (item 12)

**Files:**
- Modify: `scripts/port/lp64_equiv_expected.txt`,
  `scripts/port/abi_probe/probe.c`, `include/PR/gu.h` (`FTOFIX32`),
  `scripts/port/lp64_diagnostics.py`
  (`gate`), `scripts/port/test_lp64_diagnostics.py`, `FORK_CHANGES.md`

**Interfaces:**
- `lp64_diagnostics.py gate` also refuses a compile command of an
  aarch64 tree without `-ffp-contract=off`. `CMakeLists.txt` already
  gives that flag to every 64-bit tree, and CI's gate trees are x86_64,
  so the rule is exercised by its unit test only.
- The probe asserts `FTOFIX32(40000.0f)` and `FTOFIX32(-40000.0f)` give
  the same `int` on every host (x86 gives `INT_MIN` for an out-of-range
  conversion, Arm saturates), reading the input through a `volatile
  float` so the compiler cannot fold the conversion.

- [ ] **Step 1: Write the failing checks**

`gate_refuses_an_arm_command_without_fp_contract_off`; the two probe
assertions. Expected: the gate test fails (`gate` accepts it today);
the probe fails on aarch64 only, or passes everywhere, which is
recorded.

- [ ] **Step 2: Fix what fails** (an out-of-range `FTOFIX32` gives
  `INT_MIN` on Arm too, as x86 does; the x86 tokens do not move;
  `guMtxF2L` is its user; `gbi.h` uses the macro in constant
  expressions, so the Arm form stays one).

- [ ] **Step 3: Compare**

`lp64_equiv.py compare --expect ... build/lp64/equiv/RUN/x64
build/lp64/equiv/RUN/a64` for each `RUN` of Tasks 6, 8 and 9: `sim`
with `--streams sim`, `video-default` and `video-all` with `--streams
sim,gfx`, `snd-default` and `snd-all` with `--streams snd`. Expected:
identical, or each difference explained as in Task 6. A difference here is between two 64-bit
programs, so its cause is the compiler, the C library or Arm's
arithmetic: show the instruction or the call.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Compare the aarch64 build with x86_64 stream by stream"
```

---

### Task 15: The three checks the hand-over says do not exist

**Files:**
- Create: `scripts/port/lp64_lto.py`, `scripts/port/test_lp64_lto.py`,
  `scripts/port/lp64_lto_allowed.txt`
- Modify: the declarations Step 2 corrects (strict-tier ones on the
  branch), `.github/workflows/windows-release.yml`, `AGENTS.md`,
  `FORK_CHANGES.md`

**Interfaces:**
- `lp64_lto.py list LOG`: each `-Wlto-type-mismatch` warning of a build
  log as `SYMBOL: FILE_A:LINE TYPE_A | FILE_B:LINE TYPE_B`;
  `check LOG` compares with `lp64_lto_allowed.txt` (`SYMBOL reason`),
  exact match, a missing file allowing none.

- [ ] **Step 1: Write the failing test**

`list_reads_a_return_type_mismatch`; `check_fails_an_unlisted_symbol`;
`check_fails_a_listed_symbol_that_is_gone`. Expected: FAIL, `No module
named 'lp64_lto'`.

- [ ] **Step 2: Link once and settle every mismatch**

Configure `build/lp64/lto` (64-bit headless, `-flto=auto
-Wlto-type-mismatch` in the C, C++ and linker flags) and the same with
`-DGE_VIDEO=ON`, in the x86_64 container. Record the two trees' counts
first (97 was the aarch64 headless count). For each warning: make the
declaration match its definition where the two differ in size, sign of
a value used, or pointer against integer; allow the rest by name with a
reason (an array declared without its bound, a libm built-in declared
locally). Expected afterwards: `lp64_lto.py check` prints `OK: N
mismatches, all allowed`, N below the recorded count, with no entry
whose two types
differ in size.

- [ ] **Step 3: Unsigned differences added to pointers** (decision 11)

One scan, kept as a tool only if it finds something: compile the 64-bit
headless tree with `-fdump-tree-gimple` and list each pointer addition
whose offset is the widened result of an unsigned 32-bit subtraction.
Record the count and the command. For each site, show whether the
difference can be negative; fix those with a failing test first. If the
list is empty or all sites are provably non-negative, record that and
commit no tool.

- [ ] **Step 4: Reads through the wrong member of a union**

Commit nothing new unless Tasks 6, 8 or 9 find such a read that
`lp64_overlays.py` did not list: then extend that lister with the kind
that would have caught it, and review the sites it adds. Record which
happened.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Settle the link-time type mismatches and hold them to a list"
```

---

### Task 16: The inherited items

**Files:**
- Modify: `port/src/romconv_model_records.c`,
  `scripts/port/test_romconv_model.c`, `src/game/chraction.c`,
  `scripts/port/test_lp64_typed_chraction.c`, the file Step 3's
  sanitizer report names for the index-0 load, `port/src/video.c`,
  `scripts/port/lp64_overlays_reviewed.txt`, `FORK_CHANGES.md`

**Interfaces:**
- Consumes: `lp64_model_scan.py`, `lp64_setup_scan.py`,
  `GE_LP64_CENSUS=1`.

- [ ] **Step 1: The census, on the landed code**

`lp64_equiv.py run` and the `lp64_play.py` `stages` and `tours` sets,
each with `--env GE_LP64_CENSUS=1`; list the model opcodes and prop
record types loaded. Expected: the fixtures-only lists of the hand-over (opcodes 0,
3, 5, 6, 7, 11, 14, 15, 16, 17, 19, 20; types 0, 15, 16, 31, 41) are
unchanged or shorter; record which.

- [ ] **Step 2: `OP17`**

Test first, in `test_romconv_model.c`: `op17_keeps_position` builds an
`OP17` record with a known hit part, radius and position, converts it,
and reads it back as `ModelRoData_Op17Record`. Expected: FAIL on
`pos.y` or `pos.z` if the hand-over's reading is right. Then give
`OP17` its own map at 64 bits (decision 10). If the test passes
unchanged, record that the reading was wrong and change nothing.

- [ ] **Step 3: The three reads and the index-0 load**

For `chrGetEquippedWeaponProp(...)->chr` at `chraction.c` 1261, 1633
and 1777: a test in `test_lp64_typed_chraction.c` that calls each path
with no equipped prop. Expected: a fault at 64 bits before the guard is
added, none after; the guard sits behind `IS_64_BIT` (decision 10). For
the index-0 load: run the `stops` set on `build/lp64/boot-san`; if the
two bytes past the allocation are reported, bound the read at 64 bits;
if not, record the poisoned gap that covers it.

- [ ] **Step 4: The render thread's exit**

Where `PORT_PLAY_HARNESS` is 1, `portRenderThreadStop`
(`port/src/video.c`) logs `ge: video: render thread stop: waiting
(state S)` before it joins and `ge: video: render thread stop: joined`
after, flushing each; nothing else changes and the 32-bit video build
compiles neither line. Run 301 exits, decision 12's 300 and one more,
with `GE_RENDERTHREAD=1` (the
`menus` set's 7 cases, 43 times) on `build/lp64/video`. Expected: 301
logs that
end with `joined`. A run that stalls leaves its state as its last line,
and the cause becomes this step's work. Record the count for LP64-007.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Close the inherited items the comparison does not reach"
```

---

### Task 17: The laptop, the strict batch, and the close

**Files:**
- Modify: `scripts/port/native_64bit_plan.md` (the status table),
  `AGENTS.md`, `FORK_CHANGES.md`, `scripts/port/late_load_report.py`
  (Step 2, if decision 5's session runs); the gallery through the
  laptop's regeneration (Step 1)

- [ ] **Step 1: Hand the branch to the laptop**

As in the LP64-005 plan's Task 15, with `lp64/equiv-strict`: the
hand-off names every function of the Windows executable that differs
from the last regeneration. If the branch is empty the laptop
regenerates once on `main`.

- [ ] **Step 2: Real x86 hardware, if decision 5 is granted**

On the laptop, in the `goldeneye` container it already builds with:
build `cmp-i686`, `cmp-x64` and `cmp-x64-video`, and `cmp-i686-video`
in the `midas-lp64-i686-video` image (made there by `lp64_env.py
build-images`; the `goldeneye` image has no i386 SDL headers). If that
image cannot be made on the laptop, the i686 side runs the headless
streams only, and FORK-089 says so. Run task check step 9 for those
two with every set and stream, and
`lp64_equiv.py compare --expect`. Expected: the same lines as on the
Mac. Each `host:` line of the expectation file is then confirmed or
removed. The late-load comparison of the 32-bit build by kind and name
runs in the same session (commit its reader in `late_load_report.py`
first). Paste the tool's summary lines into FORK-089; the folders stay
on the laptop.

- [ ] **Step 3: Confirm on the Mac**

`hand_gallery.py verify` reports fresh; CI passes on the landed commit;
the whole task check runs once more with every set and stream (set
`saves` with `--saves`, as in Task 13);
`lp64_play.py peaks --budget` passes over that run's folders.

- [ ] **Step 4: Close the record**

Set LP64-006 to Done in the status table with: each item's result and
host, the expectation file's remaining lines, the budget table, and the
wall time of one full comparison pass. `AGENTS.md`: `lp64_equiv.py`,
lockstep and the three streams, `GE_COMPARE`, `lp64_budgets.txt`,
`lp64_lto.py`, `audit_buffers.py --video`. FORK-089 gets the hand-over
to LP64-007: what this plan added to the play harness in a 64-bit video
build, what real hardware did or did not confirm, the items of "What
this plan does not do", and the owner's list carried forward. Delete
this milestone's folders under `build/lp64` before the push.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Close LP64-006: the 64-bit game matches the 32-bit game"
```

## What this plan does not do

- It does not claim equivalence beyond the design's surface: the 21 solo
  stages, the 14 demos, `temple-4p`, the menu and watch walk and one
  reload, under two profiles. The other twelve arenas and the seven
  other scenarios run (LP64-005's sets) and are not compared.
- It does not soak Cuba or the arenas in laps: `GE_SOAK` knows the 20
  missions, and the others enter the budgets through single loads in
  the `stages` and `tours` sets (Task 11 Step 3).
- It does not run a 64-bit build on a GPU (decision 6) or, unless
  decision 5 is granted, on real x86 hardware.
- It does not change the 32-bit game. Where the 32-bit build is the one
  that behaves oddly (a second load's portal table), it is the
  reference, and the evidence goes to the owner.
- It does not change a shipped build's maths, sound timing or frame
  pacing.
- It does not compare the Windows executable with the Linux ones; item
  3's comparison of the Windows program with itself continues.
- It does not edit `hand_gallery.py`, its template or the gallery.
- It does not fix the owner's open items from LP64-005's close
  (`GE_KILLBOND`, `indy_commands.c`, the replayed pad reaching the watch
  rows, the five defects kept at 32 bits, `-D_scheduler_h_`).
- It does not exercise fix wave C's two paths that need a damaged ROM,
  bound the inherited unbounded reads, port the harnesses to macOS,
  define `A_PAN`, or build with `clang`, GCC 14 or for Windows x64.

## Amendments after the pre-flight scan (2026-11-21)

A read-only scan before Task 1 found 34 conflicts; the coordinator's
rulings changed this text as follows, by finding number.

1. `lp64_equiv.py run` gains `--env`, refusing the runner's own
   variables, with a test (Task 5; Tasks 6, 8, 16 use it).
2. Task check step 9 writes `build/lp64/equiv/RUN/ARCH` (`RUN` `sim`,
   `video-default`, `video-all`, `snd-default`, `snd-all`); Tasks 6, 8,
   9 name theirs and Task 14 Step 3 compares each.
3. Task 6 Step 1: the `unstable` marking is dropped; a demo that cannot
   be made to repeat stops the task for the owner.
4. Task 9 Step 4 compares sound under both profiles.
5. Task 5: `ALL_ON` sets `font_style` to the Iron Sights style and
   `EXCLUDED` holds `texture_pack`, with reasons and a test.
6. Task 11 Step 6: a raise whose `gfx` differs from the i686 digest is
   withheld and goes to the owner with the rooms named.
7. Global Constraints and decision 1: `lp64_play.py` may hold a
   `lockstep=True` case; `--env` refuses `GE_LOCKSTEP`; Task 2's
   `AGENTS.md` edit rewrites the pacing sentence.
8. Task 17 Step 2 builds the i686 video tree in
   `midas-lp64-i686-video`, falling back to headless streams.
9. Task 12 decides five more harnesses' widths; Step 1 names nine.
10. Task 6 Produces allows Step 5's `open: owner` lines.
11. Global Constraints: the harness is 1 in 64-bit video builds;
    LP64-007 decides for packages.
12. Task 1: `test_inputrec_headless.sh` under Modify, `portPlaySleepMs`
    named, `portPlayGunLine(u32)`.
13. `test_lp64_compare.sh` is under Modify in Tasks 3, 4 and 9.
14. Task 3's reader starts with `sim`; Task 7 adds the stream test file
    and `gfx_names_first_frame_and_field`.
15. Task 5 matches the exact `ge: compare: on` line, with a test.
16. Task 5 `sensitivity [--video]` and its output line; Task 7 Step 5
    passes `--video`.
17. Task 7: `T` is `g_GlobalTimer`; retained and framebuffer entries
    forwarded; retained buffers hashed at build, added at draw.
18. Task 9: the header gives `(-1)` without `GE_COMPARE`; the test uses
    it.
19. Task 3: literal `T`, the `pH` form, prop and room fields, an
    example line.
20. Task 10 writes the per-case windowed `midas.ini`.
21. Task 5 `--saves DIR`; Task 13 uses it; set `saves` is outside the
    38; Task 17 Step 3 passes it.
22. Task 13's driver links `port_lockstep.c`.
23. Task 11 `peaks` reads `runK/` subfolders, with a test.
24. Task 7: `test_gfx_digest.sh` runs at both widths.
25. Task 14: `include/PR/gu.h` under Modify, a `volatile` probe, the
    gate rule's coverage stated, the Arm form a constant expression.
26. Task 2: the host-clock test's assertion and Step 4's command.
27. Decision 4: the drawn checks run once, in Task 8.
28. "What this plan does not do": no soak laps for Cuba or the arenas.
29. Task 15 records the x86_64 counts first; N is below them.
30. Task 16: the index-0 file named by the sanitizer report, the
    typed-read test under Modify, 301 exits.
31. Task 17 gains Files and a commit step.
32. No change: the commit attribution stays as written.
33. Review Focus 2 and Task 5 Step 4 record `randomize_va_space`.
34. The file map lists every existing file a task edits.

## Amendments during Task 5 (2026-11-21)

Task 5's first Step 4 (`build/lp64/equiv/step4`, `cmp-a64`) printed
`FAIL: 22 of 38 cases did not run`, and later measurements contradicted
two of the plan's premises. These are the controller's rulings, recorded
for the owner at the milestone's close. The Task 5 text above stays as
accepted; where it conflicts, these rulings govern.

1. **The shot rule.** The plan's 30 shots was arithmetic. The route has
   30 trigger windows, but most missions start with a pistol that fires
   once a window, and the opening takes the first windows. On the first
   route the counts were caverns 140; 29 for thirteen missions; 28 for
   dam, surface, silo and `dam-twice`; 20 for frigate; 0 for bunker2,
   archives and `temple-4p`, which start unarmed (`ge: play: weapon 1`,
   and punches are not counted); and cuba has no player control at all.
   A route with fire now needs `SHOT_FLOOR` = 15 shots and `moved`. Each
   case's measured count is recorded as a figure with its date and
   commit, not judged for equality. The three unarmed cases need `moved`
   only, and cuba needs only its stage load and frame limit. An armed
   case measured under 15 would get its own lower floor, never 0 (none
   does). `sensitivity` uses the same judge.
2. **Routes sidestep; they do not turn.** The plan's C-button turn legs
   do nothing in a headless replay. Such a replay acts on `P` records
   only (`port/src/inputrec.c`), and nothing in a `P` record turns
   Bond. Measured on `cmp-a64` in a lockstep Dam run, holding each input
   from tick 1300 to 1600:
   - stick X at -80 moved Bond sideways, from (20167.4, 16892.6) to
     (19881.7, 17526.0);
   - C-left left him where he was;
   - his facing stayed at 90.3 in every run.

   The legs therefore hold stick X at full deflection (80), left or
   right by the sign of the benchmark's turn list, for 45 ticks with
   stick Y released: they are sidestep legs. The route has no `M`
   record and no C button, and the test is `route_uses_pad_records_only`.
   Each lockstep case keeps one facing for its whole run, and the 14
   demos are the cases that turn. Turning in a lockstep route would need
   an `M` record or a look-stick field that headless replay acts on.
   That is game C in input recording, and not this milestone's.
   (Ruling 2 first asked for stick-X turns; it was replaced once the
   measurement above showed that stick X sidesteps.)
3. **A demo's stream is its playback.** Ten of the fourteen paced demos
   did not agree with themselves. Each still exited at its measured
   tick, but `timer` (the retraces consumed) and the trace's line count
   followed the host's pace. Examples:
   - facility1 first differed at tick 4, with 2,012 lines against 2,013;
   - runway2 at tick 6, with a seed difference at tick 2060, outside its
     playback;
   - the others were runway1, frigate1, facility3, frigate2, dam2,
     bunker1, facility2 and dam1.

   In harness builds a `T` line now ends in `demo=K` while a recording
   replays, where K is the recording's tick (`port_simtrace.c`, with a
   seven-line accessor under `PORT_PLAY_HARNESS` in
   `src/game/ramromreplay.c`). The reader kind `sim-demo` keys a demo's
   lines by K and compares every field except `n` and `timer`. The
   runner compares demos with it but still prints the stream as `sim`,
   and a demo trace with no `demo=` line fails.
4. **The split.** Comparing moved to `lp64_equiv_compare.py`, and
   `lp64_equiv.py` stays the entry point with the same command lines.
5. **The fonts gate.** `--profile all` sets `GE_IRONSIGHTS_FONTS=1`,
   because `font_style=2` loads as Original without it. `--env` refuses
   that variable.

## Amendments during Task 7 (2026-11-21)

Task 7's runs on `cmp-a64-video` changed four details of the brief. The
Task 7 text above stays as accepted; where it conflicts, these govern.

1. **A demo's picture stream is its playback.** While a recording
   replays, an `F` line ends with `demo=K` (K from
   `portRamromReplayTick`). The reader kind `gfx-demo` keys a demo's `F`
   lines by K and compares every field except `n` and `tick`; a stream
   with no such line is unreadable. The runner reads demos with it and
   still prints the stream as `gfx`. Measured before keying: `dam1` run
   twice with `--video` wrote 453 `demo=` frames in each run, one for each
   K from 1 to 453, and all 14 demos (28 files) wrote one frame for each K.
2. **Textures are named by their contents.** With texture ids named by
   the order the backend handed them out, the two `dam1` runs differed
   from K = 13 in `state` alone, at a `select_texture`. The frames before
   a recording plays follow the host's pace, and the texture cache's new
   ids, recycled ids and frees follow those frames. A texture is now the
   hash of its last upload. Each draw hashes the textures its shader
   samples into `tex`, so `one_texel_changes_tex_only` still holds.
   Bookkeeping (queries, lookups, new and freed ids, buffer and
   framebuffer creation) hashes into no part; detail lines mark it
   `part=none`. A retained draw is its descriptor's built contents, never
   the buffer's handle.
3. **What `GE_GFXDIGEST_NODRAW` forwards.** It also forwards
   `update_framebuffer_parameters`, since `get_clip_parameters` answers
   from it, and `set_texture_filter`, which `get_texture_filter` answers.
   It forwards the frees (`delete_texture`, `retained_destroy`,
   `clear_shaders`) and the frame's own entries (`init`, `on_resize`,
   `start_frame`, `end_frame`, `finish_render`). Drawing and not drawing
   gave one digest on the first try.
4. **`snd` waits for its lock.** Two `dam` runs of one build recorded
   different sounds (5,203,820 and 5,196,524 bytes, first differing at
   byte 4). Self-agreement and `sensitivity` therefore judge `sim` and
   `gfx` until Task 9's lock (`JUDGED_VIDEO_STREAMS` in `lp64_equiv.py`).
   The `.wav` file is still required, and `compare --streams snd` still
   compares it byte for byte.

These are the controller's rulings, recorded for the owner at the milestone's close.

## Amendments during Task 8 (2026-11-22)

Task 8's runs changed six details of the brief. The Task 8 text above
stays as accepted; where it conflicts, these govern.

1. **Two phases, fixes first.** The first full runs showed three causes
   of difference before any could be classified, so phase A fixed them
   (commits of their own, each with its test and the regression checks)
   and phase B ran Steps 2 to 5 on the fixed code and recorded what was
   left.
2. **Step 3 samples six of the 22 cases.** One drawn case took 3,319 s
   on the emulated i686 build alone, so the 22 would not fit the run.
   Step 3 runs `stage-dam`, `stage-frigate` and `stage-jungle` (the
   cases with a known difference or fix), `stage-facility` (indoor,
   many rooms and portals), `stage-cradle` (the last mission of the
   campaign) and `temple-4p`. The other 16 are compared by digest only;
   on the 32-bit side they have no screenshot.
3. **Comparison builds wait for the bake and time the console by
   pictures.** Under `GE_COMPARE` only, Baked Lighting finishes its
   trace during the stage load instead of across frames by the wall
   clock, and the console's message dwell counts pictures instead of
   reading `osGetTime`. Shipped builds are unchanged.
4. **`image.c` reads zero at 64 bits only.** An odd-width 4-bit texture
   packs each row's last pixel with the byte after the decoder's output;
   a 64-bit build zeroes that scratch buffer, and the 32-bit program
   keeps its leftovers, so its pictures differ from the 64-bit builds'
   in the cases that draw such a texture (recorded as explained).
5. **Explosive Rounds' row numbers are fixed at every width.** The
   rounds were numbered 22 to 24 against a table of 24 rows (0 to 23),
   so a guard's round read past it. They are now 21 to 23; the 32-bit
   Windows executable differs from before in six `.text` bytes
   (`portExplosiveRoundsType` +0x34 and +0x41, `explosionCreate` +0x12,
   +0x1a and +0x1b, `explosionInflictDamage` +0x5f5).
6. **What phase B changed.** A comparison build draws a fixed build
   stamp on the legal screen and the file select, since the stamp
   carries the minute its tree was built. The C library's differences
   in Step 2 are attributed by a scratch build pair with the in-tree
   `tanf` and libm's `atanf` and `cosf`, which match, rather than by
   detail lines. The video libm pair's folders are `i686-libm-video`
   and `x64-libm-video`: its `sim` differs from the headless libm
   pair's, which keeps the name `i686-libm:x64-libm`. Step 3's i686 runs
   were stopped by the per-case limit after picture 1800, six at a
   time, so the 32-bit side has screenshots and digests to that picture
   and no complete run.

## Amendment by the owner: three checkpoints (2026-11-21)

Asked how to shorten the run, the owner chose to run the task check's
steps 1 to 8 in full at three checkpoints instead of after every task
("Yes, three checkpoints"). The rule is in "Task check" above. It
changes when the regression check runs, not what done means: decision 9
and Task 17 Step 3 stand.

## Decisions for the owner

Thirteen, each with a recommendation and what the other way costs. The
tasks are written for the recommendations. **The owner accepted all
thirteen recommendations on 2026-11-21.**

1. **How item 5's "fixed seed and tick" is read.** `lp64_play.py`
   refuses `GE_FIXEDTICK` because a demo then ends at another tick, and
   the planning runs show a stage run repeats neither paced nor with
   `GE_FIXEDTICK`. *Recommended:* demos keep their recorded pace and
   stay with `lp64_play.py`, which keeps refusing both flags; stage
   routes run in a new lockstep mode through a separate runner (Tasks 2
   and 5). `lp64_play.py` may also hold a case marked `lockstep=True`,
   its `--env` refuses `GE_LOCKSTEP`, and Task 2's `AGENTS.md` edit
   rewrites the sentence on its pacing (clarified after the pre-flight
   scan, 2026-11-21). *The other way:* fixed tick for everything costs
   the demos (dam1 ends at tick 455, not the recorded 438); paced runs for
   everything cost the rule that a reference agrees with itself.
2. **Which maths the comparison builds share.** The design says one
   in-tree `sinf` and `cosf`. Measured on this C library: `sinf`,
   `cosf`, `expf` and `powf` give the same bits on all three hosts;
   `tanf` and `atanf` do not. *Recommended:* replace only `tanf` and
   `atanf`, keep a sweep that fails if any of the six parts ways on a
   host, and compare with the option off as well. *The other way:*
   replacing `sinf` and `cosf` too follows the design's letter and makes
   the comparison builds differ from the shipped game from the first
   sine, so their equivalence says less about what players run.
3. **Where the picture digest sits.** *Recommended:* a wrapper around
   the renderer's function table, in `port/src/`, compiled only in
   comparison builds and chosen in `video.c`; no strict-tier file
   changes and no shipped build holds it. *The other way:* a digest
   inside `port/fast3d/` is closer to the draw calls and puts renderer
   edits on the strict branch, reviewed through the gallery, for code
   no player runs.
4. **Pictures and sound on the emulated 32-bit build.** A demo's 3,000
   frames with drawing took 769 s there. *Recommended:* compare digests
   and sound without drawing (Task 7 measures the cost first; above
   1,200 s a case it stops and comes back to you), and run the drawn
   checks once, in Task 8 (clarified after the pre-flight scan,
   2026-11-21): 22 screenshots (about 45 minutes if four run
   at once at that rate) and the gallery's 216 captures (about an hour
   by the same rate; neither is measured). *The other way:* leaving the
   drawn checks to the laptop saves that time and makes item 6's
   secondary check wait for decision 5.
5. **Real x86 hardware.** The design wants a divergence seen only
   under emulation confirmed on real hardware, and this Mac has none.
   The Windows laptop already builds through the `goldeneye` container
   (`hand_gallery.py` says so; not checked on the laptop), where the
   i686 and x86_64 Linux builds would run natively. *Recommended:* one
   session there in the milestone's laptop trip (Task 17 Step 2),
   running the two builds' comparison once.
   *The other way:* closing on the Mac alone costs nothing now and
   leaves every `host:` line, and the late-load comparison, to
   LP64-007.
6. **A GPU.** *Recommended:* not in this milestone. The digest shows
   the two builds ask for the same picture; what a driver draws from it,
   and the frame-end check's cost, belong with LP64-007's packaged
   build. *The other way:* a 64-bit Linux build on the laptop's card
   through WSL is new setup with no result this milestone needs.
7. **Item 8's tool.** The item names `audit_buffers.py
   --mission-smoke` "with video"; the tool is headless only.
   *Recommended:* add `--video` (Task 10) and also run the comparison
   cases under the sanitizers. *The other way:* accepting the `stages`
   set with video, which passed at LP64-005's close, needs no code and
   leaves the item's named command unable to do what it says.
8. **The budget rule.** LP64-005 raised a pool only past 80 percent.
   *Recommended:* final sizes give every pool, in every stage, twice
   its largest peak there over the soak, the tours, the comparison
   cases and every multiplayer scenario. From today's figures that
   raises at least Aztec's room memory (68.2 percent) and one stage's
   texture pool (53.1): a few hundred KB of a stage bank that peaks at
   9.5 percent of 66.8 MB. *The other way:* keeping 80 percent changes
   nothing and leaves Aztec's room memory with less than half again its
   peak.
9. **What "done" means.** *Recommended:* on `main`: the task check
   passes, steps 1 to 9; every stream of all 38 cases agrees with itself
   on each build; `lp64_equiv_expected.txt` has no `open:` or `strict:`
   line, and each line it has names its first index, cause and removing
   run; the screenshots and the gallery are equal or classified as the
   rasterizer's; the mission smoke with video and the 38 cases pass
   under the sanitizers; the soak passes and `peaks --budget` passes;
   the harness table is recorded and CI runs every harness; a save
   moves both ways; aarch64 matches x86_64 or each difference is
   explained; `weapons-dam` passes 26 of 26; the Windows executable is
   identical to bfc0271's or every changed function is explained; CI is
   green and the gallery fresh. A `host:` line may remain only if
   decision 5 is refused, and is listed by name at the close.
10. **Defects the 32-bit game has too** (`OP17`'s map, three NULL
    reads, the index-0 load). LP64-005's decision 13 allowed a few-line
    fix at both widths; its coordinator then kept five such fixes to 64
    bits. *Recommended:* the same here: each gets a failing test and a
    fix behind `IS_64_BIT`, and joins the owner's list with the 32-bit
    fix described. *The other way:* fixing both widths changes the
    Windows program in functions no stock file reaches.
11. **The two checks without a tool.** *Recommended:* scan once for
    unsigned differences added to pointers and keep the scan only if it
    finds a real site; extend the overlay lister only if a comparison
    finds a union read it missed. *The other way:* two standing tools,
    each with its tests and a list to maintain, against defects the
    three streams and the sanitizer runs already hunt at run time.
12. **The render thread.** Lockstep turns it off, as LP64-005's judged
    passes did. *Recommended:* compare with it off, check once that it
    draws the same lines when on (Task 8 Step 5), and give the exit
    stall two log lines around the join, in harness builds only, and
    300 counted exits (Task 16 Step 4). *The other way:* a comparison
    with the thread on needs a second lockstep design for two threads;
    a hunt for the stall without a reproduction has no end condition.
13. **One laptop trip, at the end.** No task plans a strict-tier edit.
    *Recommended:* one trip in Task 17: the regeneration, the strict
    branch if it holds anything, and decision 5's session. Say if you
    would rather make a first trip as soon as a comparison puts its
    first fix on the branch: `main`'s expectation file would otherwise
    carry `strict:` lines until the end.
