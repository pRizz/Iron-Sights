# Browser build milestone progress

The user accepted [the browser build plan](web_target_plan.md) on 2026-10-24.
Implementation proceeds through its six technical gates. Each block owns its
status and evidence; update the relevant block and append dated session notes
inside it. Preserve earlier evidence and describe superseded decisions explicitly.
Append any new milestones at the end with a new stable ID. Do not add a shared
current-status table or rewrite unrelated blocks.

## WEB-001 | 2026-10-24 | Portability spike

**Status:** Not started.

**Completion:** Emscripten compiles/links the game core; pointer classification,
data layout and memory strategy have demonstrated solutions.

**Starting paths:** `CMakeLists.txt`, `port/src/pcsched.c`,
`port/include/port_hostptr.h`, `port/src/segments.c`,
`port/src/ucode_dl_stubs.c`, `port/src/main.c`.

**Evidence:** No web build or runtime proof yet.

**Open questions:** Compatibility of GCC/assembler/linker constructs with Wasm;
address decoding for static/arena/ROM data; initial and peak memory requirements;
function-pointer assumptions and runtime loop yielding.

**Next action:** Pin an Emscripten SDK, add an isolated browser CMake
configuration and capture the first compile/link diagnostics. Prove the memory
and address strategy before treating a successful link as a playable target.

**Session notes:**

- 2026-10-24: Accepted scope and handoff files created. Source inspection found
  native high-address allocation, nested blocking scheduler waits, incomplete
  ES support and PE/ELF-specific suspend metadata. These are investigation
  targets, not reproduced Wasm failures. Existing unrelated staged standards
  adoption was preserved; refreshed `origin/main` matched local HEAD `cdd6b5f`.

## WEB-002 | 2026-10-24 | Playable prototype

**Status:** Not started. Depends on WEB-001.

**Completion:** A supplied ROM reaches the menu and playable Dam with correct
rendering, sound, mouse and keyboard; focus loss recovers safely.

**Starting paths:** `src/boss.c`, `port/src/pcsched.c`, `port/src/romdata.c`,
`port/src/video.c`, `port/src/audio_pc.c`, `port/src/input_pc.c`,
`port/fast3d/gfx_opengl.cpp`, `port/fast3d/gfx_sdl2.cpp`.

**Evidence:** No browser gameplay test yet.

**Next action:** After WEB-001, wire local ROM injection and browser startup;
prove scheduler yielding and the WebGL 2 path with native threads disabled.
Record browser/version, build identity, memory and frame/audio behavior.

## WEB-003 | 2026-10-24 | Save persistence and backups

**Status:** Not started. Depends on WEB-002 for actual campaign validation.

**Completion:** Campaign saves, profiles and settings survive fresh launches;
backup export/import round-trips and failures preserve existing data.

**Starting paths:** `port/src/libultra_shim.c`, `port/src/main.c`,
`port/src/video.c`, browser storage adapter to be added.

**Evidence:** No browser persistence or backup test yet.

**Next action:** Define a stable user-data mount and backup schema, then restore
before startup and synchronize on writes. Verify errors, concurrent tabs and
restoration after reload. Keep exact-build suspend images disabled.

## WEB-004 | 2026-10-24 | Browser launcher

**Status:** Not started. Depends on the runtime/storage interfaces from WEB-002
and WEB-003; UI groundwork may overlap once those interfaces are stable.

**Completion:** First/repeat launch, remembered ROM, staged loading, Play/Resume,
fullscreen, storage controls and recoverable errors work without ROM uploads.

**Starting paths:** New SolidJS/TypeScript launcher; existing ROM validation and
build provenance interfaces.

**Evidence:** No launcher implementation or browser UX test yet.

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
