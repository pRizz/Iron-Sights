# Browser build plan

The browser target will run Project Midas ADS locally through WebAssembly,
with a user-supplied GoldenEye ROM, persistent campaign saves and downloadable
backups. GitHub Actions will build and deploy successful pushes to `main` to
GitHub Pages. The user accepted the design and six milestones on 2026-10-24.

Use [milestone progress](web_target_progress.md) for implementation status,
verification and session handoffs. Approval of this plan is not evidence that
the target compiles or that browser gameplay has been validated.

## Accepted scope

- Start with desktop browsers and solo campaign play.
- Keep CMake as the game build system and the existing `ge` target. Use a
  separate Emscripten configuration/build directory alongside native builds.
- Compile C/C++ to Wasm32 using a pinned Emscripten SDK, with SDL2 integration
  and WebGL 2. Keep native builds working.
- Begin with single-threaded execution. Disable the optional render thread and
  native USB adapter polling in the web configuration, including settings that
  could re-enable them. Evaluate Asyncify at scheduler waits for the prototype;
  replace it with explicit frame stepping if measurements justify that work.
- Build a small SolidJS/TypeScript launcher with a static frontend build.
  Follow the repository's Bun/frontend standards and keep gameplay in C/C++.
- Let players drop a ROM or choose a file. Remember it on the device by default,
  with a visible option to opt out and a separate delete-ROM control.
- Store the original ROM in IndexedDB. Never upload it. Give the game a working
  copy because runtime conversion mutates ROM-backed data.
- Persist campaign saves, profiles and settings through an IndexedDB-backed
  virtual filesystem. Offer backup download and restore.
- Require a Play/Resume gesture for browser audio and mouse capture. Support
  Pointer Lock and optional fullscreen, with pause/input reset on focus loss.
- Show real download/initialization stages and recoverable failures. Use
  determinate progress only when measurable, and yield during lengthy work.
- Start with the existing weapon-name fallback. Port optional weapon-icon
  generation later; browsers cannot run the native helper executable.
- Defer browser suspend points, offline support and performance-driven threading
  until the basic game, persistence and deployment are proven. Mobile/touch,
  netplay and cloud saves are outside the initial delivery.

## Touch extension accepted 2026-10-25

The owner requested keyboard-free play on iPhone, iPad and Android after the
desktop preview. WEB-011 extends the original desktop-only scope with automatic
touch detection and a saved manual switch, analog movement, drag-to-look,
multi-finger Fire, toggle ADS and use/reload/crouch, weapon/gadget/zoom/detonate controls,
native menu/watch navigation and on-screen pause/options. Keep proportional game
presentation and safe-area insets in portrait and landscape. Touch play must not
require Pointer Lock or fullscreen, and input must reset on pause, visibility
loss, cancellation, rotation and returning to the room. Use a browser-only input
adapter and small shared hooks; preserve native keyboard/mouse/controller and
ADS behavior. Automate mobile viewport/multi-pointer and real-ROM input checks;
distinguish those checks from physical iOS/Android acceptance.

## Build and runtime design

Select browser configuration with CMake's `EMSCRIPTEN` condition, keeping
platform settings in a focused CMake include and adding convenient presets.
Native and web builds use separate build directories/toolchains. A repository
entrypoint should build the launcher and assemble its output with the game.

The intended game build interface, after implementation, is:

```sh
emcmake cmake -S . -B build_web -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGE_VIDEO=ON \
  -DGE_ROMDATA_FALLBACK=OFF
cmake --build build_web --target ge
```

Produce JavaScript loader and Wasm files, plus permitted configuration/font/
license files. Do not package ROMs, extracted models/textures, generated game
data, native executables or local diagnostic captures. The limited existing
review-gallery exception does not authorize shipping game assets in the site.

Mount and restore persistent user files before calling the game's entrypoint.
Keep user data separate from versioned application downloads. Synchronize after
successful save/settings changes, serialize pending syncs and expose failures;
do not rely on page shutdown to finish asynchronous writes. Restrict concurrent
tabs from writing the same saves. Backup import must validate its format and
paths, confirm replacement and preserve prior data if restoration fails.

Request persistent browser storage and handle rejection, quota exhaustion and
cleared data. Remembering files applies to the same browser profile and origin;
it is not cross-device synchronization. Keep the deployment origin stable and
use a stable user-data namespace with explicit schema migrations. A service
worker may later cache application files, but it must not own save lifecycle.

The launcher should show source, version, commit and truthful build provenance
in normal About/footer chrome, following local identity and UI guidance. Handle
GitHub project-site base paths when resolving loader, Wasm and supporting files.

## Portability work to prove

| Area                     | Existing constraint                                                                                                           | Required proof                                                                                                                 |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| Address classification   | Display lists distinguish host pointers from N64 segment words using address ranges; native arenas use high virtual mappings. | A browser strategy handles static data, arenas and ROM buffers without assuming native address placement; measure memory cost. |
| Compiler and data layout | CMake uses x86/GCC flags, inline assembler symbols, asset layout conventions and native object tools.                         | Clang/Wasm builds preserve required sizes, offsets and asset segment boundaries; replace incompatible constructs.              |
| Scheduler                | Blocking message waits re-enter the pump beneath nested game loops.                                                           | Browser events, rendering and audio progress without changing simulation timing or corrupting suspended stacks.                |
| Renderer                 | Desktop GL loading and extension checks coexist with incomplete ES support.                                                   | WebGL 2 shaders, depth, framebuffers and representative effects render correctly; document any temporary feature restrictions. |
| Platform integration     | Paths, dialogs, crash handling, subprocess preparation and device access assume a native process.                             | Browser adapters or explicit feature gates avoid those native paths.                                                           |
| Suspend points           | Native snapshots require exact PE/ELF executable layout, build identity and memory addresses.                                 | A separate Wasm design before enabling the feature; no claim of native snapshot compatibility.                                 |

Primary starting points are [CMake](../../CMakeLists.txt),
[startup](../../port/src/main.c), [scheduler](../../port/src/pcsched.c),
[pointer classification](../../port/include/port_hostptr.h),
[ROM loader](../../port/src/romdata.c),
[platform shim and EEPROM](../../port/src/libultra_shim.c),
[video and render threading](../../port/src/video.c),
[GL backend](../../port/fast3d/gfx_opengl.cpp),
[SDL window backend](../../port/fast3d/gfx_sdl2.cpp),
[native icon preparation](../../port/fast3d/weapon_cache.cpp) and
[suspend documentation](save_states.md).

## Milestone completion criteria

1. **WEB-001 Portability spike:** pin/configure the toolchain, compile/link the
   game core, demonstrate pointer/segment classification and required layout
   contracts, and record remaining browser blockers and measured memory needs.
1. **WEB-002 Playable prototype:** supply a local ROM, reach the menu and play
   Dam with rendering, sound, keyboard and mouse. Demonstrate stable scheduling,
   focus-loss recovery and a recorded performance/browser baseline.
1. **WEB-003 Persistence:** campaign progress, profiles and settings survive a
   fresh launch. Backup export/import round-trips. Invalid imports, write errors,
   denied persistence and concurrent tabs cannot silently lose existing saves.
1. **WEB-004 Launcher:** first/repeat launch, ROM validation and remembering,
   loading stages, Play/Resume, fullscreen, storage controls and actionable error
   states work. No ROM leaves the browser.
1. **WEB-005 Pages deployment:** PRs build/test without publishing; successful
   pushes to `main` publish the assembled site. Production paths work and a
   deployment update preserves user data and does not interrupt an active game.
1. **WEB-006 Compatibility and polish:** exercise missions and renderer effects,
   test current Chrome/Edge, Firefox and Safari, document the supported matrix,
   tune performance and assess optional icons/offline support. Evaluate browser
   suspend points as a separately scoped design after the core target is proven.

Work through the technical gates in order. Storage or launcher groundwork can
overlap once its runtime interface is stable; it must not hide an unresolved
memory, rendering or scheduling blocker. Record explicitly deferred optional
features rather than treating them as required for initial release.

## Deployment and verification

Use a pinned Emscripten SDK and pinned Actions. Extend or add a workflow that
builds/tests PRs, and configures/uploads/deploys a Pages artifact only from
successful `main` runs. Grant deployment permissions only to the deployment
job. Reuse build provenance and license conventions. Keep ROMs out of CI,
artifacts, caches and logs; private local gameplay tests use the user's ROM.

Start without pthreads. Emscripten pthreads need cross-origin isolation headers;
GitHub Pages has no straightforward custom-header configuration. If performance
requires threads, evaluate a header-capable host/proxy or a documented workaround
before changing the accepted hosting design.

Add focused ROM-free checks for address decoding, layout, storage/backup formats,
launcher state transitions and deployment packaging. Browser automation can
exercise the launcher and synthetic files in CI. Separately record real-ROM
gameplay, audio/input and storage tests, with browser/version and build identity.
Run relevant existing native regressions and native builds after shared engine
or build changes. Rendering changes follow the arm contracts and gallery rules
in [AGENTS.md](../../AGENTS.md), including local visual review. Documentation-only
updates require no game rebuild or gallery regeneration.

## Session handoff

The first WEB-001 tooling slice is described in the
[portability spike runbook](web_target_spike.md). It builds the ROM-free probe
and surveys the real CMake source set; it is separate from the eventual browser
game target and does not satisfy the full WEB-001 completion gate.

Before work, read repository instructions, this plan and the relevant progress
blocks; inspect Git state and refresh remote refs without overwriting other
work. Continue the first unfinished technical gate. Resolve routine choices
within this accepted scope and record material design changes with reasons.

Before handoff, update only the relevant milestone block with status, changed
paths, commands/results, browser evidence, unresolved blockers and one concrete
next action. Include commit or branch references when they exist; otherwise
identify local uncommitted work. Do not mark completion from compilation alone
when the milestone requires gameplay or persistence evidence. Keep generated
ROM-derived evidence local and update the fork inventory with implementation.

## Technical references

- [Emscripten and CMake](https://emscripten.org/docs/compiling/Building-Projects.html)
- [Browser event loop](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [Asyncify](https://emscripten.org/docs/porting/asyncify.html)
- [OpenGL and WebGL](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html)
- [Persistent filesystem](https://emscripten.org/docs/api_reference/Filesystem-API.html)
- [Threading requirements](https://emscripten.org/docs/porting/pthreads.html)
- [Browser storage](https://developer.mozilla.org/en-US/docs/Web/API/Storage_API/Storage_quotas_and_eviction_criteria)
- [Pointer Lock](https://developer.mozilla.org/en-US/docs/Web/API/Pointer_Lock_API)
- [Audio startup](https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay)
- [Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
- [Pages isolation header discussion](https://github.com/orgs/community/discussions/13309)

These references informed the 2026-10-24 plan. Recheck current SDK and hosting
behavior when selecting implementation versions.
