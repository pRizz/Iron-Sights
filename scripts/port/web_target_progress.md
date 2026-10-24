# Browser build milestone progress

The user accepted [the browser build plan](web_target_plan.md) on 2026-10-24.
Implementation proceeds through its six technical gates. Each block owns its
status and evidence; update the relevant block and append dated session notes
inside it. Preserve earlier evidence and describe superseded decisions explicitly.
Append any new milestones at the end with a new stable ID. Do not add a shared
current-status table or rewrite unrelated blocks.

## WEB-001 | 2026-10-24 | Portability spike

**Status:** Complete. Root core compile/link, pointer/layout strategy, private-ROM
runtime smoke and native regression/gallery validation passed.

**Completion:** Emscripten compiles/links the game core; pointer classification,
data layout and memory strategy have demonstrated solutions.

**Starting paths:** `CMakeLists.txt`, `port/src/pcsched.c`,
`port/include/port_hostptr.h`, `port/src/segments.c`,
`port/src/ucode_dl_stubs.c`, `port/src/main.c`.

**Evidence:** `python scripts/port/web_spike.py all --jobs 4` built and ran both
ROM-free probes with the pinned Emscripten 6.0.13 image. The tested engine ABI
contracts passed. Compact allocation reproduced segment-word/host-pointer
overlap; the high-data-base experiment avoided that overlap with final linear
capacity 434896896 bytes versus 181272576 compact. These are synthetic Node/Wasm
capacity measurements, not browser gameplay or resident/peak game memory.
The initial normalized compile-only survey compiled 246/257 real headless game units,
with 11 failures; exit 1 correctly reports those failures. Logs and JSON are
local under `build/web-spike`. See the [runbook](web_target_spike.md) for the
normalization policy, exact measurements and failed source groups. The full
game had not linked or run in Wasm at that initial survey checkpoint. The root
build and runtime evidence below supersede that limitation.

The real root core now compiles and links at final Wasm optimization levels 0
and 2, with linker warnings fatal. Both passed the private stock-ROM smoke in
Dam for 360 scheduler pumps (62 and 59 submitted graphics tasks respectively).
Startup segment/host-address contracts passed. Linear capacity remained
536870912 bytes. These are core runtime/capacity results, not browser rendering
or resident/peak browser memory measurements.

**Next action:** Continue WEB-002 browser sampler/input/audio acceptance. Keep
the high data base as the initial correctness layout; compact addressing and
resident-memory optimization require separate measured work.

**Session notes:**

- 2026-10-24: Accepted scope and handoff files created. Source inspection found
  native high-address allocation, nested blocking scheduler waits, incomplete
  ES support and PE/ELF-specific suspend metadata. These are investigation
  targets, not reproduced Wasm failures. Existing unrelated staged standards
  adoption was preserved; refreshed `origin/main` matched local HEAD `cdd6b5f`.
- 2026-10-24: Pushed standards adoption `bc6a949` and accepted plan `761b6e0`.
  Added `web_spike.py`, `web_spike_audit.py`, the SDK image pin, standalone CMake
  probe, runbook and tooling tests. No native sources, build configuration or
  render inputs were changed by this slice. The ordinary root Wasm build fails
  on x86 SSE flags; the diagnostic survey exposes four declaration failures,
  two anonymous-field failures, four assembler failures and the native
  `execinfo.h` dependency. Production pointer layout and full linking remain
  open. Continue with the next action above; no Pages deployment or browser
  gameplay is claimed.
- 2026-10-24 verification: seven tooling unit tests and Python compilation
  passed; both Wasm probes repeated their measurements on rerun. The pinned
  Markdown formatter, local links and whitespace checks passed. The compile
  survey's exit 1 is an observed portability failure, not a passed build.
- 2026-10-24 runtime work: added a root Emscripten configuration, Wasm section
  metadata, linear-memory arenas, Asyncify yielding and browser suspend stubs.
  The initial full core link validated the local ROM but trapped on legacy
  function signature mismatches. Recovered 205 game declarations from Clang AST
  definitions, with three hosted functions declared separately. Corrected the
  vertex callback and watch wrappers' return types and legacy call arguments.
  `scripts/port/web_build.py core --smoke --rom baserom.u.z64` is the current
  reproducible build/smoke entrypoint; outputs stay in `build/web/core`.
  This work is uncommitted, and full core runtime/native/gallery verification
  is pending. The high data base is the initial correctness strategy; compact
  pointer encoding remains a later optimization rather than a gameplay change.
- 2026-10-24 runtime validation: the earlier startup trap was resolved with
  authoritative function declarations and callback/return corrections. Further
  Dam testing found GCC object-order dependence in the texture bank reset;
  snapshots now enumerate real objects and sizes. Corrected the positioned-joint
  callback's extra argument. Added strict texture/DMA bounds, SDK symbol isolation,
  CMake presets, a fast `--link-opt 0` iteration mode and a reproducible
  [runtime runbook](web_runtime.md). The core smoke now sets environment fields
  in `preRun` and requires Dam's explicit frame-limit report; implicit exit zero
  does not count as success. Eight declaration-tool tests and the native/Wasm
  image-bank mutation/reset regression passed. Full optimized root core link and
  private ROM smoke passed. Native ADS/wardrobe/HUD/state checks and a full
  Windows build passed. The gallery regenerated all 72 views and 35 icons,
  repeated each sample and verified a second independent process pass: zero
  changed image cells. Representative native pistol/rifle ADS references were
  visually reviewed; gallery freshness/integrity and seven gallery tests passed.
  A fresh Clang AST scan reproduced all 205 generated declaration records and
  their header exactly. The SDK-configured web preset passed configuration.
  The updated survey entrypoint also passed its ordinary root build and
  compiled all 259/259 selected units, with zero failures. Its current JSON
  records `root_cmake_build_returncode`; the old native-prefixed field belonged
  to the initial pre-platform survey and has been replaced.

## WEB-002 | 2026-10-24 | Playable prototype

**Status:** In progress. SDL2/WebGL 2 target links and renders Dam locally.

**Completion:** A supplied ROM reaches the menu and playable Dam with correct
rendering, sound, mouse and keyboard; focus loss recovers safely.

**Starting paths:** `src/boss.c`, `port/src/pcsched.c`, `port/src/romdata.c`,
`port/src/video.c`, `port/src/audio_pc.c`, `port/src/input_pc.c`,
`port/fast3d/gfx_opengl.cpp`, `port/fast3d/gfx_sdl2.cpp`.

**Evidence:** The pinned SDK built the actual video source set. Private local
Edge 156 (Windows, headless SwiftShader) loaded/rendered Dam and continued past
720 pumps/713 graphics tasks after a Resume gesture. Its runtime log sampled
57 fps / 57 ticks per second after startup, with substantial shader/load hitches.
Linear memory grew to 730726400 bytes. These are one local software-rendered
baseline, not a supported-browser matrix or hardware performance promise.
Local screenshots/logs stay under `build/web-audit/edge`; they are not published.
GLSL ES rejected an integer `mix` factor; explicit float conversion fixed shader
compilation. Startup and continued character simulation no longer trap.

**Next action:** Correct WebGL font-border/sampler and initial mipmap warnings;
validate menus, active keyboard/mouse movement, sound output and input reset on
focus loss. The temporary file-picker harness is not the final launcher. Resume
works, but an early capture can be released by startup's SDL mouse-mode change;
the final launcher must present the appropriate gesture after initialization.

## WEB-003 | 2026-10-24 | Save persistence and backups

**Status:** Backup codec groundwork implemented; filesystem persistence and
actual campaign validation still depend on WEB-002.

**Completion:** Campaign saves, profiles and settings survive fresh launches;
backup export/import round-trips and failures preserve existing data.

**Starting paths:** `port/src/libultra_shim.c`, `port/src/main.c`,
`port/src/video.c`, browser storage adapter to be added.

**Evidence:** `web/src/backup.ts` defines a version-1 backup envelope with
allowlisted relative paths, file/total limits, exact 512-byte EEPROM validation,
canonical base64 and per-file SHA-256. Synthetic unit tests round-trip binary
campaign/text settings and reject traversal, ROM/suspend files, duplicates,
truncation, damage and unsupported versions. No actual browser filesystem,
storage-failure or campaign persistence test is claimed yet.

**Decisions:** Use the stable IDBFS mount `/midas-ads-user` with `/user` as its
runtime alias; this isolates the game from other projects on the same Pages
origin. Hold the origin-scoped Web Lock `project-midas-ads:user-files` for the
writer session. Use a separate IndexedDB database `project-midas-ads:rom`, schema
version 1, for the original ROM Blob. Keep these identities stable across app
deployments and perform explicit migrations for future schema changes.

**Next action:** Mount/restore before game startup, synchronize closed user-file
writes with a serialized queue and visible durability/error status, then add
backup import rollback while the game is idle. Verify errors, concurrent tabs
and restoration after reload. Keep exact-build suspend images disabled.

## WEB-004 | 2026-10-24 | Browser launcher

**Status:** Tooling and ROM validation groundwork implemented. Launcher UI still
depends on the runtime/storage interfaces from WEB-002 and WEB-003.

**Completion:** First/repeat launch, remembered ROM, staged loading, Play/Resume,
fullscreen, storage controls and recoverable errors work without ROM uploads.

**Starting paths:** New SolidJS/TypeScript launcher; existing ROM validation and
build provenance interfaces.

**Evidence:** `web/` has pinned Bun/SolidJS/Vite/TypeScript/Playwright dependencies,
strict source type checking and a private prototype audit. ROM parsing recognizes
three byte orders without mutating stored originals, checks exact size before
reading and validates normalized SHA-256. Combined ROM/backup tests: 33 passed.
Private local validation also accepted the user's real ROM in all three byte
orders, using in-memory swapped copies.
This package currently exposes tests/typechecking/auditing, not a production
frontend build. No remembered-ROM, loading, fullscreen or storage-control UX
acceptance is claimed.

**Next action:** Define explicit launch states and typed runtime/storage
interfaces; implement file/drop selection and browser capability checks.

## WEB-005 | 2026-10-24 | GitHub Pages deployment

**Status:** Not started. Depends on WEB-004 and passing earlier gates.

**Completion:** PR builds verify without publishing; successful `main` runs
deploy the site. Production base paths work, active sessions continue and
deployment updates preserve user files.

**Starting paths:** `.github/workflows/windows-release.yml`, existing provenance
and packaging tools, new web build/deployment workflow and packaging entrypoint.

**Evidence:** No Pages workflow, publication or update test yet.

**Next action:** Build a ROM-free web artifact with pinned inputs; validate
packaging and run a real deployed update/persistence test before completion.

## WEB-006 | 2026-10-24 | Browser compatibility and polish

**Status:** Not started. Depends on WEB-005 for deployed-site validation.

**Completion:** Mission/effect coverage, browser support matrix and performance
results are recorded. Optional icons/offline support and browser suspend points
have explicit implementation or deferral decisions.

**Starting paths:** Shared renderer/game code, browser launcher, local audit
tooling and `scripts/port/save_states.md` for snapshot design constraints.

**Evidence:** No cross-browser or performance measurements yet.

**Next action:** Define a representative mission/effects matrix for current
Chrome/Edge, Firefox and Safari. Keep unsupported browsers/features visible;
scope optional enhancements separately after core acceptance.
