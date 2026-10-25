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

**Next action:** Keep the validated-ROM and engine-readiness gates aligned across
both Play controls when extending the launcher. Deployment and compatibility
acceptance are recorded in the completed WEB-005/WEB-006 blocks below.

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

- 2026-10-24 launcher follow-up: `web/src/player.tsx` replaces the central
  Choose ROM action and drop instructions with Play and loaded-ROM copy after
  validation, including remembered ROMs restored on refresh. Play stays disabled
  until the engine is ready and uses the existing startup handler. The private
  launcher audit now checks selection, central-button startup, remembered-ROM
  refresh and the return to Choose ROM after forgetting/reloading. TypeScript,
  scoped Prettier, production assembly/allowlist, Bright Builds and Edge launcher
  UX checks passed. The Edge actual-ROM audit passed central Play startup,
  remembered-ROM refresh/removal, campaign/profile/settings durability and Dam
  audio/capture/focus/watch recovery with no page errors or upload requests.
  The repeat-launch screenshot was reviewed; private evidence is under
  `build/web-ui/central-play`. No engine or native render inputs changed.

## WEB-005 | 2026-10-24 | GitHub Pages deployment

**Status:** Complete. Main CI builds/deploys the ROM-free site; real deployed
update, continued gameplay and unchanged reloaded user files pass.

**Packaging decision:** The inherited always-linked `assets/rarewarelogo.c`
contains initialized logo textures/vertices even with `GE_ROMDATA_FALLBACK=OFF`.
Web now replaces those payloads with ROM-populated backing arrays; native builds
retain the original source. The optional generated weapon-icon pack remains
disabled. Source-selection and binary-payload checks gate assembly/publication.

**Completion:** PR builds verify without publishing; successful `main` runs
deploy the site. Production base paths work, active sessions continue and
deployment updates preserve user files.

**Starting paths:** `.github/workflows/windows-release.yml`, existing provenance
and packaging tools, new web build/deployment workflow and packaging entrypoint.

**Evidence:** The replacement generator creates 38 uninitialized arrays,
removing 23,496 bytes of texture/color/vertex payload from compiled web sources.
All 38 original arrays matched the corresponding private ROM bytes. Five
generator/binary-policy tests and native/Wasm synthetic endian/bounds/atomicity
tests passed. The final Wasm excludes original payload signatures for all 32
substantial texture/vertex arrays in both byte orders. Tiny color constants are
covered by source/BSS checks rather than ambiguous four-byte binary matching.
The packaging policy rejects the original logo, compiled ROM fallback/icons
and initialized generated arrays. Strict TypeScript and 67 unit tests passed.
The optimized build and strict callback audit pass all 272 selected sources,
including the generated logo. The auditor now labels generated sources in the
build tree; ten tooling tests pass. Native ADS/wardrobe/HUD/state/image-bank/
collision/logo checks pass. The full native rebuild and two gallery passes
reproduced all 72 views/35 icons with zero changed image cells; representative
pistol/rifle ADS reviewed, freshness and eight gallery tests passed. The assembled
site passes exact file/digest verification and rejects an injected extra file.
Actionlint 1.7.12 validates the new workflow. The final project-path artifact
passed Edge's ROM-free UX and real-ROM launcher audit, including ROM-backed logo
initialization, normal Dam, audio/capture/focus/watch, campaign/profile/settings
reload, actual backup restore and ROM removal without save loss. An audit race
was fixed by waiting for Play readiness after restoring saves; configuration
downloads may still be finishing when save data is already available.
Pages is configured for Actions at `https://prizz.github.io/Project-Midas-ADS/`
with HTTPS enforced. Commit `2360bc8` passed Browser build and Pages run
`40145610255`, Windows builds/provenance `40145610266` and Bright Builds
`40145610338`. The live manifest identifies that clean commit and its GitHub
Actions build truthfully. Its private live-site audit has reached Dam, created
real user files and is keeping the watch animating while awaiting a second
deployment. The opt-in `MIDAS_WAIT_FOR_DEPLOY=1` audit compares runtime versions,
requires continued native frame logging and then checks recovery/reloaded files.
Deployed update acceptance remains pending.
The Linux Playwright 1.64.1 image additionally passed the production project-path
UI audit, ROM storage, C/IDBFS filesystem and all ten GPU sampler cases against
the assembled local artifact, with container networking disabled.

**Next action:** Preserve the deployment/storage contracts while completing
WEB-006 compatibility and mission/effect coverage.

**Session notes:**

- 2026-10-24 completion: `e4a81be` passed Browser build and Pages run
  `40146369375`. The live audit observed runtime `d18363106e416351` at `2360bc8`
  change to `721967491085aad2` at `e4a81be` after 511 seconds. Native watch frame
  logging advanced from 2,820 to 32,820 while the deployment changed. Closing the
  watch recovered gameplay capture. Fresh initialization then recalled the ROM
  and exactly reproduced campaign/profile/settings backup files. Actual backup
  restore and forgetting the ROM preserved saves. No page errors or upload
  requests occurred. Private evidence stays in `build/web-ui/live-update`.

## WEB-006 | 2026-10-24 | Browser compatibility and polish

**Status:** Desktop preview acceptance complete. Chrome/Edge and Linux Firefox
are tested; Safari remains experimental by the user's explicit decision. Final
controller-fallback publication and live-site acceptance are verified.

**Completion:** Mission/effect coverage, browser support matrix and performance
results are recorded. Optional icons/offline support and browser suspend points
have explicit implementation or deferral decisions.

**Starting paths:** Shared renderer/game code, browser launcher, local audit
tooling and `scripts/port/save_states.md` for snapshot design constraints.

**Evidence:** Windows WebKit (Playwright 1.64.1) exposes WebGL 2, Web Locks and
Pointer Lock and initializes the launcher/saves without a page error. This is
WebKit automation, not actual macOS Safari acceptance. Playwright Firefox fails
to launch on this Windows host with `spawn UNKNOWN`, as did the bundled full
Chromium earlier; installed Chrome/Edge work. An isolated Linux browser image
was tested instead: Firefox 157 initializes successfully under Xvfb with Mesa
llvmpipe and no page errors. Its headless Linux run had no WebGL 2 and showed the
intended graphics-availability error. The browser image is pinned to
`mcr.microsoft.com/playwright:v1.64.1-noble@sha256:d65108d1a8095d8b0c4a5e32bf1807a40a5ce90ff0c78473e1116b1c243235e8`.
The [compatibility notes](web_compatibility.md) now record the eight-mission
matrix, measured limits, tested environments and explicit feature deferrals.

**Session notes:**

- 2026-10-24: Firefox 157's full ROM-free launcher UX passes under Linux Xvfb
  after replacing a thrown Web Locks callback error with direct caller rejection;
  Gecko reported the throw globally despite the request promise's catch. Its
  real-ROM audit reaches normal Dam, creates campaign/profile/settings files and
  initializes the ROM-backed logo, but audio output is not yet demonstrated in
  the isolated Linux environment. Continue investigating the sound backend.
- 2026-10-24: Windows WebKit's full ROM-free launcher UX passes after adding an
  IndexedDB byte fallback for backends that cannot prepare Blob/File storage.
  Blob remains the preferred original-ROM representation. Only clone/unsupported/
  unknown preparation errors retry as bytes; quota/permission/abort failures
  remain failures. The fallback snapshots the immutable original Blob, uses the
  same schema-1 row key and reconstructs a Blob on recall. Browser integration
  verifies original bytes after immediate working-buffer mutation and reload.
  This is compatible with existing Blob rows and needs no destructive migration.
  Actual WebKit ROM/gameplay acceptance is next; real macOS Safari is unverified.
- 2026-10-24: an explicit PulseAudio Unix socket/null sink resolved the isolated
  Firefox host-audio limitation. The standalone oscillator and then the complete
  actual-ROM launcher audit passed: normal Dam, audio, capture/focus/watch,
  profile/settings/campaign durability, backups and ROM recall/removal. Its
  analyser measured peak 0.113 with a running AudioContext. WebKit's actual ROM
  cache now works, but missing Gamepad API support exposed a real early return
  before the keyboard/console event watch was installed. A web-only fallback is
  implemented; optimized build, negative gamepad-path audit and native gallery
  verification are in progress. Native gamepad-init behavior remains unchanged.
- 2026-10-24: added a private stage-query whitelist and mission audit. The first
  Edge sweep passed Dam, Facility, Surface 2, Jungle, Frigate, Caverns, Cradle and
  Egypt for spawning, short movement, ADS, firing, continued graphics and audio,
  without shader/GL/runtime errors. Reviewed representative scenes, ADS and fired
  weapon images. Samples ran roughly 49–60 graphics tasks per second, 19–31 s to
  stable gameplay, and 730726400-byte linear capacity throughout. These are short
  local SwiftShader observations, not completed missions or resident-memory/
  hardware benchmarks. Evidence remains private under `build/web-compat`.
- 2026-10-24 final local acceptance: the optimized gamepad fallback passes all
  272 strict callback checks. With the Gamepad API deliberately removed, Edge's
  full actual-ROM audit observes the fallback log and passes native profile/
  settings edits, audio, capture/focus/watch, durable saves, backups and ROM
  recall/removal. Firefox repeats its full real-ROM audit on this build through
  the checked-in audio/container helpers. Native ADS/wardrobe/HUD/state/image-bank/
  collision checks pass. Both native gallery passes reproduce all views/icons
  with zero changed image cells; freshness/integrity and eight gallery tests pass.
  WebKit now accepts native profile/settings input, confirming the input repair,
  but its Windows runner lacks Web Audio and cannot complete capture recovery.
  It is not advertised as accepted gameplay. The user explicitly chose to keep
  actual Safari experimental. About/build chrome and the root README link the
  browser preview and compatibility notes. Icons, offline support, suspend points
  and threading remain explicitly deferred as documented; no new scope is implied.
- 2026-10-24 publication acceptance: compatibility commit `2c890b2` passed Pages
  run `40148810530`, Windows builds/provenance `40148810523` and Bright Builds
  `40148810562`. The live manifest identifies clean commit
  `2c890b2445ba357231f126bf594a3ac0fae3b542`, runtime `6521ef4a7a8c672b`, and GitHub
  Actions provenance. With Gamepad API support deliberately removed, the actual
  published site passes the complete real-ROM launch/audio/capture/focus/watch/
  campaign/profile/settings/backup/ROM-recall-and-removal audit without page
  errors or uploads. Private evidence is in `build/web-ui/final-published`.

**Next action:** Preserve the published storage/deployment contracts. Future
macOS Safari/hardware-GPU/full-mission testing and optional enhancements require
separate follow-up scope; the accepted desktop preview milestones are complete.

## WEB-007 | 2026-10-24 | Immersive viewport and controls

**Status:** Complete and published in the desktop preview. The full-viewport
room replaces the initial visible launcher; F2 retains access to its controls.

**Completion:** Full-viewport entrypoint, retained controls on F2, Alt+Enter,
stable game canvas and tested transition decisions.

**Evidence:** [Room plan](web_room_plan.md) records realistic props, returning
power-switch interaction, vertical turntable spin and P retaining the watch.
AGENTS routes later sessions here; the chrome exception is documented.
`web/src/app.tsx` retains the Solid shell/game canvas and adds F2 controls,
Alt+Enter fullscreen and post-interaction hints. `web/src/room/flow.ts` has six
pure transition tests; all 73 package tests and strict TypeScript pass.
Edge/Chrome ROM-free room audits cover viewport, physical cartridge hover/click,
keyboard selection, invalid ROM, stable canvas through F2, fullscreen, portrait,
reduced motion and graphics-context fallback. The production project prefix
works. Existing Edge launcher UX and actual-ROM Dam/save regressions pass with
F2 entry. Opening controls pauses/releases gameplay; closing requires Resume.

**Next action:** Preserve these canvas/storage boundaries in future room work.
WEB-010 records the final publication and update-preservation evidence.

**Framing refinement:** `presentation.ts` now fits the actual native framebuffer
to a centered black game stage in viewport, controls and fullscreen. Resizing
does not alter the buffer or mouse-coordinate bounds. DOM/native-game audits
cover wide and portrait windows, fullscreen and F2 toggles. The canvas is removed
from layout flow and size writes are deferred/deduplicated after WebKit exposed
resize-observer feedback in the initial implementation.

## WEB-008 | 2026-10-24 | Room scene and assets

**Status:** Published baseline complete; the requested licensed console/controller
replacements are implemented and visually verified locally. Publication is WEB-010.

**Requested refinement:** `codex/room-models-and-framing` vendors Swayne's CC BY
4.0 console and Marshle's CC BY 4.0 controller. `MODELS.md` records pinned public
redistributions, original attribution, source/output hashes and modifications.
The deterministic preparation script keeps only the console, isolates its actual
power switch, preserves normalized vertex colors and packages the controller's
unchanged mesh/normals/UVs/PNG. Regeneration twice reproduces the same outputs:
203,896-byte console and 1,204,348-byte controller. These supersede the procedural
console/controller, retain original cables/LED and ship no unrelated cartridge.
Final room/returning/insertion captures were reviewed in `build/web-room/framing-*`.

**Completion:** Visually reviewed room/CRT/console/cartridge, hover/click targets,
candidate asset inspection and attribution with explicit stand-in tracking.

**Evidence:** Candidate links/licenses are in the room plan. The console's official
download endpoint requires authentication (HTTP 401); the TV has a public GLB.
The inspected TV is only 100 triangles with plain materials and does not supply
the requested realistic CRT. Its 9,292-byte local GLB/hash and candidate licenses
are recorded in `web/src/room/README.md`; neither candidate is shipped.
Landscape, portrait, insertion, flight and inserted-cartridge captures were
visually reviewed under `build/web-room`. The user's subsequent explicit request
replaces the text-only label with their supplied 1000x1000 AVIF. Its unchanged
68,421 bytes, SHA-256, label UV rectangle and separate artwork notice are in
`web/src/room/ARTWORK.md`. Build verification checks the exact public image.

**Next action:** Preserve the licensed-model/asset provenance. The authenticated
candidate remains an optional future replacement, not a preview dependency.

**Continuation:** The first desktop preview shipped refined original procedural props.
The completed pass adds shaped console/cartridge bodies, detailed controls,
glass/screen effects and room materials, reviewed in the complete insertion/TV
sequence. This is an original reconstruction, not the Joseph Rohm candidate.
The refined modules replace `props.ts` with shaped console/cartridge shells,
recessed ports/vents/switches, a three-prong controller, curved physical CRT glass,
native screen shaders and deterministic surface grain/reflection lighting.
The supplied label remains unchanged. Windows WebKit's AVIF decoder failure
motivated a lossless PNG retry: 588,603 bytes, identical decoded RGBA data,
separately hashed/allowlisted and documented in ARTWORK.md. Neither earlier
candidate, ROM-extracted assets nor local diagnostic captures were shipped.

## WEB-009 | 2026-10-24 | Cartridge onboarding and game handoff

**Status:** Desktop interaction/handoff acceptance complete. Edge, Chrome and
Linux Firefox pass; published-site acceptance is recorded in WEB-010.

**Requested refinement:** Background dragging now pivots the floating cartridge,
with bounded pitch, pointer capture/cancellation and suppression of the release
click. Direct cartridge selection remains intact; a browser regression ends a
background drag over the cartridge without opening its dialog. Insertion eases
manual tilt/yaw into alignment; returning users still click the physical switch.
The camera endpoint now derives from the screen dimensions, aspect and field of
view. Four unit-tested ratios and actual production-scene landscape/portrait
captures contain every screen corner, with one axis flush and no overshoot.
The actual-ROM audit checks projected corners before GPU teardown, then normal
Dam/audio/P watch/focus/capture, native canvas sizing, reload and switch startup.
Evidence: `framing-final-game`, `framing-final-ui` and `framing-firefox-game`.
Chrome also passes actual reduced-motion handoff/reload/power startup with the
licensed props and native sizing checks in `framing-reduced-game`.

**Exit/startup follow-up, 2026-10-25:** Implemented directly on `main`. Native
Files-screen Exit Game now pauses the live browser instance, releases input and
audio, flushes saves and reverses the camera/power sequence into the room. The
paused game remains visible while room assets reconstruct; a crossfade precedes
retreat. The inserted cartridge and physical power switch resume the same native
instance, without a second `main` call or replacement canvas, and handoff waits
for new graphics tasks. The save-writer lock stays held. Native desktop quit is
unchanged. Reduced motion uses the short fade route.

Fresh page loads auto-start a validated remembered ROM after engine readiness,
without loading the room or prop GLBs. Explicit exit stays in the room. F2 and
**Restart to manage saves** retain pre-start backup restoration: the latter
flushes and reloads into `?controls=1`, holding startup until Play or closing
controls. The query is removed when controls close. `session.ts` tests entry
decisions; 112 unit tests and strict TypeScript pass. Edge and isolated Linux
Firefox pass actual native No/Yes, reverse transition, paused frames, silent
audio, released input, physical warm power resume, repeated exit, stable saves
and management restart. Chrome passes that cycle with reduced motion. The full Edge launcher actual-ROM audit also passes
Dam/audio/capture/watch/focus, campaign/profile/settings reload, actual backup
restore and remembered-ROM removal. Evidence: `lifecycle-game`,
`lifecycle-saves-fixed`, `exit-final-edge`, `exit-firefox` and `exit-reduced-chrome`.

The optimized Wasm and all 272 strict callback compilations pass. The native
gallery's two independent capture passes reproduce all views/icons with zero
changed image cells; source fingerprint/HTML are refreshed, verify and eight
gallery tests pass. Desktop game rendering is unchanged. Publication acceptance
is recorded in WEB-010; earlier fresh cached-cartridge power requirements below
are historical and are superseded by this follow-up.

**Completion:** First/repeat ROM flows, insertion/power/flight, loading/errors and
actual-game handoff with audio, capture/watch and durable saves.

**Evidence:** Existing ROM/storage/game interfaces are accepted in WEB-003/004.
The private `web/tools/room-game-audit.mjs` passes first selection, eased insertion,
power/TV flight, stable game-canvas handoff, actual native startup and nonzero
audio after a menu gesture, identical save files after reload, an already-inserted
returning cartridge and startup through the physical power-switch raycast target.
No page errors or non-GET requests occurred. The room RAF/resources are disposed
after the game's first graphics tasks. Private evidence is `build/web-room/game`;
the retained launcher's full normal Dam/save audit is `build/web-room/shell-game`.
The final-artwork flow also passed against the production prefix in
`build/web-room/game-final`, and its inserted-cartridge screenshot was reviewed.
One initial audit incorrectly paused via F2 before waiting for native frame 240;
the audit now waits for startup frames before opening controls. That test failure
was not evidence of an engine startup failure.

**Next action:** Preserve the verified handoff/storage/capture contracts; use
the private cinematic/gameplay audits for future changes.

**Continuation acceptance:** Edge and Chrome and isolated Linux Firefox passed
actual-ROM selection/insertion/flight/audio, unchanged reloaded save files and
the physical returning power switch. The room WebGL context is confirmed lost
after handoff and the game canvas remains the same node. Edge additionally
passed normal-menu Dam movement, P watch, focus freeze/recovery and capture after
the cinematic; Chrome passed the actual reduced-motion/fade route. Evidence is
in `build/web-room/polish-*-game`, `final-edge-game` and `final-reduced-game`.
The gameplay audit samples sound before deliberately pausing; its earlier
post-pause sample correctly saw a suspended context and was fixed in the audit.
Future runs can enable `MIDAS_ROOM_GAMEPLAY=1` or `MIDAS_REDUCED_MOTION=1`.

## WEB-010 | 2026-10-24 | Immersive compatibility and publication

**Upstream 1.0.2b integration, 2026-10-25:** Integrated pinned upstream
`9062f9f120b7f64d39e5db59500ed50e8e7a6686`; FORK-026/027 record ownership,
conflict decisions and the deliberate baseline advance. Browser shader prewarm
and audio pause/metrics move into private platform includes. Audio pause/resume
now resets upstream's field/clock/stall/drift pacing state. ROM storage, save
schema, room flow, input capture, loading surface and multi-tab ownership retain
their existing behavior. Optimized Wasm and 274/274 strict callback units pass;
TypeScript and all 119 browser unit tests pass. Real-ROM Edge room/Dam/audio,
reload/remembered startup, native Exit Game/retreat/warm resume and save-management
restart pass. Chrome passes loading UI, first/cached shader preparation before
graphics, uninterrupted cutscene/control/watch and real focus/capture recovery.
Linux Firefox passes campaign/profile/settings backup, reload/restore, ROM
recall/removal and Dam capture/audio/focus/watch. Windows/Linux native builds and
the independent 72-view/35-icon gallery pass with zero changed image cells.
Private evidence is `build/merge` and `build_win/hand-gallery-337riqu0`.
Next action: preserve these contracts on future work; verify main CI and the
published artifact after pushing this integration. Safari remains experimental.

**Status:** Desktop compatibility, packaging and Pages publication pass. Actual
Safari remains explicitly experimental.

**Startup/feedback follow-up, 2026-10-25:** Implemented and locally verified on
main. A centered loading surface covers download and runtime initialization,
including remembered-ROM launches. Download percentages use actual bytes;
graphics preparation uses completed program counts; other stages are indeterminate.
Real pauses during preparation expose **Resume startup**; Chrome's reduced-motion
audit interrupts preparation through F2, resumes and completes the natural Dam
cutscene/watch/focus checks (`control-startup-paused`).
The native browser path prepares the existing 87-program shader list before
visible logos and yields between programs. The room holds its final TV frame
without drawing during preparation. Native desktop scheduling/rendering is unchanged.

Cold Edge/SwiftShader traces show the initial logo sequence improving from
roughly 8-11 graphics fps to 57-60 fps. A cold main-to-first-frame preparation
sample takes 6.97 s, versus 0.92 s after cache warm-up. The work moves behind
progress; no hardware-GPU or overall compilation-elimination claim is made.
Evidence is private in `build/web-startup/baseline-sources` and `prepared`.

The canvas's gold focus outline is removed; UI controls retain focus indicators.
Startup failures, especially another tab's save-writer lock, open a centered
accessible retry dialog. The protection remains, with recovery after closing the
other tab. Cutscene/watch arrivals no longer trigger the capture monitor's
100 ms false pause: keyboard play stays live, with a small capture affordance
when needed. Actual focus loss, manual pause and unexpected pointer release still
pause, including capture rejection after a focus event. Unit regressions cover
those distinctions. Actual Dam cutscene/control/watch/focus acceptance passes
in Edge and isolated Linux Firefox (`control-edge-fixed`, `control-firefox`).
The Edge room/game/reload, Chrome full launcher saves/backup/ROM removal and
repeated native Exit Game/warm-resume audits pass (`room-game-edge-final`,
`launcher-game-chrome-final`, `exit-edge-final`). Menu audits now use native
stage/control counters and Files initialization instead of fixed logo timing or
expecting an artificial Resume overlay.

Optimized Wasm and all 272 strict callback compilations pass; logo/spike and
eight gallery regressions pass. Both native gallery captures reproduce every
view/icon with zero changed image cells; source fingerprints/HTML are refreshed.
119 unit tests, strict TypeScript, scoped formatting and exact public-artifact
verification pass. Edge and Windows WebKit pass loading/compilation/F2,
centered-lock/retry and room/fallback checks. Chrome reduced-motion cutscene/
control/watch/focus acceptance also passes (`control-reduced-chrome`); the final
startup audit proves all listed programs finish before graphics and loading stays
visible until then (`final-startup-proof`). The ROM-free feedback audit is part
of Pages CI. Safari remains experimental.

**Startup publication acceptance:** Direct-main source
`ce782cb561ee121daa27171df21028b0779de58c` passed Pages `40223366320`, Windows
builds/provenance `40223366337` and Bright Builds `40223366331`. The live manifest
reports clean Actions runtime `653be0dbfb70c828`. Public Wasm is 3,920,098 bytes,
SHA-256 `49c91ddd60012af64578dd6397734f7a8a62d527b5b20b7ac5fc23005a0f269c`;
size/digest/MIME/validation, frontend chunks, models and artwork match the artifact.
Live Edge passes download/compilation/F2, centered other-tab warning and retry.
Live Chrome verifies first and remembered-ROM startup: every listed program is
prepared before graphics, and loading stays visible throughout initialization.
Live Edge also passes reduced-motion F2 interruption/Resume startup, a complete
natural Dam cutscene/control/watch transition without false pause, and capture/
real focus/Escape recovery. No page errors occurred. Evidence is private in
`build/web-startup/published-ui`, `published-startup`, `published-control` and
`build/web-room/public-artifact-report.json`. The startup/feedback follow-up is
complete; preserve these contracts in later changes.

**Requested refinement:** Local acceptance passes with 104 unit tests, strict
TypeScript, assembly/allowlist and deterministic model regeneration. The model
GLBs are exact size/hash additions with embedded resources only; third-party
notices include complete credits/change records. Edge, Chrome, Windows WebKit
and Linux Firefox pass room/drag/framing/fallback checks; Edge and Linux Firefox
pass actual-ROM handoff, proportionate viewport/fullscreen/control sizing,
durable reload and physical switch startup. Edge also passes normal Dam movement,
P watch and capture/focus after the cinematic. Existing Edge launcher backup/
quota/tab-lock checks pass. Private evidence is `build/web-room/framing-*`.
An idle Edge sample shows 38 draws/30,500 triangles and about 28 fps at full scale;
this remains software-host evidence. The launcher is about 100 kB JS (36 kB gzip),
the lazy scene 668 kB (172 kB gzip), plus 1,408,244 bytes of model GLBs. Vite's
scene-size advisory remains. These changes shipped directly in `ed459f2` and
`69842af`; Pages `40164551815`, Windows builds/provenance `40164551755` and Bright
Builds `40164551825` all passed.

**Delivery correction, 2026-10-25:** The owner requested direct-to-main delivery.
The rule is persisted in AGENTS/CONTRIBUTING and the local standards overrides.
PR #2 was opened before that request; it is superseded by direct-main delivery
of the verified commits, with normal push-triggered CI/Pages acceptance retained.

**Lifecycle publication acceptance, 2026-10-25:** Source `ea804df` was committed
and pushed directly to main. Pages `40167224238`, Windows builds/provenance
`40167224278` and Bright Builds `40167224228` passed. The public manifest reports
clean Actions source `ea804dfefe1d0b763b99ebfe61b38f71e2632f18` and runtime
`04853c2b485e431a`. Public Wasm (3,918,950 bytes), SHA-256, MIME and validation,
frontend chunks, both model GLBs and artwork hashes match the exact artifact.
The live Edge audit passes native Exit Game No/Yes, reverse flight, frozen
frames, silent audio/released input, physical warm power resume, repeated exit,
identical save backups and management restart. The separate live Chrome audit
passes first insertion/flight, normal Dam movement/audio/P watch/focus/capture,
stable canvas, GPU teardown and unchanged saves after a reload that automatically
starts without room/prop downloads. No page errors occurred. Private evidence is
`published-exit`, `published-autostart` and `public-artifact-report.json` under
`build/web-room`. The requested lifecycle follow-up is complete; the checks in
WEB-009 and experimental Safari status remain applicable.

**Completion:** Keyboard/reduced-motion/responsive/failure paths, production
allowlist and browser verification, measured scene cost and verified deployment.

**Evidence:** Prior desktop acceptance is WEB-006, not acceptance of this new scene.
Three.js 0.186.1 and types 0.186.0 are exact pins. The first scene was bundled
with the frontend (approximately 654 kB JS / 176 kB gzip, 52 kB CSS); the supplied
AVIF adds 68,421 bytes. Vite flags the JS chunk above 500 kB; code splitting and
hardware frame-cost measurement were follow-ups at that checkpoint. The
continuation below records the completed code splitting and software metrics.
Assembly adds dependency/artwork notices, explicitly hashes the artwork and
retains the existing exact artifact/Wasm verifier. The workflow adds the ROM-free
room audit under `/Project-Midas-ADS/`; no ROM is supplied to CI. Local screenshots
are in ignored `build/web-room`; no native gallery input changed.

**Next action:** Maintain the accepted preview. Actual macOS Safari,
hardware-GPU performance measurements and optional candidate-model replacements
remain future scope; no accepted room milestone depends on them.

**Continuation evidence:** 94 unit tests and strict TypeScript pass. Dynamic
scene loading leaves about 98 kB initial JS (35 kB gzip) plus a 603 kB scene
(154 kB gzip); the Vite scene-size advisory remains, while the shell is usable
without that chunk. A tested parser validates the complete Vite output graph
before allowing dynamic chunks into the exact public artifact. Static batching
and lower subdivision reduce the sampled scene from 307 draws/177,510 triangles
to 86/52,710. Idle drawing is intentionally capped at 30 fps. An isolated Edge
1440x900/SwiftShader sample measured 28 fps and about 1.2 ms mean JS render
submission after shader warm-up; this is not GPU-timer or hardware evidence.
The updated audit records metrics, verifies no drawing behind F2 controls and
tests failed-chunk recovery. Initial PNG-fallback/source equality is verified;
the final cross-browser and publication results follow. Linux Chromium's
ROM-free production-prefix room audit passes, as does Linux Firefox's scene and
actual-ROM flow. No native render/gallery input changed.
Edge, Firefox and Windows WebKit now pass the final room UI/fallback checks.
WebKit uses the lossless PNG; this remains WebKit automation, not macOS Safari
acceptance. Forced AVIF-fetch failure also verifies PNG retry in a capable
browser. The standard launcher stays available after a missing scene chunk,
and failed-room return is disabled rather than exposing a blank canvas.
Resource-stressed Linux software samples were slow despite batching, so a tested
room-only quality controller reduces pixel ratio to 75% then 50% after eight
net slow frame intervals over 80 ms. Fast frames reduce the score; it ignores
isolated hitches and does not
change the game canvas. The final adaptive checks and publication results follow.
Final Chrome and Linux Firefox adaptive room audits pass. The sampled Chrome
run retained full resolution at 28 fps; the constrained Linux Firefox sample
engaged 75% scale and measured 5.3 fps, so it is functional software-host
acceptance rather than a smooth hardware-performance claim. Published users
should keep browser hardware acceleration enabled. Windows WebKit room-only
checks pass with PNG retry; actual Safari/gameplay status remains experimental.

**Publication race repair:** PR #1 and its checks passed, and merge `b38a21c`
passed the main Windows/standards workflows. Pages run `40159647626` then caught
a fast-cache race in the launcher audit: a damaged ROM's modal could appear
before F2 and block the Forget-ROM controls. Deployment was correctly withheld.
F2 now closes an idle ROM/error modal and opens controls; active validation and
backup-confirmation modals remain guarded. The room audit explicitly exercises
this route. Repair `08a07d8` passed local browser checks and all main workflows.

**Publication acceptance:** Clean source
`08a07d82c107365c7999dda104fd475058d96179` passed Pages `40160374496`, Windows
builds/provenance `40160374470` and Bright Builds `40160374431`. The live manifest
reports Actions provenance and runtime `faad72845829175b`. Public Wasm size
(3,918,755 bytes), SHA-256, MIME type and validation match that manifest; all
frontend chunks load, and both artwork files match their recorded hashes.
The published site's real-ROM audit passes insertion/flight, normal Dam with
movement/audio/P watch/focus/capture, stable game canvas, room GPU teardown,
unchanged save files after reload and physical returning power-switch startup.
No page error or upload request occurred. Private evidence is
`build/web-room/published-room` and `public-artifact-report.json`.

A separate game left running across deployment from `03aa3b6`/runtime
`91ba238318cd2da6` continued from graphics frame 2,400 to 22,020 over 334.6 seconds.
After reload into the new room, campaign/profile/settings backup bytes remained
identical; actual restore and ROM removal also passed. Evidence is private in
`build/web-room/live-update-final`. WEB-007 through WEB-010 are complete for the
accepted desktop preview. Later documentation-only publication refreshes do not
change the accepted feature behavior.
