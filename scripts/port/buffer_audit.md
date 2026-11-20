# Buffer and memory-safety audit

Audit baseline: `fb9695f` (2026-10-30). Scope includes engine/game code,
platform adapters, renderer, audio interpreter, ROM/asset conversion, save and
texture codecs, browser bridges/application, and build/audit tooling. Generated
asset data and vendored code are inventoried separately from maintained runtime
code. This is a bounded source/tool/runtime audit, not a proof that arbitrary
campaign paths or malformed ROMs are safe.

The reproducible lexical inventory is `python scripts/port/audit_buffers.py`.
Its output goes to ignored `build_win/buffer-audit/inventory.json`. Candidate
counts include comments, tests and bounded uses; they are not bug counts.

## Scope and method

The initial tracked inventory covered 2,400 source/script files and about
1.075 million lines, including generated data and tooling. Maintained runtime
code, inherited engine/audio code, generated assets, vendored runtime code,
browser application/bridges and tooling are separate inventory domains. This
was a repository-wide search and boundary review, not a line-by-line proof of
every file. The inventory includes hashes and locations so it can be repeated.
The final inventory also includes C# launcher/tool sources, C++ headers,
standalone shaders and HTML entrypoints. GPU sampler/CRT shaders were reviewed
for CPU-boundary assumptions; C# launcher review found managed parsing rather
than unsafe native-buffer operations. Neither was exhaustively fuzzed.
Including those formats and the new guard/test sources, the final inventory
contains 2,437 files and about 1.080 million lines.

The review followed `AGENTS.md`, `AGENTS.bright-builds.md`,
`standards-overrides.md`, and the architecture, code-shape, verification and
testing standards. Focused platform helpers preserve the N64 matching paths;
the arm contracts prohibit unrelated model/material changes. Rendering changes
use the required isolated build and reference-gallery workflow.

- Cppcheck used the actual CMake compilation databases for Linux video
  (`unix32`, 287 translation units) and Windows video (`win32A`, 288 units),
  with warning/portability checks and inconclusive diagnostics enabled.
- Manual review prioritized allocation arithmetic, fixed-capacity producers,
  renderer indices, model/texture conversion, audio commands, snapshots,
  external save/texture files, and browser/native boundaries.
- Small failing inputs exercised the real implementations under 32-bit ASan
  and UBSan, followed by regression tests and mission startup samples.
- Linux headless checks exercise engine simulation and display-list generation;
  actual graphics/audio checks use the isolated Windows video executable.

The analyzer runs produced 379 Linux and 205 Windows diagnostics during the
audit. These are overlapping candidates, not 584 distinct bugs. Many describe
decompiler placeholders, aliasing idioms, native pointer casts or unsupported
macros. Linux parsing failed in `chraidata.c`; Windows parsing/configuration
failed in `chraidata.c`, `chrai.c`, `blood_animation.c` and `explosion.c`.
Those files were compiled normally, but cannot be described as fully covered by
Cppcheck. Raw XML and progress logs remain local under `build_win/`.

## Fixed findings

| Area                   | Evidence and previous failure                                                                                                                                                      | Current behavior                                                                                                                                                                                                                                |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Memory banks           | Pointer-plus-size could wrap before the end check; negative resize could move the cursor backwards outside its pool. Bank indices were unchecked.                                  | Native allocation/resize validates bank, cursor and remaining bytes using subtraction, retains permanent-pool fallback, and fails with a diagnostic on exhaustion.                                                                              |
| High asset mappings    | Adding 15 or 65,535 could wrap a very large request to a small allocation.                                                                                                         | Both alignment operations reject overflow before mapping.                                                                                                                                                                                       |
| Model conversion       | ASan reproduced a heap over-read when a complete node pointed to one remaining byte of display-list metadata. Other record variants read fields before checking their span.        | Complete node/metadata records and header counts are validated before reading; range arithmetic and quad-count multiplication cannot wrap.                                                                                                      |
| Audio DMEM             | ASan reproduced a global-buffer over-read from a 128-byte DMEM move at offset 4,090.                                                                                               | Commands are preflighted against the actual source/destination spans, including rounded transfers, ADPCM output, mix buses and interleave output.                                                                                               |
| Audio diagnostics      | ASan reproduced a stack over-read while logging a one-command invalid frame: the logger read seven commands regardless of list length.                                             | Diagnostic windows stop at the actual count and initialize unused entries. Null/negative/oversized lists are rejected.                                                                                                                          |
| Audio decoder state    | Predictor nibbles could select beyond the 128-short ADPCM book; resampler history/advance could reach outside DMEM, and initialization left part of its temporary state undefined. | Predictor bounds, derived resampler/history spans and full temporary initialization are checked. Host RAM pointers remain subject to the limits below.                                                                                          |
| Shader text            | Helpers discarded destination capacity; `vsnprintf`'s requested length could exceed the copied scratch bytes and make a later append offset invalid.                               | Array-reference helpers retain destination capacity and reject truncation/invalid offsets before subsequent appends. This closes a defensive gap; no ordinary shader overflow was reproduced.                                                   |
| Renderer arrays/lights | Ambient light payloads contain eight bytes but the renderer copied a full 16-byte directional light. Vertex destination offsets relied on a count-only assertion.                  | Ambient copies read eight bytes; vertex loads, triangles, matrix stacks and light counts have release-active bounds checks. Valid rendering is unchanged.                                                                                       |
| Debug tokens           | A long token string and more than 34 nonempty tokens exceeded fixed global storage.                                                                                                | Native text copy is bounded and splitting stops at table capacity.                                                                                                                                                                              |
| Engine limits          | A full character pool was dereferenced as NULL; multiplayer ammo type zero indexed `slots[-1]`; visible-room insertion had no non-EU capacity check.                               | Character/visibility exhaustion fails before the invalid access. Ammo without a valid slot is omitted. Room diagnostics use the real room-array capacity. These are source-confirmed edge cases, not failures observed in normal campaign play. |
| Save-state ownership   | Fresh-process Dam restoration from Facility reproduced a native arm-rendering failure. Saved arena bytes and live model-header caches could belong to different worlds.            | Arm/grip cache owners and selected arena-backed texture/crosshair pointers travel with the world image. Heap copies, files, settings and GPU resources stay process-owned. Fresh-process threaded restoration now passes.                       |
| Menu return value      | A native non-void Files controller path fell through without returning.                                                                                                            | It returns zero; its caller does not use the value.                                                                                                                                                                                             |
| Explosion sprites      | With the graphics pool poisoned (LP64-005 Task 2), every demo, Jungle and the attract loop died with `use-after-poison` in `explosionRenderPart`: it allocated four vertices and wrote a fifth, at both widths (inherited from the N64 code). | The fifth write is kept only in the N64 build; the four vertices drawn are unchanged. |

The previously fixed auxiliary frame-data overflow remains documented in
FORK-014/FORK-017. This audit preserves its 256 KiB/player/half capacity floor
and pre-allocation guards. It does not establish that every display-list
producer requests enough bytes.

## Reviewed external boundaries

These paths already had substantial checks; this audit did not find a new
reproducible overflow in them:

- Native campaign backup/import: 4 MiB/256-file limits, bounded JSON nesting,
  canonical base64, checksums, path allowlists, case aliases, symlink rejection
  and staged replacement. Browser backup parsing and storage tests also ran.
- HD texture packs: covered when audited; the loader and its tests were later
  removed with the feature (FORK-033).
- BMP/font/icon loading: dimension/row/file spans, bounded native icon records
  and decoded sizes. These codecs have unit coverage; that is not a complete
  fuzzing campaign against every image format.
- Snapshot codec: descriptor and run bounds, exact-build/ROM identity, payload
  checksum, decode before application, render barrier and process/world ownership.
- Browser ROM loading: stock image size/hash validation before handing bytes to
  Wasm. Runtime downloads enforce manifest sizes/hashes and chunk limits.
  Touch/input bridges bound action indices and bit shifts.

## Remaining risks and follow-up order

| Priority                       | Remaining risk                                                                                                                                                                                                                                                                                                                        | Follow-up needed                                                                                                                                                                                                                                                               |
| ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| High                           | Engine master display lists and individual generated lists still use raw `gdl++`/GBI writers. Auxiliary allocation checks bound reservations, not writes beyond an underestimated reservation. ASan cannot see every boundary inside a large custom arena.                                                                            | Carry an end pointer or capacity through command producers and reject before each packet write; add stress cases with maximum effects, models, split-screen and atypical visibility. A check after generation alone is insufficient.                                           |
| High                           | `alAudioFrame` emits into `audio_pc.c`'s fixed command array before the interpreter receives `cmdLen`. Interpreter rejection cannot undo an emitter overflow. Host RAM/state addresses embedded in commands are not proven mapped just because they pass the existing low-address/alignment filters.                                  | Bound command emission in the audio library/ABI packet writers and track authorized RAM/state spans. Exercise voice/reverb/music saturation with a real audio backend.                                                                                                         |
| High for untrusted/modded data | Inherited texture Huffman/RLE, background, animation and 1172 decompression paths still assume valid ROM structures. Model conversion checks do not make every downstream traversal safe.                                                                                                                                             | Introduce length-bearing readers and bounded decompression outputs throughout the loader graph, with format-specific malformed-input corpora. Native modified ROMs remain outside a hostile-input safety guarantee; the browser's strict stock hash reduces its input surface. |
| Medium                         | Analyzer candidates remain in inherited code, including unsupported texture formats in `image.c`, the unused watch inventory page's uninitialized dimensions in `options.c`, and rare audio/geometry/control-flow paths. Some are dead placeholders or alias-analysis errors; others need targeted fixtures before changing behavior. | Triage by actual reachability and data contracts. Keep XML evidence and build configuration; do not suppress a whole diagnostic class or treat all findings as proven.                                                                                                         |
| Medium                         | Tooling and vendored/generated sources received inventory/search/build coverage, not exhaustive sanitizer execution. Native legacy formatting has many fixed buffers sized around stock language strings and game limits.                                                                                                             | Add boundary fixtures when changing these inputs, especially localization/modding, external files and generated metadata. Review actual callers before replacing every `sprintf` mechanically.                                                                                 |

No conclusion about browser flicker issue #3 follows from this audit. There is
no claim of complete multiplayer, all-mission gameplay, GPU-driver or browser
acceptance. The campaign samples below cover startup and a short controllable
period, not mission completion or every dynamic event.

Starting points for the high-priority work are
[`dyn.c`](../../src/game/dyn.c) and
[`gbi.h`](../../include/PR/gbi.h),
[`synthesizer.c`](../../src/libultra/audio/synthesizer.c) and
[`abi.h`](../../include/PR/abi.h), and
[`image.c`](../../src/game/image.c),
[`decompress.c`](../../src/game/decompress.c) and
[`inflate.c`](../../src/inflate/inflate.c). In particular, legacy `-mgfx`/`-mvtx`
budget parsing in `dynInitMemory` multiplies before calling the guarded bank
allocator. Stock stage budgets were exercised; arbitrary override arithmetic
still needs checked parsing/multiplication and is not covered by the allocator
fix alone.

## Reproduction and validation

Static scans used Cppcheck 2.13.0 after exporting each native compilation
database (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`). For example:

```sh
cppcheck --project=build_pc_video/compile_commands.json --platform=unix32 \
  --enable=warning,portability --inconclusive --inline-suppr \
  --suppress=missingIncludeSystem --xml --xml-version=2 -j8 \
  2>build_win/buffer-audit-cppcheck.xml
```

Use the Windows build database and `--platform=win32A` for that configuration.
Summarize either XML with `audit_buffers.py --cppcheck <report.xml>`; compiler
errors/parser failures must be reviewed alongside diagnostic counts.

ROM-free targeted tests (also wired into Windows release CI):

```sh
MIDAS_BUFFER_SANITIZE=1 bash scripts/port/test_buffers.sh
bash scripts/port/test_states.sh
MIDAS_SAVE_TEST_I686=1 bash scripts/port/test_save_transfer.sh
```

The new suite covers exact fit and one-past boundaries, wrapping sizes,
invalid banks/negative resize, sentinel preservation, short audio lists and
derived audio spans, eight-byte ambient payloads, shader truncation/invalid
offsets, long token input, truncated model metadata and 10,000 deterministic
malformed models. Python tests check domain inventory and mission verdicts,
including isolated child executables and rejection of incomplete runs.

Sanitizer runs disable alignment, shift and signed-integer-overflow checks to
retain this port's inherited packed/unaligned and integer-wrap contracts.
ASan and the other UBSan checks remain enabled with immediate failure. The
headless build uses `GE_SANITIZE=ON` and its existing generated-asset exclusions;
the original 2026-10-30 mission samples disabled leak detection, which the
[leak-detection pass](#leak-detection) below adds. These exclusions and custom
arenas limit what a clean run establishes.

Since LP64-005 Task 2 (FORK-089) a sanitizer build also sees inside the game's
own arenas and its uninitialized globals. `GE_SANITIZE` compiles C with
`-fno-common`, so every instrumented global array gets a red zone (the
animation-table data files and the manifest assets stay uninstrumented, as
`CMakeLists.txt` says) and a global defined twice
fails to link (shipped builds keep `-fcommon`). `port/include/port_poison.h`
poisons what each arena has not handed out: a stage-bank reset poisons the
bank, `portMempAllocate` unpoisons exactly the bytes asked for and leaves a
32-byte poisoned gap before the next block (dropped at the bank's end), and a
resize re-poisons what a shrink gives back; `memaReset` poisons the room heap,
`memaAlloc` and `memaGrow` unpoison, `_memaFree` poisons; `alHeapInit` poisons
the music heap and `alHeapDBAlloc` unpoisons each block in whole 16-byte lines; the graphics pool's
half a frame writes is poisoned at the frame's start (`dynGetMasterDisplayList`,
not at the swap, because the renderer reads the finished half after it) and
`portDynAllocate` unpoisons each block. A touch of a byte no block owns is
reported as `use-after-poison`. Without the sanitizer the macros are empty and
the allocators place every block as before. What this cannot see: an overrun
inside one block that its user carves up itself (the synthesizer's parameter
pool, the gun barrel list inside the front end's 0x78000-byte buffer, a
display list inside the player record); up to 15 bytes past a music-heap
request, which the whole-line rounding leaves unpoisoned; and, where a
room-heap or pool block does not start on an 8-byte boundary, the tail of the
previous block's last 8-byte shadow unit, which ASan cannot poison apart from
the bytes before it. `scripts/port/test_lp64_poison.c`, built by the sanitized
half of `test_buffers.sh`, pins each hook, each probe with a deadline.

Two readers were left open when the hooks went in. `portStateFrame()`
(`src/boss.c`, every frame) in `port/src/port_state.c` reads and writes whole
saved regions when it writes, restores or audits a suspend point, which would
touch poisoned arena bytes and global red zones; that file is compiled only in
32-bit builds (64-bit builds compile `port_state_unavailable.c`, whose
`portStateFrame` only calls `portGraphicsFrame`), so this is a 32-bit
sanitizer build's risk, and it is still open: no 32-bit sanitizer video build
has run. And `portDynAllocate` unpoisons only the bytes asked for, leaving
`dynAllocate`'s rounding to 16 poisoned, so a renderer that read a
graphics-pool block rounded up to 16 would hit that pad. The 64-bit sanitizer
video runs answer this one for `port/fast3d` as the play sets drive it: LP64-005
Tasks 11 and 13 (`lp64_play.py run --video`, demos, stages, stops, menus,
attract, input and sound, the renderer on the game's thread) and the final
review's pass with the render thread forced on (`--env GE_RENDERTHREAD=1`,
demos, stages, menus, attract and input, every log `render thread ON`)
reported no `use-after-poison` or other sanitizer report (FORK-089). No
headless run reads the pool.

The local GCC headless build was configured with `GE_HEADLESS=ON`,
`GE_VIDEO=OFF`, `GE_SANITIZE=ON`, and both C/C++ flags set to
`-fsanitize=address,undefined -fno-sanitize=alignment,signed-integer-overflow,shift -fno-sanitize-recover=all`.
Backup, HD-texture and snapshot tests also ran with sanitizer compiler wrappers;
the backup/HD suites exercised both 64-bit and shipping i686 ABIs.

The headless matrix uses a separately built executable and the user's ROM:

```sh
python3 scripts/port/audit_buffers.py --mission-smoke build/buffer-asan/IronSights \
  --rom baserom.u.z64 --output build_win/buffer-audit/final-missions/results.json
```

Each child owns its copied executable, runtime/settings and logs. A pass needs
the explicit stage launch, the requested 900-pump frame limit, exit zero and no
ASan/UBSan report. All 20 solo mission startup samples passed on the final
sanitized headless build. Logs are local under `build_win/buffer-audit/`.

A 64-bit executable's log is also checked for `ge: lp64:` lines (LP64-004). A
mission fails, whatever its exit status, on a pool line (`ge: lp64: pool`), a
converter's refusal (`ge: lp64: cannot convert`), or any other `ge: lp64:` line
whose kind is not informational. The informational kinds are the address
check's success (`address space OK`), the two audio banks' sizes (`audio
bank`), the music sequence table's (`music sequence table`), a background's
tables (`bg tables`) and the 32-bit oracle build's `oracle:` lines, which
`lp64_oracle.py` judges by their totals. The list and the rule are
`lp64_boot.py`'s (`INFORMATIONAL_PREFIXES`, `failing_lp64_lines`), which the
boot check applies too (the asset-class stop line aside, which it judges
itself), so a new kind of line fails both tools until it is added to the list.

The smoke runs on the game clock: a slow frame (a sanitized build, an emulated
run, a long load) advances several game fields, so a run covers at least its
frames' worth of game time. `--fixed-tick` instead runs one game field a frame
(`GE_FIXEDTICK=1`) and prints the 64-bit converters' census lines
(`GE_LP64_CENSUS=1`); it is for `scripts/port/lp64_census.py`, which compares a
32-bit and a 64-bit smoke that must play the same fields, and for the oracle's
missions (`lp64_oracle.py --fixed-tick --frames N`). A flagged run covers only
its frames' game time, so it needs more frames than an unflagged one: the
LP64-004 census runs use 1800 (FORK-089, "LP64-004 task 16"). The ASan and LSan
commands here keep the game clock.

Windows/Linux video and optimized Wasm builds passed. All 176 browser unit
tests passed. The final native executable passed synchronous full suspend
checks, threaded fresh-process Dam/Facility restore, and both same-session
and fresh-Facility-process Aztec effect sequences (24 captures each, texture
source-byte checks intact). A restored fire/smoke capture was inspected. The
72-view/35-icon gallery passed repeated samples and an independent second pass
with zero changed image cells. FORK-036 records the affected sources and checks.
Useful local logs use the `build_win/buffer-audit-` prefix. The original ASan
reproductions are `repro-model.log`, `repro-audio-copy.log` and
`repro-audio-log.log` under that prefix. ROM-derived captures and binaries are
not part of this report's versioned artifacts.

## Leak detection

Added 2026-11-07 on top of `91640bc`. The earlier samples ran with
`detect_leaks=0`; this pass runs the same sanitizer builds with
LeakSanitizer (LSan) on and triages every report.

### Method

LSan runs once, when the process exits (the frame limit's `exit(0)`, after
`atexit` handlers and static destructors). It reports heap blocks that no
pointer reaches from globals, thread stacks, TLS or other reachable blocks.
So it finds orphaned allocations: a dropped pointer, an error path that skips
`free`, a container destroyed without its owned pointers. It does not find a
reachable cache that only grows (that needs multi-stage memory sampling), GL or
driver objects (they are not port heap blocks), or memp/`portAllocHigh` arena
use: both arenas are anonymous `mmap` regions that LSan neither reports nor
scans, so a heap pointer stored only in an arena would show as a leak. The
game never tears down SDL or the GL context, so driver state stays reachable
and produced no reports. On i686 the scan is conservative: a stale word that
looks like a heap address keeps a block alive, so a clean run is evidence,
not proof.

- **Headless matrix:** `build/lsan-headless`, configured exactly like the
  original headless build (`GE_HEADLESS=ON GE_VIDEO=OFF GE_SANITIZE=ON
  GE_ROMDATA_FALLBACK=OFF`, the same C/C++ sanitizer flags) in `goldeneye:latest`.
  `audit_buffers.py --leaks` sets `ASAN_OPTIONS=detect_leaks=1` and
  `LSAN_OPTIONS=suppressions=scripts/port/lsan_suppressions.txt:print_suppressions=1`,
  allows 300 s per child for the exit-time scan, and fails a mission on
  `ERROR: LeakSanitizer` as well as ASan/UBSan output.
- **Renderer run:** `build/lsan-video`, `GE_VIDEO=ON GE_SANITIZE=ON` (Debug,
  same flags) in `midas-linux-release` with `SDL2_LINUX_ROOT` set to an i686
  SDL 2.30.11 prefix as `linux_build.py` builds it (`build_linux*/deps/sdl`).
  This configuration keeps `headless_stubs.c` out, as the CMake comment
  requires, and links. `scripts/port/leak_audit_video.sh` installs Xvfb and
  i386 Mesa, gives each case its own folder, `midas.ini` (640x480 window,
  `msaa=1`, point filtering, no mipmaps, `render_thread` from `RT`) and icon
  cache, and runs Mesa's `softpipe` (`GALLIUM_DRIVER=softpipe`). llvmpipe
  works too, but its shader JIT under ASan stalled single frames for up to
  325 s; softpipe ran a 150-frame Dam case in 112 s instead of 615 s.

### Rerun

The ROM stays in the main checkout and is mounted read-only; it is never
copied into the worktree. On Git Bash prefix `docker` with `MSYS_NO_PATHCONV=1`.

```sh
# Headless: configure/build build/lsan-headless as above, then
docker run --rm -v "<worktree>:/src" -v "<rom>:/rom/baserom.u.z64:ro" -w /src goldeneye:latest \
  python3 scripts/port/audit_buffers.py --mission-smoke build/lsan-headless/IronSights \
  --rom /rom/baserom.u.z64 --leaks --jobs 8 --output build/lsan-logs/matrix/results.json

# Renderer: build build/lsan-video (copy libSDL2-2.0.so.0 into build/lsan-video/lib), then
docker run --rm -v "<worktree>:/src" -v "<rom>:/rom/baserom.u.z64:ro" -w /src midas-linux-release \
  bash scripts/port/leak_audit_video.sh build/lsan-video/IronSights build/lsan-logs/video \
  'title|1200|GE_NOINISAVE=1' 'dam|300|GE_STAGE=dam GE_STAGE_AFTER=1 GE_NOINISAVE=1' \
  'dam-rt|500|GE_STAGE=dam GE_STAGE_AFTER=1 RT=1 GE_NOINISAVE=1'
```

A case is `name|frames|VAR=value ...`; it passes on exit zero, the requested
frame limit and no sanitizer report. Leave `GE_NOINISAVE` out to exercise the
settings writes at exit (each case writes only its own folder).

### Suppressions

[`lsan_suppressions.txt`](lsan_suppressions.txt) currently has no active entry,
because no run printed a report. It lists the deliberate never-freed
singletons (weapon icons, HD texture workers, the mask-wrap buffer, the decal
vertex copy, replacement-texture blobs) with their reasons: each stays
reachable through a static pointer, so LSan does not report it. A future entry
must name a report a run actually printed: a deliberate process allocation by
its allocating function, or third-party internals by library (for example
`leak:libGLX_mesa.so`). A pattern matches any frame of the allocation stack,
so it must not also cover port code calling into that function or library.

### Findings

| Finding | Classification |
| ------- | -------------- |
| The headless sanitizer build no longer linked: `port_hitmap_probe.c` (FORK-048) sets `ge_shot_force`, which only `gfx_sdl2.cpp` defines. | Fixed (build): `headless_stubs.c` defines the trigger; nothing reads it without a renderer. |
| No LSan, ASan or UBSan report in any run below. | No leak to fix or suppress. |

Runs, all exit zero with no report, logs local under the worktree's ignored
`build/lsan-logs/`:

- Headless: all 20 solo missions (900 frames each, `--leaks`); the title for
  14,000 frames, including the Runway and Dam attract demos and their stage
  loads and unloads; Temple four-player and Complex two-player multiplayer
  (`GE_MP`, `GE_DRIVE`); title and Dam with `GE_NOINISAVE` unset; Facility
  with a console command; the hit-map probe through mission abort and debrief.
- Renderer (softpipe unless noted): title for 300 and Dam for 300 frames
  (llvmpipe, the game's default 1280x800 settings with the render thread on);
  title for 900 (llvmpipe) and 2,400 frames; Dam for 150 frames (llvmpipe and
  softpipe); Dam with the render thread on; Dam with
  settings writes at exit; Facility, Aztec, Frigate and Cradle for 300 frames;
  Temple two-player for 400 frames; the hit-map probe through the debrief;
  the in-process weapon-icon stage (`GE_ICON_STAGE=1`), which hit its 60 s
  limit under softpipe and so exercised the failure path, not a cache write.
- ROM-free suites with ASan/UBSan/LSan compiler wrappers first on `PATH`:
  `test_hd_textures`, `test_enhanced_fonts`, `test_open_fonts`,
  `test_save_transfer`, `test_states`, `test_hud`, `test_ads`, `test_wardrobe`,
  `test_buffers`, `test_dyn`, `test_hitmap`, `test_hit_sound`,
  `test_image_bank`, `test_mission_cheats`, `test_mission_hints`,
  `test_mission_times`, `test_stan_callbacks`. `test_start_input` needs SDL
  development headers that the image lacks and did not build.

A source review of the port's own allocation sites (SDL strings and mappings,
`midas.ini` rewrites, suspend-point files, texture replacement buffers,
threads) found them freed, detached or joined on the paths read. Not exercised:
HD texture workers (no matching pack installed), the Enhanced font and HD pack
downloads (no consent or network), a completed icon-cache write, campaign
backup/restore inside the game, and Windows (MinGW has no LSan). This pass
also does not cover growth of reachable caches over many stages.
