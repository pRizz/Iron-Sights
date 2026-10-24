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

**Status:** Complete. Optimized SDL2/WebGL 2 prototype reaches normal menus and
playable Dam; repeated Chrome/Edge input/audio/focus/watch checks pass.

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

**Next action:** Continue WEB-003 game persistence and its launcher integration.
Broader browser/mission and performance acceptance remains WEB-006.

**Session notes:**

- 2026-10-24 after `e3f7b5e`: implemented WebGL transparent-border sampling in
  language-aware GLSL, per-program/per-draw border uniforms (including retained
  draws), explicit mirror-clamp extension negotiation and mip generation only
  after real texture upload. Ten ROM-free GPU cases passed for point/linear
  edges/corners; the next real-ROM run had no WebGL sampler/mipmap warnings.
  Desktop shader precision is highp; native shader precision stays unchanged.
- 2026-10-24 active input audit: real keyboard input moved Bond about 88 units
  before exposing a float/integer callback mismatch in the collision walker.
  Corrected the callback's three float parameters and added a typed threshold
  adapter. Native and Wasm synthetic-geometry regressions pass. A new optional
  `web_build.py --callback-types` audit enables/fails function-pointer warnings
  despite the inherited `-w`; it additionally found enum/index and debug-thread
  entrypoint type mismatches, now adapted explicitly. All 270/270 video target
  units passed this stricter declaration audit. Runtime input/audio/focus checks
  are being rerun; audio measurements taken after the trap were inconclusive.
  New read-only runtime counters/player-coordinate exports and a QA-only Web
  Audio analyser allow measured checks. These changes are currently uncommitted.
- 2026-10-24 prototype verification: all 271/271 video units passed the strict
  callback audit, including the new focus adapter. The optimized Edge 156 build
  moved Bond 443 units, turned 1.2 degrees, produced Web Audio peak 0.21, held
  simulation at zero steps on focus loss and resumed 31 steps during the next
  half-second interval; idle drift was 1.19 units (below the 5-unit stuck-key
  threshold). The watch released capture, continued animation and recovered
  gameplay capture. Canvas size is 960x540. Earlier silent measurements came
  from tick-1 direct launch bypassing menu initialization, not a broken browser
  audio device; a 120-tick startup delay preserves real initialization. The page
  distinguishes menu releases from unexpected lock loss and resets input before
  requesting capture. Browser-only P opens/closes the watch; Return confirms.
  Native ADS/wardrobe/HUD/state/image-bank/collision regressions passed, as did
  native/Wasm collision checks, 17 Python tooling tests and eight gallery tests.
  The full Windows rebuild and two complete native gallery passes reproduced
  all 72 views/35 icons with zero changed image cells; pistol/rifle ADS reviewed.
  Ten GPU sampler cases passed in Edge and Playwright Chromium. Playwright's
  Windows headless shell did not produce the required focus change in its first
  runtime audit; installed Chrome is being checked separately. Its full bundled
  Chromium executable failed to launch with `spawn UNKNOWN` on this host.
- 2026-10-24 completion: installed Chrome 155 passed the optimized audit (443
  units of motion, 1.2-degree turn, audio peak 0.22, zero unfocused steps,
  31 resumed steps, idle drift 0.60 and watch capture recovery). Edge's normal
  startup/menu navigation rendered 780 frames without a runtime error; local
  screenshots of the file/mission menus were reviewed. The successful Dam
  screenshots and native pistol/rifle gallery references were also reviewed.
  These observations complete the prototype gate; they do not claim full
  missions, hardware performance, Firefox or Safari compatibility.
- 2026-10-24 repeat-check correction: the preceding completion conclusion was
  premature. SDL queued automatic pointer requests that could collide with the
  page's Resume request. Web capture now has one owner: the page's gesture calls
  a C entrypoint that updates SDL focus/relative mode; automatic native capture
  requests are disabled only on web. Intentional releases notify the page first,
  and the page waits for confirmed capture. Three consecutive fast-link audits
  passed after this correction. The focus-motion audit now uses the existing
  invisibility/invulnerability diagnostics to isolate input from enemy impulses;
  normal menu/game launches do not enable them. Optimized repeat checks follow.
- 2026-10-24 final completion: the corrected source passed three consecutive
  fast-link audits, then two optimized Edge 156 audits and one optimized Chrome
  155 audit. Optimized runs measured 435 units of motion, a 1.2-degree turn,
  audio peak about 0.22, zero unfocused steps, 31 resumed steps, 0.70–1.32 units
  idle drift, continued watch animation and recovered capture. No browser API
  capture request was rejected. The strict audit again passed all 271 units.
  The final native rebuild/gallery repeated both full passes with zero changed
  cells; settled pistol/rifle ADS and the browser watch/menu views were reviewed.

## WEB-003 | 2026-10-24 | Save persistence and backups

**Status:** Complete. Actual campaign/profile/settings reload and backup restore
pass through the launcher, alongside storage failure/retry and concurrent-tab checks.

**Completion:** Campaign saves, profiles and settings survive fresh launches;
backup export/import round-trips and failures preserve existing data.

**Starting paths:** `port/src/libultra_shim.c`, `port/src/main.c`,
`port/src/video.c`, `web/src/persistent-files.ts` and `web/src/rom-storage.ts`.

**Evidence:** `web/src/backup.ts` defines a version-1 backup envelope with
allowlisted relative paths, file/total limits, exact 512-byte EEPROM validation,
canonical base64 and per-file SHA-256. Synthetic unit tests round-trip binary
campaign/text settings and reject traversal, ROM/suspend files, duplicates,
truncation, damage and unsupported versions. A real SDK filesystem fixture now
tests C writes through IDBFS, reload, transaction sequencing and backup rollback.
The earlier synthetic checkpoint is superseded by the real-game launcher
evidence below. Actual host quota exhaustion is not claimed.

**Decisions:** Use the stable IDBFS mount `/midas-ads-user` with `/user` as its
runtime alias; this isolates the game from other projects on the same Pages
origin. Hold the origin-scoped Web Lock `project-midas-ads:user-files` for the
writer session. Use a separate IndexedDB database `project-midas-ads:rom`, schema
version 1, for the original ROM Blob. Keep these identities stable across app
deployments and perform explicit migrations for future schema changes.

**Next action:** Preserve these storage identities during WEB-005 deployment and
verify an actual deployed update. Keep suspend images disabled.

**Session notes:**

- 2026-10-24: added a separate original-ROM IndexedDB Blob adapter and the named
  Web Lock. ROM-free real-browser integration passed original-byte preservation
  after working-buffer mutation/reload, aborted-write preservation, deletion,
  second-tab rejection and lock transfer. Three queue tests passed serialization
  of writes during a transaction, dirty-data retention/error reporting/retry and
  burst coalescing. Strict TypeScript checking passed. These adapters are not yet
  wired to IDBFS or game writes; actual campaign durability remains unverified.
- 2026-10-24 filesystem adapter: mount/restore precedes `/user` alias creation.
  Successful writable closes/renames/deletes schedule serialized synchronization;
  monotonic modification times preserve successive writes within one millisecond.
  Validate campaign/backup boundaries before persistence, retain dirty data on
  failure, and prohibit import after the game starts because its EEPROM cache
  would otherwise overwrite imported data. Replacement restores the prior memory
  snapshot on failure. The video target links/exports IDBFS. The separate
  `web_build.py files` CMake target builds a tiny ROM-free SDK fixture; it passed
  real C stdio save/reload, writes during sync, profile/backup round trip,
  injected sync and partial memory-write failure rollback, durable prior-file
  preservation after reload and the running-game import guard. These tests use
  synthetic bytes, not an actual campaign or a real quota exhaustion event.
- 2026-10-24 completion: the actual launcher in Edge 156 and Chrome 155 created
  a 512-byte campaign EEPROM, device bindings, `Web.Audit!.geprofile` and settings
  changed through the native console, then reproduced all backup bytes after
  fresh page initialization. Chrome additionally restored that actual backup
  through confirmation and reproduced its files exactly. Forgetting the real
  remembered ROM and reloading disabled Play while preserving all save files.
  The native profile filename policy now retains valid punctuation/Unicode with
  its 23-byte limit. The ROM-free launcher audit passed invalid backup rejection,
  cancel/restore/export, denied storage protection, second-writer rejection and
  lock transfer/retry. An injected quota-style abort of the real IDBFS transaction
  rolled back memory files, retained durable data, exposed a save error and
  recovered with Retry. Storage/IDBFS fixture reruns and 62 unit tests passed.

## WEB-004 | 2026-10-24 | Browser launcher

**Status:** Complete for the local desktop launcher. First/repeat launch,
storage/error flows and normal Dam capture/audio/focus/watch checks pass.

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
The initial codec checkpoint exposed tests/typechecking/auditing without a
frontend. The local build and accepted browser flows below supersede that
limitation. Deployed-site acceptance remains WEB-005.

**Next action:** Continue WEB-005 packaging and Pages deployment, then WEB-006's
broader mission/browser compatibility and performance acceptance.

**Session notes:**

- 2026-10-24: pushed prototype/storage checkpoint `9e725cb`; both Windows
  builds/provenance and Bright Builds workflows passed. Added a SolidJS page,
  typed engine/ROM/capture controllers, file/drop selection, remembered-ROM
  preference, engine download/initialization stages, save status and retry,
  backup download/confirmed restore, storage protection and fullscreen controls.
  The engine restores IDBFS before Play; saves can be backed up without starting
  a mission. These flows are implemented, not yet accepted by browser tests.
  The static build passes TypeScript and Vite (84 kB JS / 49 kB CSS before gzip).
  MysticUI follows the managed Solid default and its official README setup,
  pinned to `d36017757708ed01ef2b3b47beb14f294726411c`; Tailwind 3.4.19, PostCSS
  8.5.31 and Autoprefixer 10.6.1 are exact pins. Its unused particle dependency's
  install script remains blocked; the selected button builds without it.
  Assembly allowlists runtime/config files, hashes an immutable runtime path,
  verifies the downloaded Wasm digest, and uses the game's generated build-info
  metadata rather than attributing an old binary to the current frontend HEAD.
  No Pages deployment exists yet; the compiled-logo follow-up remains WEB-005.
- 2026-10-24 completion: the assembled site passed real-ROM launch/reload/removal,
  native campaign/profile/settings durability, normal-menu Dam startup, nonzero
  Web Audio, capture, focus pause/resume, watch release/recovery and independent
  save backup checks in Edge 156 and Chrome 155. Outgoing requests were GET only;
  no page errors occurred. Private evidence stays under `build/web-ui`.
  Reviewed the launcher, Dam, watch and repeat-launch screenshots. The ROM-free
  UI audit additionally passed invalid drop/import, confirmed restore/cancel,
  export, declined protection, fullscreen enter/exit, concurrent-tab retry,
  damaged-ROM removal, preference reload and IDBFS failure/retry. Manifest
  metadata is validated before rendering provenance links. Main startup repairs
  missing default settings after an empty restore and surfaces fatal startup
  errors. The page uses component-specific CSS and pinned Prettier 3.9.10;
  strict TypeScript, 62 unit tests, storage/IDBFS tests, scoped formatting and
  Bright Builds checks passed. Static output is 86 kB JS / 49 kB CSS before gzip.
  No game/render source changed in this launcher slice, so the committed native
  gallery remains current. Commands are in `web/README.md`; deployment is next.

## WEB-005 | 2026-10-24 | GitHub Pages deployment

**Status:** Not started. Depends on WEB-004 and passing earlier gates.

**Packaging follow-up:** The inherited always-linked `assets/rarewarelogo.c`
contains initialized logo textures/vertices even with `GE_ROMDATA_FALLBACK=OFF`.
Before publishing, replace those web payloads with ROM-populated backing data
and verify the site contains only approved runtime/configuration outputs. Native
logo behavior must remain unchanged. The optional generated weapon-icon pack is
not enabled in the current web compiler definitions, even when a local pack exists.

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
