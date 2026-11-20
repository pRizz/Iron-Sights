# LP64-005 First Playable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-17 from measurements on `main` at 98a76dd
and accepted by the owner the same day, with all fourteen
recommendations under "Decisions for the owner" at the end (the first,
the reading of "no `ge: lp64:` line", in the session that closed
LP64-004; the other thirteen together, "Accept all 13"). Nothing is
built; execution starts at Task 1. The tasks are written for the
recommendations. The status table of
[native_64bit_plan.md](native_64bit_plan.md) records what is built and
verified as the tasks land.

**Goal:** A 64-bit build that replays the 14 attract demos and loads every
stage the ROM can serve, first headless and then with the renderer and
sound running, checked by tools that can fail, without changing the 32-bit
program.

**Architecture:** The milestone is run-driven. One new tool,
`lp64_play.py`, runs named sets of cases (demos, stages, stops, menus,
tours, the attract loop, scripted input, sound) on an executable and judges
each by what its log must and must not contain, headless or with video.
Two things then make those runs find what the earlier runs could not: the
sanitizer build poisons the game's own arenas so that an overrun inside one
allocation is reported, and each demo's per-tick trace is compared with the
32-bit build's, which the measurements show finds records read by position.
Defects are fixed at the declaration, each with a check that failed first.
Video runs in the existing containers through SDL's offscreen driver and
Mesa's software renderer. Work in the gallery's strict-tier files is
batched on one branch and lands through one regeneration on the Windows
laptop.

**Tech Stack:** C and C++ (GCC 13.3, Ubuntu 24.04 containers), CMake,
Python 3 standard library, Bash test harnesses, Docker Desktop on Apple
Silicon, SDL 2.30 (offscreen video, dummy audio), Mesa 25.2 llvmpipe.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md): milestone LP64-005
("Headless 64-bit replays every attract demo and loads every stage; then
the same with video"), "Design > Display lists", "Memory budgets",
"Verification" items 5, 7, 8 and 9 (which LP64-006 completes and this
milestone starts), "Working machine" and the hazard inventory. Read it
first, then `AGENTS.md`, [the LP64-004 plan](native_64bit_lp64_004.md) for
the task check and the rules this plan continues, and in `FORK_CHANGES.md`
(FORK-089) the passage "LP64-004 hand-over to LP64-005" and the note
"LP64-004 task 17". Search that file; its lines are very long.

## What was measured for this plan (2026-11-17, `main` at 98a76dd)

All runs used the Mac's containers (`lp64_env.py`, GCC 13.3). aarch64 is
native; x86_64 and i686 run under emulation, so nothing seen only there is
a finding yet (the design's rule). Scratch builds and one scratch copy of
the tree (a `git archive` with four stand-in edits, described below) lived
under `build/lp64` and were deleted afterwards. The working notes, with
every figure, are outside Git in the planning sessions' shared notes
folder (`lp64-005-planning-notes.md`).

### How a run is started, with what exists today

- **Demos.** `GE_DEMO=<name>` plays one of the 14 recordings after 120
  menu ticks (`src/game/front.c`, `portDirectLaunchDemoTick`): dam1, dam2,
  facility1, facility2, facility3, runway1, runway2, bunker1, bunker2,
  silo1, silo2, frigate1, frigate2, train. `GE_DEMODIAG=1` prints one
  `ge: demo tick N draws=D live=XX rec=YY ...` line a tick (the random
  seed the game has against the one the N64 recorded), and
  `GE_RNGTRACE=<tick>` prints the caller of every random draw in two ticks.
  A demo's end prints `ge: demo exit: REASON at tick N`. The process does
  not exit when a demo ends: the attract loop goes on (logo, cast, another
  demo), and only `GE_FRAMES` ends the run.
- **Two flags must not be used.** With `GE_FIXEDTICK=1` a demo advances one
  field a tick instead of its recorded pace: dam1 then ends by its
  terminator at tick 455 after 454 vsyncs, where the real replay ends at
  tick 438 after 1,407. With `GE_NOSLEEP=1` the frame limit, which counts
  retraces, passes before the game has done anything: 4,000 frames in half
  a second with no stage loaded. Runs are therefore paced in real time, 60
  retraces a second.
- **Stages.** `GE_STAGE=<name|level id>` for a solo load; `GE_MP=<arena>`
  with `GE_MPPLAYERS=2..4` and `GE_MPSCENARIO=0..7` for multiplayer (13
  arenas); `GE_MENU=files|select|options|gameplay|control|display|credits`
  for a front-end screen; `GE_LOADTOUR=24` to stand Bond in every room of
  a stage in turn; `GE_SOAK=all:600:1` to play the 20 missions one after
  another in one process, through the title between them;
  `GE_KILLBOND=<tick>` to kill player 1.
- **A headless build cannot press a button.** `osContGetReadData`
  (`port/src/libultra_shim.c`) zeroes every pad, and the input merge and
  the `GE_INPUTPLAY` replay hook sit inside `#ifdef GE_VIDEO`. Demos work
  because the game's own playback writes the pads.
- **Video needs no display server.** With `SDL_VIDEODRIVER=offscreen` a
  64-bit `GE_VIDEO` executable opens its window and a GL 4.5 compatibility
  context on Mesa's llvmpipe in both images as they are (Mesa 25.2.8 is
  already installed there). `SDL_VIDEODRIVER=dummy` cannot make a GL
  context and the game stops with `ge: FATAL: Could not open SDL window
  ...`. A run needs a writable `HOME` and `GE_ICON_CACHE_DIR`.

### Headless, 64 bits

- **The 14 demos play to their end.** Plain and under the sanitizers
  (aarch64), 3,000 frames each: every run exits 0 with no sanitizer report,
  and each demo ends with `totaltime end (timer/limit)` at the tick the
  i686 build ends it: dam1 438, dam2 445, facility1 410, facility2 538,
  facility3 380, runway1 388, runway2 388, bunker1 482, bunker2 771, silo1
  501, silo2 503, frigate1 328, frigate2 384, train 464. Fourteen demos
  take 100 seconds, seven at a time.
- **But the 64-bit game is not the 32-bit game from the second tick.** The
  i686 build stays exact with the N64's recorded seed through the whole of
  seven demos (facility1, facility2, facility3, runway2, bunker1, silo2,
  train) and parts from it at tick 217, 44, 301, 332, 280, 116 and 64 in
  dam1, dam2, runway1, bunker2, silo1, frigate1 and frigate2. The 64-bit
  build parts from it at tick 2 in all fourteen, having made more random
  draws in the first tick of play: 111 against 76 in Dam, 196 against 146
  in Facility, 33 against 23 in Runway, 75 against 54 in Bunker, 245
  against 179 in Silo, 156 against 117 in Frigate, 144 against 110 in
  Train.
- **The cause is a record read by position.** The caller trace of Runway's
  first tick shows ten extra draws in `chrlvIdleAnimationRelated`: every
  armed guard takes the unarmed branch. `weaponIsOneHanded`
  (`src/game/chraction.c:343`) casts a weapon's record to `ChrRecord` and
  reads `act_bytes.padding[84]`. A probe compiled at both widths gives that
  byte at 128 at 32 bits, which is `WeaponObjRecord.weaponnum`, and at 140
  at 64 bits, where `weaponnum` is at 144. No sanitizer can see this, and
  two reading sweeps of LP64-004 and a third made for this plan missed it.
- **With four stand-in edits in a scratch copy** (that read taken through
  `WeaponObjRecord`; the three below), dam1, runway1 and runway2 trace
  identically to the i686 build for every tick, and runway2 stays exact
  with the recording for all 417 ticks at 64 bits. The other eleven first
  differ from the i686 trace at tick 227 (dam2), 406 (facility1), 35
  (facility2), 114 (facility3), 113 (bunker1), 325 (bunker2), 186 (silo1),
  54 (silo2), 125 (frigate1), 158 (frigate2) and 70 (train). The aarch64
  and the emulated x86_64 64-bit traces are identical to each other for
  all fourteen, so these are differences between 64 bits and i686, not
  between processors. One was looked at: in facility2 at tick 34 the i686
  build makes one draw at `gunfire.c:4419` (the recoil kick, taken when the
  hand's `field_890` is 0) that the 64-bit build does not make. It was not
  diagnosed further. `-ffp-contract=off` is already set for 64-bit builds
  (`CMakeLists.txt:41`).
- **Three more positional reads, found by reading and confirmed by the
  probe:** `chraction.c:2240`, `:2258` and `:2306` take a hit animation's
  frame count as `((u16 *) header)[2]`, which at 64 bits is half of the
  header's first pointer (`unk04` is at 8 there);
  `src/game/unk_092E50.c:271` and `:277` write the mip-map list's fifth
  command as `((u32 *) list)[8]`, which at 64 bits is the third command;
  `src/game/options.c:564` reads a font record as `s32` words (a debug
  cursor). `src/sched.c:334` has a fourth that is dead on PC.
- **Multiplayer and Cuba run; twelve stage numbers stop by name.** 1,800
  frames each, plain and under the sanitizers: all 13 arenas with 2 and
  with 4 players, Temple with 4 players in each of scenarios 1 to 7 (33
  starts) and Cuba (the credits stage) exit 0 with no report. Twelve stop
  with status 1: a solo load of Citadel, Complex or Temple (`ge: FATAL:
  The setup file UsetupcatZ is not served from the ROM; ...`, `UsetuprefZ`,
  `UsetupdishZ`) and level ids 42, 44, 47, 49, 51, 52, 53, 55 and 56
  (`ge: FATAL: The bg file bg/bg_sho_all_p.seg is not served from the ROM;
  ...` and its eight siblings). "The eleven cut stages" of LP64-004 are
  eleven background files the ROM does not serve, nine of which `bg.c`'s
  level table names (these nine level ids); `bg_imp` and `bg_ash` have no
  level id (measured in the LP64-005 fix wave, FORK-089).
- **The i686 build on those twelve:** Temple solo runs 1,800 frames;
  Complex solo and the nine ids end in a segmentation fault; Citadel hangs
  (no frame in 150 seconds).
- **An inherited stack overrun on the way to three of the stops.** Under
  the sanitizers the three setup stops are preceded by
  `stack-buffer-overflow ... in strncpy src/str.c:17`, called from
  `indyrescmdCheckFileExists` (`src/game/indy_commands.c:203`), which
  `fileGetIndex` reaches for a file the ROM does not hold. It is in the
  32-bit game too.
- **Every room of every stage.** `GE_LOADTOUR=24` on the 20 missions and
  six arenas: all 26 exit 0 with no `ge: lp64: pool` line. Eleven tours
  visit every room; fifteen end early with `(the mission ended there)`
  (Surface 2 after 0 of 37 rooms, Statue 6 of 25, Surface 11 of 35, Bunker
  2 24 of 46, Facility 50 of 65 and ten more), 848 room visits in all.
- **Paths that need no button, 30,000 frames each (eight minutes), plain
  and under the sanitizers:** `GE_SOAK=all:600:1` plays the 20 missions in
  one process, through the title between them, and ends `ge: soak:
  complete after 1 laps`; the attract loop with no launch variable shows
  the logos and the cast and starts five demos; Facility with all nine
  `GE_CHEATS` names; each of the seven `GE_MENU` screens, file select
  with the wallet's model among them. All exit 0 with no sanitizer report
  and no pool line. A four-player Temple match with `GE_KILLBOND=600`
  runs the death of player 1 and then waits: nothing respawns without a
  button, so `mp_respawn_handler` has still not run at 64 bits. Under the
  sanitizers that case ran slower than real time (17 to 35 frames a
  second) and finished its 30,000 frames with no report.

### With video, 64 bits

- **It configures, compiles and links.** A 64-bit `GE_VIDEO=ON` tree builds
  in 17 seconds in the aarch64 container and 53 in the x86_64 one; with
  `GE_SANITIZE=ON` as well it links in aarch64 (a 72 MB executable, which
  compiles in `GE_LP64_DL_CHECK`).
- **The renderer walks 64-bit display lists.** Offscreen, the plain
  executable ran the title for 300 frames and drew 68 pictures, exit 0;
  the sanitizer executable ran it for 900 frames and drew 166 with no
  sanitizer report and no display-list fault. A screenshot of Dam's
  opening shot (`GE_SHOT`, kept local) shows the dam, sky, water and
  crates as they should be.
- **Every run that makes sound fails.** Dam died with signal 11 in
  `alEnvmixerPull` (`src/libultrare/audio/env.c:94`), the runway1 demo in
  `__allocParam` (`src/libultra/audio/synthesizer.c:247`), and a
  four-player Temple match ran 1,800 frames printing `ASSERTION FAULT -
  samples >= 0 - env.c, line 104`. Under the sanitizers Dam stops at
  `env.c:94:20: runtime error: member access within misaligned address
  ... for type 'struct ALParam'`.
- **The cause is the storage family again.** The synthesizer's parameter
  pool holds records of `sizeof(ALParam)`, 32 bytes at 64 bits
  (`synthesizer.c:127`). `alSynStartVoiceParams` takes one and writes an
  `ALStartParamAlt` into it, 40 bytes there, whose `wave` pointer lands on
  the next record's `next` link. Both are 28 bytes at 32 bits. It happens
  at every music note. The pool lies inside the static music heap, so the
  sanitizers saw only the wreckage.
- **With a stand-in for that** (one pointer of room in `ALParam`), the
  sanitizer video executable ran the title, Dam, Facility, the runway1
  demo, four-player Temple and `GE_MENU=select` and `options` for 900
  frames each with no report and no display-list fault. They drew only 8
  to 71 pictures each: under the sanitizers, four runs at a time, llvmpipe
  draws less than one picture a second. The plain executable draws about
  nine a second (256 pictures in 1,500 frames of Dam).
- **And the sound it then makes is silence.** `GE_AUDIO_CAPTURE` over
  1,500 frames of Dam recorded 11.9 seconds in which every sample is zero.
  The log has, once, `ge: audio: WARNING - audio frame skipped (silent):
  command 15 op 0x04 has address 0x...`; the warning prints only the first
  time, and every such frame is dropped. `GE_MUSIC_EXPORT=dam` fails at 0.0
  seconds with the same line. Opcode 4 is the sample load, and
  `portAudioCommandFits` (`port/include/port_audio_bounds.h`) refuses it
  when the buffer count set before it does not fit the mixer's memory, so a
  sample count is wrong before the command is built. The cause was not
  found. The music heap holds 331,632 of 753,664 bytes with sound running.
- **There is no 32-bit control with sound on the Mac yet.** A 32-bit
  `GE_VIDEO` tree does not compile in the `goldeneye` image as plainly
  configured (`SDL2/_real_SDL_config.h: No such file or directory`); the
  Linux release build has its own route to the i386 SDL
  (`scripts/port/linux_build.py`).

### The storage family, by reading

An agent read `src/` and `port/` for the family's patterns and counted
them: about 70 bank allocations (25 with a literal size, none holding a
pointer or a display list), 90 `dynAllocate` calls, 44 `alHeapAlloc`, 78
casts to `Gfx *`, 387 `memcpy` and `bzero` lines, 95 indexed reads through
a scalar cast of a record. It found no new overrun, and the four
positional reads above. It did not find `weaponIsOneHanded`, which a demo
trace found, or the parameter pool, which the first run with sound found.
Reading has now missed a member of this family four times.

What a run can and cannot see today: `CMakeLists.txt:106` compiles all C
with `-fcommon`, so about 144 global arrays without an initializer are
common symbols and get no sanitizer red zone; the stage bank, the room
heap, the graphics pool and the music heap are each one allocation; and
nothing in the tree uses the sanitizer's manual poisoning. Each allocator
has one place where it could: `portMempAllocate` and `cursorAfter`
(`port/src/port_memp.c`), `memaAlloc` and `_memaFree` (`src/mema.c`),
`portDynAllocate` (`port/src/port_dyn.c`) and `alHeapDBAlloc`
(`src/libultra/audio/heapalloc.c`). A rough count gives 379 casts to a
record pointer type in 31 files.

### The renderer, by reading

`roc_part_key` (`port/fast3d/gfx_roomcache.inl:155`) shifts a list's
address left by 20 bits and mixes in the vertex address, which at 64 bits
drops the address's upper bits; the key is the cache's whole identity, and
the cache is used only with `room_cache=2` (the default is 0). `phVector`
(`port/src/port_hud.c:349`) copies 4-byte floats into the 8-byte words of
three commands and leaves their upper halves as they were; only the
renderer's own case for that opcode reads them. Nothing else was found:
segment tables, texture cache keys and list strides are pointer-sized.

### macOS

On the Mac itself, with clang and `MIDAS_HARNESS_WIDTHS=64`, 10 of the 33
`scripts/port/test_*.sh` harnesses pass. Thirteen stop at
`include/PR/os.h:1033:13: error: expected parameter declarator`; the rest
on GNU linker options (`--gc-sections`), GCC-only warning names, `-msse2`,
`port/src/port_lp64.c`'s `#error` for a platform without an image range,
and a missing `emcc`.

### What could not be measured

- Any path that needs a button with a 64-bit build: pause, the watch
  pages, the briefing, a mission chosen through file select, an aborted or
  completed mission.
- Sound that is not silence at 64 bits, and so whether it is the right
  sound; a 32-bit recording to compare it with.
- More than a few dozen pictures a run under the sanitizers; the arm and
  hand code's alias cases; `room_cache=2`.
- Peaks of any pool: the tree has no high-water counter, only the
  `ge: lp64: pool` line when one is exhausted.
- The late-load audit (it wants a 32-bit and a 64-bit video build side by
  side) and the cost of the graphics buffer's frame-end check.
- Real x86 hardware, a GPU, `clang`, GCC 14, the browser build (CI only).

## The hand-over, item by item

Every item of "LP64-004 hand-over to LP64-005" and of the LP64-004 row's
closing lists, with where this plan puts it. "Measured" means the plan's
own runs above already executed the path at 64 bits without a fault.

| Hand-over item | Where it goes |
|---|---|
| The reading of "no `ge: lp64:` line" | Accepted by the owner on 2026-11-17: a rule of this plan (Global Constraints), no longer a decision |
| `bondview2.c`'s intro walk after a WATCH record | Decision 2; Task 8 runs the walk (a respawn needs a button) |
| The wallet Bond's room | Decision 3; Task 12 |
| Attract demos | Measured (14 of 14 play). Tasks 1, 5 and 6 |
| The attract cast | Measured headless (the attract loop loads the cast's models and goes on to five demos). Task 1's `attract` set; with video in Task 11 |
| Multiplayer | Measured (33 starts; a death, but no respawn without a button). Tasks 7 and 8 |
| Cheats, stage transitions, file select and the other `GE_MENU` screens | Measured headless without input. Tasks 1 and 7 |
| Pause, the watch pages, the briefing, a mission ended or aborted | Not reachable without input. Task 8 |
| The cut stages | Measured (they stop by name). Decision 4; Task 7 |
| Play beyond 1,800 frames, room streaming, peak room memory | Tours measured (848 room visits, no pool line). Tasks 1 and 13 |
| The renderer's walk, `GE_LP64_DL_CHECK`, `seg_addr`'s 64-bit test | First runs measured (no fault in few pictures). Task 11 |
| The arm mesh's alias cases (`port_arm_mesh.c`) | Task 11 |
| Sound synthesis, the mixer, `Acmd`, `aPan`, the bank views, `GE_AUDIO_CAPTURE` | Measured: a crash, then silence. Task 10. `aPan` stays unreachable: `A_PAN` is defined nowhere |
| The pause list, the watch background lists (73308dd) | Task 8 runs both. The watch lists get Task 2's proof (a reverted fix must be reported); the pause list lies inside the player record, where no hook sees, and keeps the probe's room assertion |
| The gun barrel list (73308dd, `title.c`) | Task 2's Step 3 finds the case that reaches it (the attract loop is the candidate; the planning runs did not check) and proves the fix on it |
| The briefing's bank span (a657b4b), pinned by reading only | Task 8 (a run) and Task 12 (a probe assertion) |
| Fix wave C: stops for a file the ROM does not serve | Measured (12 stops). Task 7's `stops` set |
| Fix wave C: a missing ROM segment; a compressed text bank zeroed | Not in this milestone (see that list) |
| `PROPDEF_UNK41` out of the 64-bit prop fix-up | Task 7's census step; else fixtures only, for LP64-006 |
| The storage family is not exhausted | Tasks 2, 3 and 4. Measured: one new overrun (the synthesizer's parameter pool) and four reads by position |
| Model opcodes and prop record types only fixtures cover | Task 7's census step over the new runs; the rest to LP64-006 |
| Pool figures are snapshots; room memory and the mema heap unscaled; what the pool line's N means | Task 13; decision 10 |
| The graphics buffer's frame-end check owes a benchmark | Task 13 |
| Inherited unbounded reads at both widths (animation stream, sequence, AI list end, glyph extent, wave type) | Not in this milestone |
| The 32-bit special-portal walk's dependence on link order | Not in this milestone |
| Stale background pointers on a stage without `load_bg_file` | Task 12 |
| Two `GE_DUMP_BOND` diagnostics (`model.c`, `sky.c`) | Task 12 (`sky.c`); the strict batch (`model.c`) |
| The model converter's opcode rule (`G_MOVEWORD`, a crafted `G_TEXRECT` tail) | Task 12 (the rule's text); the crafted input: not in this milestone |
| For macOS: `test_audio_buffers.c`'s `mmap` flags; the bank cursor's alignment | Task 12 asserts the arena's alignment; the tests: not in this milestone (measured above) |
| The late-load audit of a 64-bit build | Task 14 |
| LP64-004's decision 6 measurement (a Dam reload leaves 334 portal fields) | LP64-006, unchanged |
| Stale wording in the guard (`port_lp64.c`) | Task 12 |
| One CI line cannot run on the Mac (`MIDAS_BUFFER_SANITIZE=1 ... test_buffers.sh`) | Unchanged: its two halves run apart on the Mac, and CI is its evidence |
| A model slot request that returns `NULL` (`model.c`, strict) | The strict batch, only if a run shows it (LP64-004's rule) |
| `FTOFIX32`, the two `atof` reads | LP64-006, unchanged |

## What LP64-005 owns, and what it leaves to LP64-006

LP64-005 owns: every stage and demo running at 64 bits, headless and with
video; input and sound running at all; the tools that make those runs
repeatable; the storage family searched by run and by a reviewed list;
and every defect those runs show that stops a run, corrupts memory or
changes what the game does.

LP64-006 owns equivalence: the per-frame picture digest, sound compared
sample by sample, input recordings across every stage, the soak and the
pool sizes it sets, saves moved between widths, the aarch64 against x86_64
comparison as a formal check, and real x86 hardware.

One thing straddles the line and is placed deliberately (decision 5): the
demos' per-tick traces are compared with the i686 build's in this
milestone, because that comparison found a defect in the first tick of
play that nothing else could see. Each difference is diagnosed here. One
whose cause is arithmetic (a C library result, an operation's rounding) is
recorded with its tick and call site and handed to LP64-006, which has the
test-only shared maths option to remove it.

## Global Constraints

- **The 32-bit program does not change.** Before every commit,
  `python3 scripts/port/compare_win32.py commits HEAD WORKTREE` exits 0.
  Where it exits 1, the rule and the accepted kinds of difference are those
  of [the LP64-003 plan](native_64bit_lp64_003.md#global-constraints):
  every changed function is named and read, and the commit says why.
- **Fix the declaration, not the symptom.** A positional read becomes a
  read of the member it means, through the type the object has. Storage is
  sized by `sizeof` of what it holds (`AGENTS.md`). A record that must
  overlay another by position is pinned at both widths in the layout probe
  (`scripts/port/abi_probe/probe.c`).
- **Every 64-bit branch sits behind `IS_64_BIT`** where the 32-bit tokens
  would otherwise change; where a typed read compiles to the same 32-bit
  code, one form serves both widths and the Windows comparison shows it.
- **Runs are judged by what they did, not by their exit status.** A case
  names the lines its log must have (a launch line, a demo's start and
  exit, pictures drawn) and must not have (a sanitizer marker, a failing
  `ge: lp64:` line, `ge: fatal signal`, `ASSERTION FAULT`, `audio frame
  skipped`). Demos never run with `GE_FIXEDTICK`; nothing runs with
  `GE_NOSLEEP`.
- **The reference must agree with itself first** (the design's rule). A
  demo's i686 trace is used only if two i686 runs of it are identical.
- **Sound is verified by recording, not by arithmetic** (`AGENTS.md`).
- **No `ge: lp64:` line but the informational ones** (the owner's
  reading of LP64-004's decision 3, accepted on 2026-11-17). A 64-bit run
  prints no pool, refusal, converter or stop line; the kinds on
  `INFORMATIONAL_PREFIXES` (`scripts/port/lp64_boot.py`) are allowed, and
  the tools fail any other `ge: lp64:` line. A new kind that a passing run
  prints is added to that list in the same commit, with its reason
  (`AGENTS.md`); this plan adds one, `ge: lp64: peak` (Task 13).
- **Sanitizer-only code changes no shipped build.** The poisoning hooks
  expand to nothing unless the compiler defines `__SANITIZE_ADDRESS__`;
  `-fno-common` is given only with `GE_SANITIZE`.
- **Renderer formats are fixed**; **no design may depend on low
  addresses**; **no laundering casts**: as in the LP64-004 plan. The
  narrowing and unprototyped checks stay at their counts (0 and 2), and the
  cast allowlist at 24 in 11 files unless a line is removed.
- **Upstream-owned files get small mechanical edits.** New logic goes in
  `port/`. FORK-089 records each file touched.
- **Strict-tier files** (`src/game/gunfire.c`, `model.c`, `bondview2.c`,
  everything under `port/fast3d/`, `port/src/port_arm_mesh.c`,
  `port_native_arms.c`, `port_grip_mesh.c`, `port_wardrobe.c`,
  `ads_hands.c`, `port_knife_ads.c`, `port_graphics.c`, `port_shiny.c`,
  `port_weapon_images.c`, `port_hud_weapons.c`, `joy_port.c`,
  `scripts/port/hand_gallery.py` and its template) are changed only on the
  branch `lp64/play-strict` (see "The strict batch").
- **Render performance and load before play** (`AGENTS.md`). No task adds
  per-frame drawing or first-use work to a shipped build. The peak counters
  of Task 13 are one comparison and one store a frame at 64 bits; their
  cost is measured with the frame-end check's, which LP64-004 left owing.
- **Input idioms and settings placement** (`AGENTS.md`). No task adds a
  setting, a menu row or a prompt. The scripted-input hook reads a file
  named by an environment variable and is not a player-facing input.
- **No ROM-derived data in Git.** Traces, logs, recordings and screenshots
  stay under `build/lp64/` and are deleted before each push; what a record
  relies on is written into `FORK_CHANGES.md` as text. Test inputs are
  built in the test from numbers written there. Python uses the standard
  library only. New source files stay under 500 lines. Shell scripts start
  with `#!/usr/bin/env bash` and `set -euo pipefail`.
- **Every commit updates `FORK_CHANGES.md`** (FORK-089): what changed, the
  check counts, what ran.
- Delivery is direct to `main`, one push per task after its review is
  clean, except commits on `lp64/play-strict`. Never use
  `git commit --no-verify`. Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

### The strict batch

Planned for the branch: nothing by name. Candidates the measurements point
at: the cause of facility2's difference at `gunfire.c:4419` (Task 5), the
room cache key (`gfx_roomcache.inl`, Task 11), `model.c`'s `GE_DUMP_BOND`
read (Task 12), and decision 2 if the owner chooses the fix.

- A commit that touches a strict-tier file goes to `lp64/play-strict`,
  never to `main`. From its first commit the branch is rebased on `main`
  after every push to `main`, and every 64-bit run of the task check is
  made on the branch.
- A check on `main` that needs a strict-tier fix to pass says so in its
  expectation file (a demo's line in `lp64_demo_traces.txt` reads
  `differs-from TICK strict: REASON`) until the branch lands.
- Each strict-tier commit leaves the Windows executable identical, or
  names every changed function for the laptop.
- The branch lands once, in Task 15, through one regeneration on the
  Windows laptop. If the branch is empty the laptop still regenerates once.

### Task check

Every task ends by running this and recording the results in FORK-089.
Steps 1 to 7 are the LP64-004 plan's task check as its dated note of
2026-11-17 has it (`--fixed-tick --frames 1800` for steps 2, 5 and 6),
with `lp64_boot.py --expect-run --frames 600` as step 4 and all ten
classes in step 5. Step 8 exists from Task 1 and grows as sets are added;
`SETS` is the list the task names.

```bash
# 8. The play sets, 64 bits: plain and sanitizer headless (aarch64), and
#    from Task 11 the same with video. The i686 reference traces once a
#    task that changes game code.
python3 scripts/port/lp64_env.py run --arch aarch64 -- python3 scripts/port/lp64_play.py run --rom /rom/baserom.u.z64 --set SETS --output build/lp64/play/plain build/lp64/boot/IronSights
python3 scripts/port/lp64_env.py run --arch aarch64 -- python3 scripts/port/lp64_play.py run --rom /rom/baserom.u.z64 --set SETS --output build/lp64/play/san build/lp64/boot-san/IronSights
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/lp64_play.py run --rom /rom/baserom.u.z64 --set demos --jobs 4 --output build/lp64/play/ref build/lp64/ref-i686/IronSights
python3 scripts/port/lp64_play.py traces --expect scripts/port/lp64_demo_traces.txt build/lp64/play/ref build/lp64/play/plain
```

Expected: each `run` prints one `NAME: PASS` line a case and
`OK: N of N cases`; `traces` prints `OK: 14 demos, I identical, D
differing as recorded`. Also run every harness line of the CI tests step
(`Test gameplay, cache and release tooling without a ROM` in
`.github/workflows/windows-release.yml`), in the i686 container where a
line needs `-m32`. Run step 8's first line once in the x86_64 container at
each task that changes C. The browser build is checked by CI after the
push; a red browser build is the task's to fix.

### Behaviour-sensitive sites

- `weaponIsOneHanded` (`chraction.c:343`): the byte it means is
  `WeaponObjRecord.weaponnum`. Other code reads a weapon through a
  `ChrRecord` name on purpose (`src/bondtypes.h:2420` describes
  `attack_item` overlaying `weaponnum`); Task 3 lists each such site, and
  none is changed without its own evidence. (Tasks 3, 4)
- The hit animation's end frame (`chraction.c:2240`, `:2258`, `:2306`): at
  32 bits `((u16 *) header)[2]` is `unk04`; the typed read must compile to
  the same load there. (Task 4)
- The parameter pool (`synthesizer.c:127`): its record must hold the
  largest view written into it. At 32 bits the three views and `ALParam`
  are 28, 16, 16 and 28 bytes, so a record sized by a union of them is the
  same size and the program does not change. (Task 10)
- `ALSndpEvent` (`src/snd.c`) is copied as an `ALEvent`; both are 32 bytes
  at 64 bits by coincidence. Pinned in the probe, not changed. (Task 10)
- The pad a headless build reads: today all zeros. With `GE_INPUTPLAY`
  unset it stays all zeros, so the i686 reference and every existing
  headless run are unchanged. (Task 8)
- `indyrescmdCheckFileExists`'s `strncpy` writes past a stack buffer at
  both widths. Bounding it changes the 32-bit function; the commit names
  it. (Task 7, decision 13)
- The poisoning hooks must not change what a plain build allocates: block
  addresses and sizes in a build without the sanitizer are those of
  98a76dd. In a sanitizer build each bank block is followed by a 32-byte
  gap, so its layout differs from the plain build's on purpose. (Task 2)
- `-fno-common` turns two definitions of one global into a link error.
  `table_for_portals` is defined in `src/game/bg.c:5923` and in
  `bg_portal_table.c:6`; the fix is one `extern`, and the Windows
  comparison must stay identical. (Task 2)

## Review Focus

1. A case passes because the game did nothing: the frame limit was reached
   with no stage loaded, a demo never started, a route never reached the
   pause it was written for. Expected: every case requires the line that
   proves its subject ran (a launch line, `demo start` and `demo exit`,
   `ge: play:` events from Task 8, a minimum of pictures with video).
   (Tasks 1, 8, 9.)
2. Sound "runs" and is silent, or drops frames quietly. Expected: the
   `sound` set fails on `audio frame skipped` and on a recording whose
   peak is 0, and the ROM-free synthesizer test asserts non-zero output at
   both widths. (Task 10.)
3. The poisoning unpoisons too much and finds nothing. Expected: with each
   of the known fixes reverted in a scratch copy, the sanitizer run
   reports the overrun; the reports are recorded. (Task 2.)
4. A fix for a positional read changes the 32-bit program. Expected: the
   Windows comparison is identical, or the changed function is read and
   explained. (Tasks 4, 5.)
5. Traces are equal by luck of timing, or differ by it. Expected: the i686
   reference is run twice and must agree with itself; a demo that does not
   is marked `unstable` and proves nothing. (Tasks 1, 5.)
6. The reviewed-overlay list rots: a line number moves and a new
   positional read hides behind an old verdict. Expected: sites are keyed
   by file, function and the cast's text, and a changed text is a new
   site. (Task 3.)
7. A pool is too small in real play and nobody knows by how much.
   Expected: every run prints each pool's peak, and the close lists them
   against their sizes. (Task 13.)

---

### Task 1: The play check

**Files:**
- Create: `scripts/port/lp64_play.py`, `scripts/port/test_lp64_play.py`,
  `scripts/port/lp64_demo_traces.txt`
- Modify: `.github/workflows/windows-release.yml` (the test),
  `AGENTS.md` (the tool, beside `lp64_boot.py`), `FORK_CHANGES.md`

**Interfaces:**
- `lp64_play.py run --rom ROM --set NAME[,NAME...] [--video] [--jobs N]
  [--only CASE] [--env NAME=VALUE]... --output DIR EXECUTABLE`: runs each case of the named sets
  on a copy of EXECUTABLE in a fresh temporary folder, with every `GE_`
  variable of the caller dropped and `GE_ROM`, `GE_SEED=1`,
  `GE_NOINISAVE=1`, `ASAN_OPTIONS=detect_leaks=0` and the case's own
  variables set; writes `DIR/CASE.log` and `DIR/play.json`; prints
  `CASE: PASS` or `CASE: FAIL (first failing line or missing line)`, then
  `OK: N of N cases` (exit 0) or `FAIL: M of N cases` (exit 1). Default
  `--jobs` 7. It imports `failing_lp64_lines`, `SANITIZER_MARKERS` and
  `FATAL_SIGNAL` from `lp64_boot.py`.
- A case fails on: a status other than the one it expects; any sanitizer
  marker; `ge: fatal signal`; `ASSERTION FAULT`; a failing `ge: lp64:`
  line; a missing required line; a timeout.
- The sets of this task (later tasks add `input` and `sound`):

  | Set | Cases | Variables | Frames | Required |
  |---|---|---|---|---|
  | `demos` | the 14 names | `GE_DEMO=NAME`, `GE_DEMODIAG=1` | 3000 | `ge: demo start:`; `ge: demo exit: totaltime end (timer/limit) at tick T` with T the tick measured above; the frame-limit line |
  | `stages` | 20 missions, `cuba`; 13 arenas with 2 and 4 players; `temple` with 4 players and scenarios 1 to 7 (54 cases) | `GE_STAGE`, `GE_STAGE_AFTER=1`, `GE_CHEATS=invuln,invis`, `GE_LP64_CENSUS=1`; or `GE_MP`, `GE_MPPLAYERS`, `GE_MPSCENARIO` | 1800 | the direct-launch line; the frame-limit line |
  | `stops` | solo `citadel`, `complex`, `temple`; level ids 42, 44, 47, 49, 51, 52, 53, 55, 56 | `GE_STAGE`, `GE_STAGE_AFTER=1` | 1800 | status 1; `ge: FATAL: The setup file U...Z is not served from the ROM` or `The bg file bg/bg_..._all_p.seg is not served from the ROM`, with the file name of the measurement |
  | `menus` | the 7 `GE_MENU` values | `GE_MENU=NAME` | 900 | `ge: GE_MENU harness: menu=`; the frame-limit line |
  | `tours` | 20 missions, 6 arenas (`temple`, `complex`, `caves`, `library`, `basement`, `stack`, 2 players) | `GE_LOADTOUR=24`, `GE_CHEATS=invuln,invis` | 16000 | `ge: loadtour: done, V of R rooms` with V at least the count measured for that stage |
  | `attract` | `attract` (no launch variable), `soak` (`GE_SOAK=all:600:1`), `death` (`GE_MP=temple`, 4 players, `GE_KILLBOND=600`), `cheats` (`GE_STAGE=facility` with all nine `GE_CHEATS` names) | as named | 30000 | `attract`: two `ge: demo start:` and two `ge: demo exit:` lines; `soak`: `ge: soak: complete after 1 laps`; the others the frame-limit line |

  The measured ticks and room counts are constants in the tool, each with
  the date and commit of its measurement. Rooms visited of rooms listed,
  2026-11-17 at 98a76dd: dam 54 of 56, facility 50 of 65, runway 11 of 11,
  surface 11 of 35, bunker1 21 of 24, silo 64 of 65, frigate 35 of 42,
  surface2 0 of 37, bunker2 24 of 46, statue 6 of 25, archives 55 of 59,
  streets 54 of 54, depot 33 of 36, train 50 of 50, jungle 43 of 44,
  control 57 of 66, caverns 45 of 46, cradle 28 of 29, aztec 55 of 55,
  egypt 26 of 26; arenas temple 9 of 9, complex 23 of 23, caves 10 of 10,
  library 28 of 28, basement 28 of 28, stack 28 of 28. Task 7 finds out
  why fifteen tours end early (the 32-bit build's counts are the
  reference) before these become the floor. The tool never sets
  `GE_FIXEDTICK` or `GE_NOSLEEP`, and refuses a caller's `--env` of either.
- `lp64_play.py traces --expect FILE DIR_REF DIR_NEW`: for each demo, the
  `ge: demo tick` lines of the two logs; prints `NAME: identical (N
  ticks)` or `NAME: first differs at tick T`, and compares with FILE.
  `lp64_demo_traces.txt` has one line a demo, `NAME identical` or
  `NAME differs-from T REASON`; exit 0 only if every demo is exactly as
  its line says (a demo recorded as differing that now matches fails too,
  so the file is lowered when a fix lands). `--reference-check DIR_A
  DIR_B` compares two runs of one build and prints `unstable` for a demo
  that differs from itself.
- `lp64_play.py callers --tick T LOG EXECUTABLE`: the `ge: rng draw` lines
  of a `GE_RNGTRACE=T` log as `tick function file:line`, through
  `addr2line`, subtracting the `executable image` address of the log's
  `ge: lp64: address space OK` line when the executable is
  position-independent.

- [ ] **Step 1: Write the failing test**

`test_lp64_play.py`, with stand-in executables written by the test (shell
scripts that print a canned log and exit, as `test_lp64_boot.py` does):
a demo case passes a log with the start line, the exit line at the
expected tick and the frame-limit line; fails one whose exit tick differs;
fails one with no `demo start` line though it exits 0; fails one that has
`ASSERTION FAULT`; a `stops` case passes status 1 with its FATAL line and
fails status 0; a tour case fails `done, 5 of 59 rooms` when 55 are
expected; `run` refuses `--env GE_NOSLEEP=1`; `traces` on three pairs of
sample logs (identical; differing at tick 35 and recorded as such;
differing at 35 but recorded `identical`) exits 0, 0 and 1;
`--reference-check` prints `unstable` for a pair that differs; `callers`
turns two sample lines into `tick function file:line` against a stand-in
`addr2line`.

Run: `python3 scripts/port/test_lp64_play.py`
Expected: FAIL, `No module named 'lp64_play'`.

- [ ] **Step 2: Implement the tool**

- [ ] **Step 3: Record today's state as the first expectation file**

Run step 8 of the task check with `--set demos` for the reference (twice,
then `traces --reference-check`), the plain and the sanitizer executable.
Write `lp64_demo_traces.txt` from it: on `main` today every demo is
`differs-from 2 weaponIsOneHanded reads a weapon record by position
(Task 4)`.
Run: step 8 with `--set demos,stages,stops,menus,tours,attract`.
Expected: `demos`, `stages`, `menus`, `tours` and `attract` pass on the
plain executable; `stops` passes plain and fails under the sanitizers on
its three setup cases (the `strncpy` report), which Task 7 fixes. Record
the failing cases by name in FORK-089; a set that fails for a named,
planned reason does not block this task's commit.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Add the play check: sets of 64-bit runs judged by what they did"
```

---

### Task 2: Let the sanitizers see inside the arenas and the common symbols

**Files:**
- Create: `port/include/port_poison.h`, `scripts/port/test_lp64_poison.c`
- Modify: `CMakeLists.txt` (`-fno-common` with `GE_SANITIZE`),
  `port/src/port_memp.c`, `src/memp.c` (the bank reset), `src/mema.c`,
  `port/src/port_dyn.c`, `src/game/dyn.c` (the buffer swap),
  `src/libultra/audio/heapalloc.c`, `src/game/bg.c` or
  `src/game/bg_portal_table.c` (one `extern`), any other global defined
  twice, `scripts/port/test_buffers.sh`, `scripts/port/buffer_audit.md`,
  `FORK_CHANGES.md`

**Interfaces:**
- `port_poison.h`: `PORT_POISON(address, bytes)` and `PORT_UNPOISON(address,
  bytes)`, which call `__asan_poison_memory_region` and
  `__asan_unpoison_memory_region` when the compiler defines
  `__SANITIZE_ADDRESS__` and are `((void) 0)` otherwise;
  `PORT_POISON_GAP`, 32 with the sanitizer and 0 without.
- Bank (`memp`): a bank reset poisons the whole bank; `portMempAllocate`
  unpoisons exactly the bytes asked for and leaves `PORT_POISON_GAP` bytes
  poisoned after the block; `portMempResize` re-poisons what a shrink gives
  back and unpoisons what a growth takes. Room heap (`mema`): a reset
  poisons the heap, `memaAlloc` unpoisons the block it returns, `_memaFree`
  poisons it. Graphics pool (`dyn`): the half not being written is
  poisoned at each swap, and `portDynAllocate` unpoisons each block. Music
  heap (`alHeapDBAlloc`): poisoned at `alHeapInit`, each block unpoisoned.
- CMake: `GE_SANITIZE` adds `-fno-common` to the C flags.

- [ ] **Step 1: Write the failing test**

`test_lp64_poison.c`, built by `test_buffers.sh` in its sanitized half at
each width: in a child process, allocate two blocks of 0x14 bytes from a
bank made in the test and write one byte at offset 0x14 of the first; the
child must die with `use-after-poison` on its standard error. The same
for a block of the room heap written one byte past its end, and for a
music heap block. In a second child, write inside the first block's 0x14
bytes: it must exit 0.

Run: `MIDAS_BUFFER_SANITIZE=1 MIDAS_HARNESS_WIDTHS=64 bash scripts/port/test_buffers.sh` (aarch64 container)
Expected: FAIL, the first child exits 0 (nothing is poisoned today).

- [ ] **Step 2: Implement the hooks and the flag**

A sanitizer report on code that is right (a reader that scans a whole
bank, a block handed back larger than asked) is fixed in the hook, where a
comment says what the reader does; it is never suppressed by name.

- [ ] **Step 3: Prove the detector on the defects already found**

In a scratch copy under `build/lp64/` (a `git archive`, never committed),
revert in turn the sizing fix of each LP64-004 storage defect that a
headless run can reach: the model hit pool (69759f6), the player record
and the inventory (a27e1bd), the gun barrel list (73308dd). Build the
sanitizer executable of each and run `lp64_play.py run --set stages
--only dam` (the `attract` case for the gun barrel list).
Expected: each run fails with a sanitizer report naming the overrun.
Record the four report headers in FORK-089. For a defect the run does
not report, say which it is: the case does not reach the code (find the
case that does), or the hooks have a hole (fix the hook and repeat).
What the hooks cannot see, by construction, is an overrun inside one
record (the pause list and the gait model storage were such): those are
Task 3's `run` kind and the probe's room assertions.

- [ ] **Step 4: Run everything under it, and fix what it reports**

Run: task check steps 4 and 6 and step 8 with every set, on the sanitizer
executable.
For each report: find the storage and its record, write the probe
assertion or ROM-free test that shows the wrong size, fix the size at the
declaration, one commit a defect with its note in FORK-089. A report in a
strict-tier file goes to `lp64/play-strict`.

- [ ] **Step 5: Task check; commit**

Expected: step 3 of the task check exits 0 (the hooks and the flag are
not in a Windows build).

```bash
git commit -m "Poison the game's arenas and drop common symbols in sanitizer builds"
```

---

### Task 3: A reviewed list of every record read through another type

**Files:**
- Create: `scripts/port/lp64_overlays.py`,
  `scripts/port/test_lp64_overlays.py`,
  `scripts/port/lp64_overlays_reviewed.txt`
- Modify: `scripts/port/abi_probe/probe.c`,
  `.github/workflows/windows-release.yml`, `AGENTS.md`, `FORK_CHANGES.md`

**Interfaces:**
- `lp64_overlays.py list [PATH...]`: reads the C sources of `src/` and
  `port/src/` (not `port/fast3d/third_party`, not the `romconv_`,
  `romfile_` and `romdata` loaders, whose inputs are file layouts) and
  prints one line a site, `file:function:kind:text`, for five kinds:
  `index` (an indexed read or write through a cast of an expression to a
  pointer to a scalar type, `((u16 *) x)[n]`); `offset` (a dereference of
  a cast of pointer arithmetic with a constant, `*(T *) ((u8 *) x + K)`);
  `opaque` (a constant index into a member named `padding`, `pad` or
  `unk...` of array type, `x->act_bytes.padding[84]`); `record` (a cast
  to a pointer to a struct or union type declared in `src/bondtypes.h`,
  `src/game/*.h` or `include/PR/libaudio.h`); `run` (from the headers, a
  struct member of type `Gfx`, `Model` or another record that is followed
  by filler members named `field_...`, `pad...` or `unk...`, which code
  may write through the first member's address; seven by the planning
  count). Lines inside `#ifdef TARGET_N64`, `#if !IS_64_BIT` and `#if 0`
  are left out.
- `lp64_overlays.py check`: every site must have a line in
  `lp64_overlays_reviewed.txt`, `file:function:kind:text<TAB>VERDICT<TAB>
  reason`, and every line must match a site; prints `OK: N sites, all
  reviewed` or the unreviewed and the stale lines, exit 1.
- Verdicts: `typed` (the object is of the type it is read as); `same`
  (the layout read is the same at both widths: the reason names the probe
  assertion that pins it); `fixed` is not a verdict, a fixed site leaves
  the list; `overlay-64` (deliberately different at 64 bits, behind
  `IS_64_BIT`, with the assertion's name); `dead` (not compiled or not
  reached on PC, with why).

- [ ] **Step 1: Write the failing test**

`test_lp64_overlays.py` on sources written in the test: the five kinds
are each found once in a sample file; a site inside `#ifdef TARGET_N64` is
not; `check` fails with a site missing from the list, fails with a stale
line, and passes when they match; a change to a site's text makes it a new
site.

Run: `python3 scripts/port/test_lp64_overlays.py`
Expected: FAIL, `No module named 'lp64_overlays'`.

- [ ] **Step 2: Implement the tool; list the tree**

Expected size, from the planning counts: about 95 `index`, 6 `offset`, 50
`opaque`, 380 `record` and 7 `run` sites.

- [ ] **Step 3: Review every site**

For each: read the code, decide the verdict, and for `same` and
`overlay-64` add the `_Static_assert` to `probe.c` that makes the verdict
true at both widths (an `offsetof` equality between the two views, as the
gait run's names have; for a `run`, the room of the member and its
fillers against the most that is written there, as the four player list
rooms have). A site that reads the wrong member at 64 bits is
a defect: it is listed in FORK-089 and fixed in Task 4, or on the strict
branch. The `record` kind is reviewed by rule first (a cast of
`prop->chr`, `prop->obj` and the other members of the prop's own union to
the type its `type` field names is `typed`), then one by one for what the
rule leaves.

- [ ] **Step 4: Task check; commit** (the tool and its test; then the
  list, in one or more commits by directory)

```bash
git commit -m "List and review every record read through another type"
```

---

### Task 4: Read the four measured records through their own types

**Files:**
- Modify: `src/game/chraction.c` (`weaponIsOneHanded`; the three end-frame
  reads), `src/game/unk_092E50.c` (two writes), `src/game/options.c:564`,
  `scripts/port/abi_probe/probe.c`, `scripts/port/lp64_demo_traces.txt`,
  `scripts/port/lp64_overlays_reviewed.txt`, `FORK_CHANGES.md`

**Interfaces:**
- `weaponIsOneHanded` reads `((WeaponObjRecord *) prop->voidp)->weaponnum`.
- The end frame is `((ModelAnimation *) header)->unk04`.
- The mip-map lists' LOD byte is written through `[4].words.w0`.
- `options.c:564` reads the `struct fontchar` member the 32-bit word
  index 0x224 names (`chars[91].height`, by the planning read; confirm
  with the probe before editing).

- [ ] **Step 1: Write the failing checks**

`probe.c`: `offsetof(WeaponObjRecord, weaponnum) == 0x80` at 32 bits and
`_Static_assert`s that the animation header's `unk04` is the member at
byte 4 of the 32-bit layout. `lp64_demo_traces.txt`: change dam1, runway1
and runway2 to `identical`, and the other eleven to `differs-from T` with
the ticks measured above (227, 406, 35, 114, 113, 325, 186, 54, 125, 158,
70) and the reason `not yet diagnosed (Task 5)`.

Run: task check step 8's `traces` line.
Expected: FAIL: all fourteen still differ at tick 2.

- [ ] **Step 2: Make the four edits**

- [ ] **Step 3: Run**

Run: task check step 8 with `--set demos`, then `traces`.
Expected: `OK: 14 demos, 3 identical, 11 differing as recorded`. A demo
whose first difference is at another tick than recorded is not a failure
of this task: record the tick the run gives, with the run's commit.
Run: task check step 3.
Expected: identical, or `weaponIsOneHanded` and the three callers named
and read: the typed loads must be the loads the casts made.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Read weapon, animation, mip-map and font records through their own types"
```

---

### Task 5: Diagnose each demo that still parts from the 32-bit build

**Files:**
- Modify: whatever each cause names (a strict-tier file on
  `lp64/play-strict`); `scripts/port/lp64_demo_traces.txt`;
  `scripts/port/abi_probe/probe.c`; `FORK_CHANGES.md`

**Interfaces:**
- Consumes: `lp64_play.py traces` and `callers` (Task 1); the reviewed
  list (Task 3), which may already name the site.
- Produces: `lp64_demo_traces.txt` with every demo `identical`, or
  `differs-from T arithmetic: FUNCTION file:line` (for LP64-006), or
  `differs-from T strict: REASON` until the branch lands.

- [ ] **Step 1: For each of the eleven, earliest tick first**

Run both builds with `GE_RNGTRACE=T` for the first differing tick T and
`lp64_play.py callers`; the first caller that one build has and the other
lacks names the branch that went differently (in facility2 at tick 34 it
is `gunfire.c:4419`). Read back from that branch to the value that
differs: print it in both builds from a scratch copy if reading does not
settle it. Classify the cause:
- *a 64-bit defect* (a positional read, a size, a sign, a pointer
  compared as an integer): write the check that shows it (a probe
  assertion, a ROM-free test), fix it, and lower the demo's line;
- *arithmetic* (the two builds compute a float differently: a C library
  function, an x87 or SSE difference in the i686 build): show the two
  values and the operation, change nothing, and record it for LP64-006;
- *not repeatable*: the reference disagrees with itself there
  (`--reference-check`); mark the demo `unstable` with the tick.

One fix often moves several demos: rerun all fourteen after each.

- [ ] **Step 2: Stop rule**

A difference that two working sessions have not classified goes to the
owner with its caller trace and what was ruled out, and the task goes on
with the next demo.

- [ ] **Step 3: Task check; commit** (one commit a cause)

```bash
git commit -m "Record what each demo's first difference from the 32-bit build is"
```

---

### Task 6: The demos, as the milestone states them

**Files:**
- Modify: `scripts/port/lp64_play.py` (the `demos` set's x86_64 note),
  `FORK_CHANGES.md`

**Interfaces:**
- Consumes: Tasks 1, 2, 4 and 5.

- [ ] **Step 1: Run the `demos` and `attract` sets where the milestone
  says**

Plain 64-bit headless in the aarch64 and in the x86_64 container, and the
sanitizer executable (with Task 2's hooks) in aarch64.
Expected: `OK: 14 of 14 cases` and `OK: 4 of 4 cases` in each; `traces`
between the aarch64 and the x86_64 folders prints fourteen `identical`.

- [ ] **Step 2: Record**

In FORK-089: the three runs, each demo's end tick, the trace file's
lines, and the time the sets take.

```bash
git commit -m "Replay every attract demo on a 64-bit headless build"
```

---

### Task 7: Every stage the ROM can serve, and a named stop for the rest

**Files:**
- Modify: `src/game/indy_commands.c` (`indyrescmdCheckFileExists`),
  `scripts/port/lp64_play.py` (only if a case list changes),
  `scripts/port/lp64_census.py` (reads the play folders),
  `scripts/port/test_lp64_census.py`, `FORK_CHANGES.md`,
  `scripts/port/native_64bit_plan.md` (what "every stage" means, after
  decision 4)

**Interfaces:**
- `lp64_census.py types DIR` accepts a `lp64_play.py` output folder as
  well as a smoke folder.

- [ ] **Step 1: The inherited stack overrun**

Write a ROM-free test in `scripts/port/test_buffers.sh`'s set that calls
`indyrescmdCheckFileExists` with a 12-character name under the sanitizer.
Expected before the fix: `stack-buffer-overflow`. Bound the copy to the
field it fills. The Windows comparison names that one function
(decision 13).

- [ ] **Step 2: Run the `stages`, `stops` and `tours` sets**

Plain in both containers, sanitizer in aarch64.
Expected: 54 of 54, 12 of 12 and 26 of 26. A stage that fails is fixed as
Task 2's Step 4 says. Then run `tours` on the i686 reference (`--jobs
4`) and compare each stage's `done, V of R rooms` line: a tour that ends
earlier at 64 bits than at 32 is a defect to find; where both end at the
same room (a mission that fails when Bond is put there), the count is
the tool's floor for that stage and FORK-089 records it.

- [ ] **Step 3: Census**

Run: `lp64_census.py types build/lp64/play/plain`.
Record which of model opcodes 0, 3, 5, 6, 7, 11, 14, 15, 16, 17, 19 and
20 and prop record types 0, 15, 16, 31 and 41 a run of this milestone has
now loaded. The rest stay verified by fixtures only and go to LP64-006's
hand-over by number.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Load every stage the ROM serves on a 64-bit build and stop by name on the rest"
```

---

### Task 8: Buttons for a headless build

**Files:**
- Create: `port/src/port_playlog.c`, `port/include/port_playlog.h`,
  `scripts/port/test_inputrec_headless.sh` and its `.c`
- Modify: `port/src/libultra_shim.c` (`osContGetReadData`),
  `port/src/inputrec.c`, `src/game/front.c` (`frontChangeMenu`, one call;
  the `GE_MENU` harness gains `briefing`), `src/game/lv.c` (stage load and
  unload, one call each), `scripts/port/lp64_play.py` (the `input` set and
  its routes), `scripts/port/test_lp64_play.py`,
  `scripts/port/abi_probe/probe.c` (the briefing span), `FORK_CHANGES.md`

**Interfaces:**
- In a build without `GE_VIDEO`, `osContGetReadData` calls
  `portInputRecPad(&data[0])` after zeroing the pads, and the replay's
  tick is the count of `osContGetReadData` calls. With `GE_INPUTPLAY`
  unset nothing changes. Only `P tick buttons stick_x stick_y` lines act
  in a headless build; the mouse, action and drift lines are ignored
  there.
- `void portPlayLog(const char *event, s32 value);` prints
  `ge: play: EVENT VALUE` when `GE_PLAYLOG` is set and is otherwise one
  test of a cached flag. Events: `menu` (the menu id, from
  `frontChangeMenu`), `stage-load` and `stage-unload` (the level id),
  `pause` (1 on, 0 off; from the non-strict caller that sets the pause
  state, or from `GE_PAUSEDBG`'s existing line if the only site is in
  `bondview2.c`).
- `GE_MENU=briefing[,after]`: the harness sets folder 1, Agent and Dam's
  briefing and changes to the briefing menu.
- The `input` set, each case a route the tool writes as an `GE_INPUTPLAY`
  file in the run folder, with `GE_PLAYLOG=1`:

  | Case | Route | Required |
  |---|---|---|
  | `pause-MISSION` for dam, facility, train | the mission; START at tick 600; START again at 1200 | `ge: play: pause 1` then `pause 0` |
  | `watch-TAB` for each watch tab | Dam; START at 600; `GE_WATCHPAGE=TAB` | `pause 1`; 1,800 frames |
  | `walk-fire` for dam, facility, runway | stick forward 300 ticks, Z held 60, repeated | the frame-limit line |
  | `front-end` | from boot: START or A at the ticks that pass the legal screen, file select, mode, mission, difficulty and briefing, then start | `ge: play: menu` for each of those menus in order, then `stage-load 33` |
  | `abort` | Dam; START; the watch's abort; confirm | `stage-unload 33`, then a `menu` line |
  | `briefing` | `GE_MENU=briefing` | `ge: GE_MENU harness: menu=`; 900 frames |
  | `respawn-ARENA` for each of the 13 arenas | 2 players; `GE_KILLBOND=600`; START at tick 900 and every 60 ticks until 1500 | `ge: mp spawn p0 pad=` (the respawn's own line, `bondview2.c`) |

  The ticks of `front-end` and `abort` are found by running, not guessed:
  the route's builder is a list of (wait for this `ge: play:` line, then
  press) steps that the tool turns into ticks from a first recording run.

- [ ] **Step 1: Write the failing tests**

`test_inputrec_headless`: a ROM-free harness that links
`libultra_shim.c`'s pad read with `inputrec.c` in a build without
`GE_VIDEO`, sets `GE_INPUTPLAY` to a file written in the test (`P 3 1000
0 0`), calls `osContGetReadData` five times and asserts the third call's
pad 0 has START and the others have nothing; with the variable unset all
five are zero.
Run: `bash scripts/port/test_inputrec_headless.sh`
Expected: FAIL, the third pad is zero.
`test_lp64_play.py`: an `input` case fails when its required `ge: play:`
line is missing though the run exits 0.

- [ ] **Step 2: Implement the hook, the play log and the harness value**

- [ ] **Step 3: Write the routes and run the `input` set**

Plain and sanitizer, aarch64.
Expected: every case passes. These are the first 64-bit runs of the pause
list, the watch background lists and the briefing's bank span, under
Task 2's hooks: a report here is fixed as Task 2's Step 4 says.

- [ ] **Step 4: Prove the two fixes the route is for**

In a scratch copy, revert the watch background lists' size (73308dd) and
the briefing span (a657b4b) in turn; the `watch-` and `briefing` cases
must fail with a sanitizer report. Record the reports. The pause list
lies inside the player record, where no hook can see an overrun: its
check stays the probe's room assertion, and the `pause-` cases are its
first execution. Add to `probe.c` the assertion that the briefing's bank
starts inside its 0x78000-byte block at both widths (the fix that rests
on reading only).

- [ ] **Step 5: Task check; commit**

Expected: step 2 of the task check still passes 20 of 20 (the i686
reference reads no input file), step 3 identical.

```bash
git commit -m "Replay recorded pads in a headless build and run the paths that need a button"
```

---

### Task 9: A 32-bit reference with video and sound in a container

**Files:**
- Modify: `scripts/port/lp64_env.py` (an image choice),
  `scripts/port/test_lp64_env.py`, `scripts/port/lp64_play.py`
  (`--video`), `scripts/port/test_lp64_play.py`, `AGENTS.md`,
  `FORK_CHANGES.md`

**Interfaces:**
- `lp64_play.py run --video`: adds `SDL_VIDEODRIVER=offscreen`,
  `SDL_AUDIODRIVER=dummy`, `GE_WINDOW=640x480`, `GE_NOINPUT=1`, a `HOME`
  and `GE_ICON_CACHE_DIR` inside the run folder; requires in every case
  `ge: video: window + GL context up` and `gfxtasks=N` of the frame-limit
  line at least the case's `pictures` (default 30); `--shots N` adds
  `GE_SHOT=N` and copies the last `shot_*.bmp` of each case to
  `DIR/CASE.bmp`.
- `lp64_env.py run --arch i686 --video -- COMMAND`: runs in an image that
  can build and run a 32-bit `GE_VIDEO` executable. The builder chooses
  between giving the `goldeneye` image what `linux_build.py` uses for the
  i386 SDL and building `scripts/port/linux-release.Dockerfile`'s image;
  either way `build-images` makes it and FORK-089 records the choice and
  the build time.

- [ ] **Step 1: Write the failing tests** (`test_lp64_env.py`: the
  argument list for `--arch i686 --video`; `test_lp64_play.py`: a video
  case fails with `gfxtasks=3` when 30 are required, and fails without the
  GL line).

- [ ] **Step 2: Implement; build the 32-bit video reference**

Run: `lp64_env.py run --arch i686 --video -- sh -c 'cmake ... -DGE_VIDEO=ON && cmake --build ...'`, then `lp64_play.py run --video --set demos --only runway2`.
Expected: `runway2: PASS`. Record the pictures a second the emulated
32-bit build draws; if it is too slow to finish a demo in ten minutes, the
32-bit video reference is used for the sound capture only (Task 10) and
FORK-089 says so.

- [ ] **Step 3: Task check; commit**

```bash
git commit -m "Run video builds offscreen in the containers, 32-bit reference included"
```

---

### Task 10: Sound that runs, and is not silence

**Files:**
- Create: `scripts/port/test_audio_synth.sh`,
  `scripts/port/test_audio_synth.c`
- Modify: `src/libultra/audio/synthInternals.h`,
  `src/libultra/audio/synthesizer.c` (the pool's record),
  `scripts/port/abi_probe/probe.c`, whatever the silence's cause names
  (`src/libultra/audio/load.c`, `port/src/romconv_bank*.c`,
  `port/src/audio_pc.c`), `scripts/port/lp64_play.py` (the `sound` set),
  `scripts/port/test_lp64_play.py`,
  `.github/workflows/windows-release.yml`, `FORK_CHANGES.md`

**Interfaces:**
- `synthInternals.h`: `typedef union { ALParam param; ALStartParam start;
  ALStartParamAlt startAlt; ALFreeParam free; } ALParamRoom;` and the pool
  is allocated and stepped by `sizeof(ALParamRoom)`. `__allocParam` still
  returns `ALParam *`.
- `probe.c`: `sizeof(ALParamRoom) >= sizeof(ALStartParamAlt)` at both
  widths and `sizeof(ALParamRoom) == 28` at 32 bits;
  `sizeof(ALSndpEvent) <= sizeof(ALEvent)` and the offset of the event's
  pointer member equal in both.
- The `sound` set (video executables only): `capture-title` (no launch
  variable, 1,800 frames), `capture-dam` (`GE_STAGE=dam`, `GE_SKIPINTRO=1`,
  1,800 frames), each with `GE_AUDIO_CAPTURE` to a file in the run folder.
  Required: no `audio frame skipped` line; the recording is 22,050 Hz
  stereo, at least 10 seconds long, with a peak of at least 1,000 (the
  planning run's recording had a peak of 0). `lp64_play.py sound DIR`
  prints each recording's length, peak and share of non-zero samples;
  Step 4 compares those with the 32-bit reference's instead of fixing a
  floor for the share.

- [ ] **Step 1: Write the failing tests**

`probe.c`'s assertions (they fail to compile at 64 bits today: 32 against
40). `test_audio_synth.c`, built at each width with the synthesizer's
sources and the interpreter (`audio_pc.c` with
`PORT_AUDIO_INTERPRETER_ONLY`): build in the test a bank with one raw
16-bit wave of 2,000 samples of a square wave, one sound, one instrument;
start one voice through `alSynAllocVoice`, `alSynStartVoiceParams` and
`alSynSetVol`; call `alAudioFrame` and `portAcmdRun` for ten frames;
assert that the interpreter skipped no frame, that the output has a
non-zero sample, and that after 64 note starts and stops the parameter
free list still has its starting count.

Run: `bash scripts/port/test_audio_synth.sh` (aarch64 container)
Expected: FAIL at 64 bits (the free list is corrupted, or the output is
all zeros); `PASS (32-bit)` in the x86_64 container.

- [ ] **Step 2: Size the parameter pool by what is written into it**

Expected: the probe compiles; the Windows comparison is identical.

- [ ] **Step 3: Find why every sample load is refused**

Run `capture-dam` on the plain 64-bit video executable with the
interpreter's one-time warning made to print the buffer state it refused
(in a scratch copy): the count, the input offset and the command's
address. Trace the count back through `alLoadPull` and
`alAdpcmPull`/`alRaw16Pull` (`load.c`) to the wave table record the
converter built (`romconv_bank_records.c`): the planning run suggests a
length or loop field read at the wrong width. Write the ROM-free
assertion that shows it (in `test_audio_synth.c` or
`test_romconv_bank.c`), fix it at its declaration.

- [ ] **Step 4: Record**

Run: `lp64_play.py run --video --set sound` on the 64-bit plain and
sanitizer video executables, and on Task 9's 32-bit video reference;
then `lp64_play.py sound` on the three folders.
Expected: every case passes; the 64-bit recordings' length, peak and
non-zero share are within a factor of two of the 32-bit ones'. Listen to
nothing and claim nothing more: whether it is the same sound is
LP64-006's comparison. Record the nine figures in FORK-089.

- [ ] **Step 5: Task check; commit**

Add `bash scripts/port/test_audio_synth.sh` to the CI tests step.

```bash
git commit -m "Size the synthesizer's parameter records and make 64-bit sound audible"
```

---

### Task 11: The same with video

**Files:**
- Modify: `port/src/port_hud.c` (`phVector`), whatever the runs show
  (strict-tier files on `lp64/play-strict`: `port/fast3d/`, the arm and
  hand files), `scripts/port/lp64_play.py` (per-case `pictures`),
  `FORK_CHANGES.md`

**Interfaces:**
- Consumes: Task 9's `--video`; Task 10's fixes (without the parameter
  pool's, every video run that makes sound dies).

- [ ] **Step 1: `phVector` writes whole words**

A ROM-free test at both widths: after `phVector`, each of the three
commands' two words has a zero upper half at 64 bits. Expected before:
the upper halves hold what the buffer held.

- [ ] **Step 2: Plain video, every set**

Run: `lp64_play.py run --video --shots 120` with `demos`, `stages`,
`stops`, `menus`, `attract` and `input` on the plain 64-bit video
executable (aarch64); `demos` again in the x86_64 container.
Expected: every case passes with at least 30 pictures. Fix what stops a
case. Look at every `CASE.bmp` once and record, by case, anything that is
plainly wrong (missing hands, a black screen, garbage polygons); pictures
stay under `build/lp64` and are deleted before the push. This is a look
for gross faults, not the picture comparison, which is LP64-006's.

- [ ] **Step 3: Sanitizer video with the display-list check**

Run: the same sets on the `GE_VIDEO=ON GE_SANITIZE=ON` executable,
`--jobs 2`, each case's frames tripled.
Expected: every case passes with at least 30 pictures and no `ge: FATAL:
Display-list command word with a non-zero upper half` line. Record the
pictures each case drew.

- [ ] **Step 4: The hands**

Add to the `input` set a `weapons-dam` case: Dam with `GE_CHEATS=guns`,
the A button every 120 ticks thirty times (the next weapon), Z held for
30 ticks after each. Run it and `walk-fire` on the sanitizer video
executable with `--shots 60`, and the ROM-free wardrobe and ADS
regressions at 64 bits (`test_wardrobe.sh`, `test_ads.sh`).
Expected: no report; the pictures show both hands and each weapon.
These runs are the first 64-bit execution of `port_arm_mesh.c`'s alias
cases.

- [ ] **Step 5: The room cache key**

Read `roc_part_key`, then run `stages --only dam --video --env
GE_ROOMCACHE=2`. If the key can
collide at 64 bits (two parts whose list addresses differ only above bit
43), the fix widens the key on `lp64/play-strict` with a ROM-free test of
two such addresses; otherwise FORK-089 records why it cannot.

- [ ] **Step 6: Task check; commit** (one commit a defect; then)

```bash
git commit -m "Run every demo and stage on a 64-bit build with video"
```

---

### Task 12: The small items of the hand-over

**Files:**
- Modify: `port/src/port_lp64.c` (the guard's message and the table's
  comment), `port/src/port_memp.c`, `src/memp.c` (one call),
  `scripts/port/lp64_boot.py` (`STOP_PATTERN`), `src/game/bg.c` or
  `src/boss.c` (the stage reset), `src/game/sky.c` (the `GE_DUMP_BOND`
  read), `port/include/port_romconv.h` (the opcode rule's text),
  `src/game/initmenus.c`, `src/game/front.c` (the wallet's room, after
  decision 3), `scripts/port/abi_probe/probe.c`,
  `scripts/port/test_lp64_address.c`, `FORK_CHANGES.md`; on
  `lp64/play-strict`: `src/game/model.c` (its `GE_DUMP_BOND` read)

**Interfaces:**
- The guard's stop reads `ge: lp64: stopping: asset class "CLASS" is
  marked unconverted in this build`, and `lp64_boot.py`'s `STOP_PATTERN`
  and `test_lp64_address.sh` change with it.
- At 64 bits the stage reset sets `g_BgPortals` and
  `ptr_bgdata_room_fileposition_list` to `NULL`, so a reader on a stage
  without `load_bg_file` faults at once instead of reading reset memory.
- `void portMempCheckBankStart(u8 bank, const void *start);`
  (`port/src/port_memp.c`), called where a bank's start is set
  (`mempSetBankStarts`, `src/memp.c`): at 64 bits it stops the build by
  name if the start is not 8-aligned (the bank cursor's alignment depends
  on it); nothing at 32 bits.
- The wallet's room (recommendation of decision 3): `#define
  FRONT_WALLET_ROOM` in `src/game/front.h`, 0xA000 at 32 bits and 0x10400
  at 64 (13/8, as `gun.c`'s rooms); used by `load_walletbond`'s size, by
  the two `(4096*10)` offsets of the file-select background in `front.c`
  and added to the 0x78000-byte allocation in `initmenus.c` as
  `FRONT_WALLET_ROOM - 0xA000`.

- [ ] **Step 1: Write the failing checks** (`test_lp64_address.c`: the
  new stop text; a child with a misaligned bank start exits by name;
  `probe.c`: `FRONT_WALLET_ROOM` is 0xA000 at 32 bits and holds a
  38,368-byte image with at least 0x6000 bytes to spare at 64).

- [ ] **Step 2: Make the edits; run the `menus` set**

Expected: `menu-select` passes plain and under the sanitizers with the
wallet's model loaded (`ge: romdata: model PwalletbondZ converted`).

- [ ] **Step 3: Task check; commit**

```bash
git commit -m "Settle the small items LP64-004 handed over"
```

---

### Task 13: Pool peaks

**Files:**
- Modify: `port/include/port_lp64.h`, `port/src/port_lp64.c`,
  `port/src/port_memp.c`, `src/mema.c`, `src/game/dyn.c`,
  `src/libultra/audio/heapalloc.c`, `src/game/image.c`,
  `scripts/port/lp64_boot.py` (`INFORMATIONAL_PREFIXES`),
  `scripts/port/test_lp64_boot.py`, `scripts/port/lp64_play.py` (`peaks`),
  `scripts/port/test_lp64_address.c`, `AGENTS.md`, `FORK_CHANGES.md`

**Interfaces:**
- `void portLp64PoolUse(const char *pool, u32 used, u32 size);` keeps the
  largest `used` seen for each pool; at each stage unload and at exit it
  prints `ge: lp64: peak POOL USED of SIZE bytes` for each pool used since
  the last print. `((void) 0)` at 32 bits. Pools: `stage bank`, `room
  memory` (bytes in use, and a second line `room memory largest request`),
  `graphics buffer`, `vertex pool`, `music heap`, `texture pool`.
- `lp64_boot.py`: `'ge: lp64: peak '` joins `INFORMATIONAL_PREFIXES`.
- `lp64_play.py peaks DIR...`: the largest peak of each pool over the
  logs, with the case that had it and the share of the pool's size.

- [ ] **Step 1: Write the failing test** (`test_lp64_address.c`: three
  calls with 10, 30 and 20 of 100 print one line with 30 at the flush;
  `test_lp64_boot.py`: a `peak` line does not fail a run).

- [ ] **Step 2: Implement; place the calls**

- [ ] **Step 3: Measure**

Run: `lp64_play.py peaks` over the plain headless and the plain video
folders of every set.
Record the table in FORK-089. By decision 10: a pool whose peak is above
80 percent of its size, or that printed a `ge: lp64: pool` line in any
run, is raised at 64 bits to twice its peak in this task, with the figure
beside the constant; every other pool is left for LP64-006's soak.

- [ ] **Step 4: The frame-end check's cost**

Build the 64-bit video executable of this commit and one with
`portDynAuditFrameEnd`'s 64-bit check and `portLp64PoolUse` compiled out
(a scratch copy). Run `bench.py run --smoke` with both in the aarch64
container, offscreen. Record both figures and the runs' range. Software
rendering makes this indicative only; FORK-089 says so and names LP64-007
as the milestone with a 64-bit build on a GPU.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Report each pool's peak on a 64-bit build"
```

---

### Task 14: The late-load audit of a 64-bit build

**Files:**
- Modify: `scripts/port/late_load_audit.py` (only what stops it running
  in a container: `--video-driver offscreen`, a writable `HOME`),
  `scripts/port/test_late_load_audit.py`, `scripts/port/late_loads.md`,
  `FORK_CHANGES.md`

- [ ] **Step 1: Run it at both widths**

Run: `late_load_audit.py --exe` on the 64-bit plain video executable
(aarch64) and on Task 9's 32-bit video reference, for the 20 missions and
6 arenas, without a texture pack.
Expected: both finish. Compare the two reports by kind and name of late
load, not by milliseconds (software rendering and emulation distort the
times).

- [ ] **Step 2: Act on the difference**

A late load the 64-bit build has and the 32-bit build lacks is a
regression of the converters (`AGENTS.md`, "Load before play"): fix it by
doing the work in the stage load, or record it in FORK-089 with its
reason. If Task 9 found the 32-bit video reference too slow, compare the
64-bit report with `--env GE_LOAD_AHEAD=0` of the same executable and
with the kinds FORK-090 records, and say that the 32-bit comparison waits
for real hardware.

```bash
git commit -m "Audit late loads on a 64-bit build"
```

---

### Task 15: Land the strict batch and close

- [ ] **Step 1: Hand the branch to the laptop**

As in the LP64-004 plan's Task 17, with `lp64/play-strict`. The hand-off
names every function of the Windows executable that differs from the last
regeneration (bbcac00), by file and kind of difference. If the branch is
empty, the laptop regenerates once on `main`.

- [ ] **Step 2: Confirm on the Mac**

`hand_gallery.py verify` reports fresh; CI passes on the landed commit;
the whole task check runs once more, step 8 with every set: plain and
sanitizer headless (aarch64), plain headless (x86_64), plain and
sanitizer video (aarch64), `demos` with video (x86_64).

- [ ] **Step 3: Close the record**

Set LP64-005 to Done in the status table with the runs, each set's count
and the peaks. `AGENTS.md`: `lp64_play.py` and its two forbidden flags;
the poisoned sanitizer build; `lp64_overlays.py` beside the other
allowlist checks; the new informational prefix. FORK-089 gets the
hand-over to LP64-006: the demo trace file's remaining lines; the opcodes
and record types still covered by fixtures only; the pool peaks and which
pools were raised; the recordings' figures; what video was run on (a
software renderer, no GPU, no real x86); the items of "What this plan
does not do".

## What this plan does not do

- It does not claim the 64-bit game behaves like the 32-bit one beyond
  the fourteen demo traces. Pictures, sound and input recordings are
  compared in LP64-006.
- It does not run on a GPU or on real x86 hardware. "With video" here is
  SDL's offscreen driver and Mesa's software renderer in a container
  (decision 8).
- It does not set final pool sizes (LP64-006's soak), and raises a pool
  only on the evidence decision 10 names.
- It does not make the nine cut level ids, Citadel, or solo loads of
  Temple and Complex playable (decision 4).
- It does not exercise fix wave C's stop for a missing ROM segment or its
  zeroing of a compressed text bank: both need a damaged ROM image, and no
  test may carry one. They stay verified by reading.
- It does not bound the inherited reads that are unbounded at both widths
  (an animation's stream length and entries offset, a sequence's address
  and lengths, the end of an AI list, a glyph's extent, a wave of an
  unknown type), or refuse a crafted `G_TEXRECT` tail in a model list.
  They are not pointer-width defects; with Task 2's hooks a run on the
  stock ROM would show any of them that actually overruns, and none did
  in the planning runs. They are listed for LP64-007's hardening and for
  upstream.
- It does not touch the 32-bit special-portal walk's dependence on link
  order (`sub_GAME_7F0B37EC`), a 32-bit matter.
- It does not port the test harnesses to macOS (10 of 33 pass there
  today) or change `test_audio_buffers.c`'s Linux `mmap` flags: the macOS
  project's, with the measured list above as its starting point.
- It does not run sound in a headless build. Sound runs wherever
  `audio_pc.c` is compiled, which is the video build, and that runs in the
  containers.
- It does not define `A_PAN`, compile `aPan`, or test it.
- It does not build `clang`, GCC 14 or Windows x64.

## Decisions for the owner

**The owner accepted all fourteen recommendations on 2026-11-17**: item 1
in the session that closed LP64-004, and items 2 to 14 together ("Accept
all 13") in the session that wrote this plan. They are kept here as put,
each with the alternative that was not taken; the plan is written for
the recommendations. Decision 6 defines the milestone's done condition
and decision 4 what "every stage" means; decisions 5, 7 and 8 bring part
of LP64-006's comparison forward, let a headless build replay recorded
pads and accept a software renderer in a container as "with video"; the
design records all of them. The first three were left waiting by
LP64-004.

1. **How "with no `ge: lp64:` line" is read** (LP64-004's decision 3).
   *Settled, not open: the owner accepted the reading below on 2026-11-17
   ("Accept the reading", in the session that closed LP64-004, as that
   session relayed it). It is a rule of this plan under Global
   Constraints.*
   The tools read it as: no pool, refusal, converter or stop line, with a
   short list of informational kinds allowed, and they fail any other
   `ge: lp64:` line (`INFORMATIONAL_PREFIXES`, five kinds today).
   *Recommended:* confirm that reading. It is enforced by default-deny,
   so a new kind cannot slip in, and this plan adds one kind
   deliberately (`ge: lp64: peak`, Task 13). The alternative, the literal
   reading, means renaming the informational lines to another prefix (a
   mechanical change to the port, three tools and their tests) so that a
   passing run prints nothing that starts `ge: lp64:`.
2. **The intro walk in `bondview2.c`** (`mp_respawn_handler`, strict
   tier). It has no arm for a WATCH or CREDITS record and the PC
   `INTRO_TYPE` reads a WATCH's hours as a type, so the walk mis-steps
   after a WATCH at both widths. It runs only on a multiplayer respawn,
   and none of the 13 `Ump_` setups has a WATCH. *Recommended:* leave it.
   It is not a pointer-width defect, the 64-bit build behaves as the
   32-bit one does, and the design's anti-goal is rewriting upstream game
   files beyond what pointer width requires. Task 8 runs the walk in all
   13 arenas, LP64-004's fixture keeps pinning that both widths mis-step
   alike, and FORK-089 records it as inherited, for upstream. The
   alternative: two explicit arms on the strict branch, which changes the
   32-bit function (explained for the laptop) and fixes a case no stock
   setup has.
3. **The wallet Bond's room** (`load_walletbond`, `front.c`). At 64 bits
   the stock model's image leaves 2,592 of 0xA000 bytes, and one that did
   not fit would be refused by name and the file-select menu would show no
   wallet. The hand-over says it cannot grow where it is, because the
   file-select background starts at +0xA000 of the same buffer.
   *Recommended:* at 64 bits move the background's start and enlarge the
   buffer by the same amount, so that the room is 13/8 of its 32-bit size
   (0x10400), as `gun.c`'s three rooms were scaled by your decision 5 of
   LP64-004. It is four sites in two non-strict files, 25,600 bytes of
   the stage bank, and the 32-bit tokens do not change (Task 12). The
   alternative: leave it at 6.3 percent of margin and rely on the
   refusal.
4. **What "every stage" means.** Measured: the 20 missions, Cuba and the
   13 arenas load and run. Twelve stage numbers have no file to load: the
   nine cut level ids (no background on the cart), Citadel (no setup),
   and solo loads of Temple and Complex (they have only multiplayer
   setups). The 64-bit build stops on each by name with status 1; the
   32-bit build crashes on ten of them, hangs on Citadel and runs Temple
   solo with nothing in it. *Recommended:* "every stage" is the 34 that
   have their files, in every multiplayer scenario; the twelve keep
   stopping by name, checked by the `stops` set, and the design says so.
   This also answers LP64-004's decision 7. The alternative, imitating
   the 32-bit build on the twelve (loading zeros), reproduces crashes on
   purpose; Temple solo would be the one case with something to gain, and
   nothing in the game offers it.
5. **Demo traces are compared with the 32-bit build in this milestone.**
   The design gives simulation equivalence to LP64-006. Measured: the
   comparison found, in the first tick of play, a defect that changes what
   every armed guard does and that three reading sweeps and the
   sanitizers missed; with it fixed, three demos match the 32-bit build
   for every tick and eleven first differ later. *Recommended:* bring the
   comparison forward for the fourteen demos, diagnose each difference
   here (Task 5), fix the 64-bit defects, and hand arithmetic differences
   to LP64-006 with their evidence. The alternative, leaving all of it to
   LP64-006, builds video and sound on a simulation known to differ.
6. **What "done" means.** *Recommended:* on `main`: the task check
   passes, steps 1 to 7 as at LP64-004's close; `lp64_play.py` passes
   every set (`demos`, `stages`, `stops`, `menus`, `tours`, `attract`,
   `input`) on the plain 64-bit headless executable in the aarch64 and
   the x86_64 container and on the sanitizer executable with poisoned
   arenas in aarch64; the same sets pass with video on the plain and the
   sanitizer video executable in aarch64, each case with at least 30
   pictures and no display-list fault, and `demos` with video in x86_64;
   the `sound` set passes and the synthesizer test passes at both widths
   in CI; every demo's trace is identical to the i686 build's or its
   first difference is classified in `lp64_demo_traces.txt`;
   `lp64_overlays.py check` passes in CI with every site reviewed; the
   32-bit smoke is 20 of 20 and the Windows executable is identical to
   98a76dd's or every changed function is explained; CI is green and the
   gallery fresh. A demo difference still unclassified after Task 5's
   stop rule is accepted only with your agreement, by name.
7. **A headless build replays recorded pads.** Without it, every path
   that needs a button runs only with video, where the sanitizer build
   draws less than a picture a second. *Recommended:* let
   `osContGetReadData` call the existing replay hook in a build without
   `GE_VIDEO` (Task 8). With `GE_INPUTPLAY` unset nothing changes; no
   shipped build is headless. The alternative, test hooks for each screen
   (as `GE_MENU` is), reaches the screens but presses nothing.
8. **"With video" on the Mac means a software renderer in a container.**
   Measured: the 64-bit video build runs offscreen on Mesa's llvmpipe in
   the images as they are, about nine pictures a second plain and under
   one a second under the sanitizers. *Recommended:* accept that for this
   milestone, judge a video run by the pictures it drew (at least 30 a
   case), and leave a GPU and real x86 hardware to LP64-006 and LP64-007.
   The alternative is to wait for a 64-bit Linux machine with a graphics
   card, which this project does not have.
9. **Overruns are searched by run, and positional reads by a reviewed
   list.** *Recommended:* poison the arenas and drop common symbols in
   sanitizer builds only (Task 2), and keep a list of every site that
   reads a record through another type, each with a verdict, checked in
   CI so that a new one must be reviewed (Task 3, about 530 sites). No
   shipped build changes. The alternative for overruns, guard bands that
   plain builds check, changes the layout of the builds players get; the
   alternative for positional reads, another reading sweep without a
   list, is what has missed four times.
10. **Pools: measure first, raise only on evidence.** Your LP64-003
    decision left pool sizes to this milestone and LP64-006. Measured: no
    pool line in 848 room visits, 33 multiplayer starts and 14 demos,
    headless; nothing is known about peaks. *Recommended:* add the peak
    lines (Task 13), and in this milestone raise at 64 bits only a pool
    that printed an exhaustion line or peaked above 80 percent, to twice
    its peak; LP64-006's soak sets the final sizes. Room memory and the
    mema heap stay unscaled unless they meet that test. The alternative,
    doubling room memory now because its display lists double, spends
    stage bank on a guess.
11. **What counts as sound running.** *Recommended:* the parameter pool
    fixed and pinned; a ROM-free synthesizer test in CI at both widths;
    recordings of the title and of Dam that are not silent and drop no
    frame, with length, peak and non-zero share near a 32-bit recording
    of the same run (Task 10). Whether it is the same sound is LP64-006's
    verification item 7. The alternative, moving all sound to LP64-006,
    leaves a crash at the first note of music in every video run of this
    milestone.
12. **One laptop trip, at the end.** Strict-tier fixes go to
    `lp64/play-strict`; a demo whose fix waits there is marked so in the
    trace file. *Recommended:* one trip (Task 15), as before. Say if you
    would rather make a first trip when Task 5 or Task 11 puts its first
    commit on the branch: `gunfire.c` and the renderer are the likely
    places, and `main`'s video runs would otherwise carry known defects
    until the end.
13. **Defects the 32-bit game has too.** The first is the stack overrun
    in `indyrescmdCheckFileExists`. *Recommended:* LP64-004's rule
    carried on: fix one here when the fix is a few lines, naming the
    changed 32-bit function in the commit; otherwise record it with your
    agreement. Task 2's hooks are likely to show more of them.
14. **The tests on macOS are left to the macOS project.** Measured: 10
    of 33 harnesses pass there with clang. *Recommended:* change nothing
    here; the list of what stops each is in this plan and in FORK-089.
    The alternative, fixing the common cause now (thirteen stop at one
    line of `include/PR/os.h`), is cheap but belongs to a project with
    its own plan and compiler.
