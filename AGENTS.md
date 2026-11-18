# Repository instructions

These instructions apply throughout this repository.

## Maintain the fork inventory

Whenever you add, modify, remove, or replace something in this repository,
update [FORK_CHANGES.md](FORK_CHANGES.md) as part of the same change. This applies
to gameplay, rendering, settings, fixes, documentation, assets and asset
generation, tests, tooling, build configuration, and repository rules.

- Read the relevant inventory entries before changing an existing feature.
- Update an existing entry when refining or fixing that feature. Add a new,
  stable `FORK-NNN` entry for a separate addition, using the next unused number.
- Describe the current behavior, why it differs from the inherited upstream
  baseline, relevant settings/defaults, main source paths, and validation.
- Include documentation-only and maintenance changes. Inventory bookkeeping
  itself can update its existing maintenance entry; it needs no recursive entry.
- Record removals and replacements, including what superseded an earlier
  implementation. Do not leave removed behavior described as current.
- Distinguish inherited upstream behavior from fork work. If an upstream merge
  changes ownership or makes an entry obsolete, explain that in the inventory.
  Change the comparison baseline only deliberately, with its old and new refs
  documented; do not silently reset it to the latest upstream commit.
- Keep entries factual. Do not invent commit IDs, claim unrun checks passed, or
  describe local work as released or pushed. A Git commit reference is useful
  evidence when available, but is not required before committing the change.
- Record implementation and workflow changes, rather than each local build,
  capture, or regeneration run. Keep ROM-derived images, packs, models and other
  generated game data out of Git, as required by CONTRIBUTING.md and .gitignore;
  document their generator and regeneration instructions instead. The user has
  explicitly authorized the rendered HTML/PNG reference gallery under
  `review/hand-gallery`; this exception excludes ROMs, extracted models/textures,
  raw capture files, executable copies and binary caches.

Before handing off work or preparing a commit/PR, compare the changes with the
inventory and ensure every changed area is covered. Updating the inventory is
part of completing the requested work, not a separate optional follow-up.

## First-person hand and arm invariants

Before changing hands, arms, wardrobe materials or their render paths, read
[the arm rendering contracts](scripts/port/arm_rendering.md). Preserve native
pistol hand vertices, UVs, colors, retained triangles and original material/render
setup unless the user explicitly requests a hand redesign. Fit the added forearm
to that protected hand. Rifle forearm bulk must not alter the grip hand, wrist
join or watch. Keep stock hand rendering separate from generated-arm shading.
Keep native pistol hands and added sleeves in the same weapon depth layer;
preserving hand data alone does not prevent skin from painting over a cuff.
Match generated wrist skin to the native weapon's UVs and vertex shade; change
only skin-owned attributes, preserving clothing/watch mapping and geometry.

Run the ROM-free wardrobe and ADS regressions for these changes. When the local
ROM is available, compare native pistols with the stock audit reference and
review intermediate ADS poses, both hands and affected wardrobes. Generated
reference images stay local unless published by the versioned review-gallery
generator. Deliberate contract changes must
update the guide, regression checks and fork inventory together.

## Render review gallery

Keep `review/hand-gallery` fresh for changes to rendering inputs. Use
`python scripts/port/hand_gallery.py generate`, review changed images, and stage
the gallery alongside its source changes. `verify` checks freshness and image
integrity without a ROM. Install the clone's pre-commit hook with `install-hook`;
the hook never stages files automatically and rejects partially staged render
inputs. See `scripts/port/hand_gallery.md` for the capture and hook workflow.

The inputs are in two tiers. A change to a strict-tier file (`STRICT_FILES` in
the tool: the hand, arm and wardrobe code, the game files the arm contracts
name, `port/fast3d/`, the weapon icon files, the capture path and the tool
itself) still needs a fresh, reviewed gallery. A change only to other rendering
inputs may leave the gallery pending. Pending is opt-in per clone through the
local Git setting `midas.galleryDefer` (`install-hook --defer` sets it); a clone
that can render leaves it off, so its hook regenerates on any staleness. CI
runs `verify --allow-pending`. A new file that draws hands, arms or wardrobe
must be added to `STRICT_FILES` in the same change.

A clone with `midas.galleryDefer` set may also commit a strict-tier change on a
branch other than `main` (owner decision, 2026-11-16): its hook still checks
the staged gallery's images and HTML, then passes and prints that the gallery
is stale on that branch. On `main`, on a detached HEAD, or without the setting,
the hook is unchanged, and `verify` and CI still fail on a strict mismatch, so
such a branch reaches `main` only through a regeneration on a machine that can
render.

## Render performance

A change that adds or alters something drawn or computed every frame gets a
smoke benchmark and a profile before it is committed: a render pass or
post-process, a shader, first-person or HUD drawing, an effect, a per-frame
mesh or texture build, or renderer state handling. The weapon glint looked
free and cost more than half of a browser frame (FORK-087); nobody could have
known without measuring.

- Build the commit from before the change (`stock.py --ref <commit> --name
  before`) and run `bench.py run --smoke` with both builds, on a quiet machine.
  Take one profiled run of each (`--env GE_PROFILE=profile.txt --keep`) and
  read `bench_profile.py`'s differences table. For a change the browser build
  compiles, also check its Render ms with the browser runner.
- Put the before and after figures in the change's `FORK_CHANGES.md` entry. A
  cost beyond the runs' own range is either fixed or stated there as the
  feature's price, with the setting that turns it off.
- Never ask the GL driver a question in a per-frame path: `glGet*`,
  `glIsEnabled`, `glGetError`, `glReadPixels`, `glCheckFramebufferStatus`,
  `glGetUniformLocation` and their like make the driver, and in a browser the
  GPU process, finish everything first. Cache locations at link time. The
  state shadow (`port/fast3d/gfx_gl_shadow.inl`, FORK-088) answers the state
  queries it tracks from memory; a pass that needs another piece of state adds
  it there, and a run with `GE_GLSHADOW_CHECK=1` must report no mismatch.
- Documentation, tooling and gameplay-only changes need none of this. See
  [the benchmark guide](scripts/port/benchmarks.md#smoke-check-for-a-render-change).

## Load before play

A slightly longer load is better than a stall during play. Work done the first
time something is seen (reading or decoding a file, building or uploading a
texture, compiling a shader, making GL objects, anything that waits for the
driver) belongs to the stage load or the boot. Runway's shutter decoded 60 HD
textures in one 350 ms frame because the pack was read on first draw
(FORK-090); a per-frame benchmark cannot see that.

- Do new first-use work while loading: prefetch it in the stage load
  (`port/fast3d/hd_texture_prefetch.cpp`, the shader prewarm, the warm-up
  frames in `port/src/port_late_loads.c`), or record it on first use so later
  runs load it ahead (`tools/shaders_base.txt`,
  `port/fast3d/texture_packs/hd_stage_prefetch.json`). When neither can work,
  do it in the background and show the old result until it is ready. Never
  stall a frame of play for it.
- Run `python scripts/port/late_load_audit.py` (the stages a change affects,
  or all of them) for a change that adds an asset, texture, shader, pass, file
  read or GL object, or changes when one loads. Compare its report with one
  from the build before the change (`--env GE_LOAD_AHEAD=0` audits the old
  first-use behaviour of one executable). A new late load, or a costlier one,
  is fixed or recorded in the change's `FORK_CHANGES.md` entry with its reason.
- `ge: late load:` lines in a game log name work done during play. Treat a new
  one as a regression.
- Leaving work on first use needs a reason in the inventory: memory the 32-bit
  Windows build cannot spare, or behaviour the original game has on purpose
  (the CLASSIC Load Hitches row). See [the guide](scripts/port/late_loads.md).

## Iron Sights settings

Settings this fork adds live on the Iron Sights page, not on upstream's pages.
In both the Options folder (`src/game/front.c`) and the watch
(`port/src/port_watchpages.c`), Gameplay > Iron Sights opens the subsections
Aiming and Movement, Combat Feedback, Enemies, HUD Additions, System and Graphics. Put a new fork
setting in the subsection that fits its topic in both menus, or add a
subsection when none fits. Fork-owned rows and actions (About / Build Info,
Suspend Points) belong there too.

- A fork change to an upstream setting leaves that setting where upstream put
  it; describe the fork behavior in its help text and the inventory.
- Keep midas.ini key names and sections stable when moving rows, so existing
  configurations keep working.
- A key that is deliberately ini-only, or in only one menu, needs an entry with
  its reason in `scripts/port/settings_placement.py`.
- `python scripts/port/settings_placement.py` enforces this against
  `scripts/port/settings_upstream_keys.txt`, the option keys at the inherited
  baseline; the pre-commit hook runs it. Refresh that list only when
  `FORK_CHANGES.md` deliberately advances the baseline.
- Update help lines (`port/src/port_option_help.c`), `tools/midas_ini_help.txt`
  and any docs or audits that name the setting's menu path.

## Input idioms

Every interactive screen must work with each supported input: keyboard, mouse,
gamepad and browser touch. When adding or changing a menu row, list, prompt,
capture or watch page, check how each input confirms, cancels or backs out,
adjusts a value, scrolls, and unbinds or deletes. No screen may need an input
that another supported one cannot send, and none may wait on one without a way
out.

- In menus, browser touch sends only cursor motion and A on a tap; the watch
  adds one-step arrows and Select/Back. Anything else needs a native touch
  context (`port/web/touch.c`, `portWebTouchContext`) with an overlay control in
  `web/src/touch-controls.tsx`, and coverage in the touch fixture and audits.
- Prompts and help text name the control the current input actually has.
- Record any input deliberately left unsupported, and why, in `FORK_CHANGES.md`.

## Local build artifacts

Do not keep previous executables as rebuild backups. The user accepts losing
save-state compatibility when the executable changes. Obsolete local game
executables and suspend images associated with those old builds may be purged
without another confirmation; there is no need to archive them first.

Keep the current main executable. Match suspend images to obsolete executable
build fingerprints before purging them; leave unknown or current-build images
alone. This permission concerns build artifacts and suspend images, not normal
campaign saves (`Data/Saves`), profiles, settings, source files or other user data.
Audit executables are temporary: remove the copied executable when the audit
finishes. The audit's logs and captures follow the cleanup rule below.

Clean up temporary generated files before pushing. Before each push, delete
what the task generated outside Git and no longer needs: its audit, capture
and repro folders under `build_win` (logs, screenshots, frame dumps, copied
executables), hand-gallery capture folders (`build_win/hand-gallery-*`),
temporary isolated build or runtime directories, and scratch scripts. Delete
them outright: the Recycle Bin shares the drive and frees nothing. Keep the
main executable and the files beside it (DLLs, `.map`, link outputs,
`shiny_weapons.ini`), `build_win/Data`, `midas.ini`, saves and profiles, the
CMake build trees (`build_win`, `build_win/hd-prototype`), Sentry inputs
(`build_win/sentry-*`), and files the owner placed there (recordings,
screenshots). Leave alone what another session or running process may still
use. A finding a `FORK_CHANGES.md` entry relies on belongs in that entry's
text; if an entry names a capture folder, update the entry when the folder
is removed.

The user may play/debug the main Windows executable during development. Before
building or replacing an executable, check running game processes and their
resolved executable paths. Use a separate CMake binary directory and isolated
runtime directory when a build or audit could collide with a running instance
(for example `build_win/hd-prototype`). Keep the main executable, settings,
saves, logs and crash output untouched. Do not stop a user's game to make a
build succeed. Audit only task-owned processes and remove only their temporary
executable copies after they exit. This also applies to gallery regeneration;
pass its isolated executable rather than rebuilding an active player binary.

## Repo-Local Guidance

- Minimize changes in upstream-owned files. Put new fork behavior in focused
  files under existing fork/platform directories, connected through small,
  explicit hooks. Preserve upstream layout and names; avoid unrelated formatting
  and broad refactors. Extract existing fork code when the boundary is clear and
  verification can establish equivalent behavior. Do not force separation that
  adds needless indirection or weakens rendering/ABI contracts. Document shared
  hooks and overlapping upstream behavior in `FORK_CHANGES.md` for later merges.
- Default delivery is direct to `main`: sync remote state, complete the requested
  work, run the relevant checks, then commit and push to `main`. Do not create a
  feature branch or pull request by default. Use a PR only when the user requests
  one or an external requirement makes it necessary, such as an upstream
  contribution or branch protection. Preserve unrelated work and never bypass
  protections or force-push to satisfy this preference.
- Web target work follows the accepted [plan](scripts/port/web_target_plan.md) and
  [milestone progress](scripts/port/web_target_progress.md). Read both before
  resuming; update only the relevant stable milestone block with changes,
  verification evidence, blockers and the next action before handing off.
- The native 64-bit port (and the macOS and Apple TV builds that depend on it)
  follows the accepted [64-bit plan](scripts/port/native_64bit_plan.md). Read it
  before resuming, keep the 32-bit and web builds working, do not reopen its
  recorded decisions, and update its milestone status table before handing off.
- ROM data converters (LP64-004) are `port/src/romconv_*.c`, declared in
  `port/include/port_romconv.h`, `port_romconv_audio.h` and
  `port_romconv_stage.h`, with the file layouts they read in
  `port_romlayout.h`, `port_romlayout_audio.h` and `port_romlayout_stage.h`.
  CMake compiles that pattern only at 64 bits and in the 32-bit headless
  oracle build (`-DGE_LP64_ORACLE=ON`), never in a shipped 32-bit build, and
  with `-Werror=incompatible-pointer-types`. Call one from shared code only
  inside `#if IS_64_BIT` or `#ifdef GE_LP64_ORACLE`, leaving the existing code
  unedited in the other branch. The oracle build compares each converter with
  the in-place conversion it replaces; with the ROM, in the i686 container,
  configure `build/lp64/oracle` with `-DGE_HEADLESS=ON -DGE_LP64_ORACLE=ON`,
  build it, and run `python3 scripts/port/lp64_oracle.py --rom
  /rom/baserom.u.z64 --classes "animation tables,text banks,audio
  banks,music sequences,textures,fonts,models,backgrounds,collision
  tiles,setup files" --frames 1800 --fixed-tick
  build/lp64/oracle/IronSights`; it passes when each class reports `N files,
  0 differ` with N above zero. `scripts/port/lp64_census.py compare DIR32
  DIR64` compares what the missions load at both widths, from the folders of
  a 32-bit and a 64-bit `audit_buffers.py --mission-smoke ... --frames 1800
  --fixed-tick` run. `scripts/port/lp64_boot.py --expect-run [--frames N]` is
  the normal boot check: it passes a 64-bit executable that runs N frames
  (600 by default), logs `ge: frame limit reached: frames=N` and exits 0;
  `--expect-stop NAME` remains for the test hook below. A 64-bit run, in the
  boot check or the mission smoke, fails on any `ge: lp64:` line outside the
  informational list (`INFORMATIONAL_PREFIXES` in `lp64_boot.py`); a new
  informational line is added there deliberately.
- `scripts/port/lp64_play.py` (LP64-005) runs named sets of 64-bit game runs
  and judges each by what its log shows: `python3 scripts/port/lp64_play.py
  run --rom /rom/baserom.u.z64 --set demos,stages,stops,menus,tours,attract
  --output build/lp64/play/NAME EXECUTABLE`, in the container of the
  executable's architecture. A case passes only with the exit status it
  expects, no sanitizer report, fatal signal, `ASSERTION FAULT` or failing
  `ge: lp64:` line, and every line that proves its subject ran (a demo's
  start and its exit at the measured tick, a stage's launch and background,
  a tour's rooms); the cases and their measured figures are in
  `lp64_play_cases.py`. `traces --expect scripts/port/lp64_demo_traces.txt
  DIR_REF DIR_NEW` compares each demo's per-tick trace with the i686 build's,
  whose two runs must first agree (`traces --reference-check`); a fix that
  makes a demo match lowers its line in that file. Runs are paced in real
  time, 60 retraces a second, so the sets take their frames' time. Never run
  them with `GE_FIXEDTICK` (a demo then advances one field a tick instead of
  at its recorded pace and ends at another tick) or `GE_NOSLEEP` (the frame
  limit, which counts retraces, then passes before the game has done
  anything); the tool refuses both from `--env`.
- Storage for a record or a display list is sized by `sizeof` of what it
  holds, never by a literal byte count or by filler members, and a member
  that overlays another record by position is pinned at both widths in the
  layout probe (`scripts/port/abi_probe/probe.c`). Records and commands that
  hold pointers grow at 64 bits, and the sanitizers do not see an overrun
  inside one allocation or a common symbol. `g_ModelHitEntries`
  (`src/game/objecthandler.c`) was 600 times 0x14 bytes for a 40-byte
  record, and the pause list `buffer_for_watch_static_DL`
  (`src/game/bondview.h`) was one `Gfx` and six `s32` fillers for three
  commands.
- Every place the C sources read a record through another type has one
  reviewed line in `scripts/port/lp64_overlays_reviewed.txt`:
  `python3 scripts/port/lp64_overlays.py list` finds six kinds (`index`,
  `offset`, `opaque`, `record`, `run`, `stored`; `--help` says what each
  covers and what the tool cannot see), and `check` fails on a site without a
  line or a line without a site. A line's verdict is `typed`, `same`,
  `overlay-64`, `dead` or `defect`, with its reason. A `same` names, by its
  message, a layout assertion in the probe (`scripts/port/abi_probe/`) that
  was seen failing at both widths when the bytes it pins drift. A reason that
  rests on what the stock ROM holds says `measured:` and gives the scanner
  command that measured it (`lp64_model_scan.py`, `lp64_setup_scan.py`);
  a scratch reader's figure is not a measurement. A type that a 64-bit build
  reaches only through pointers (`python3 scripts/port/lp64_pointer_types.py
  scan` on a configured 64-bit tree lists them) is judged over its real
  record and every member read through it anywhere, not only at the line
  that casts to it (R11, FORK-089's table). A `defect` line is temporary: its
  reason starts with its owner (`Task N:`, `strict:` or `undecided:`), and
  the fix takes the site off the list or changes its verdict in the same
  change. `check` runs in CI's tests step; run it before pushing a change to
  C or C++.
- At `GE_POINTER_BITS=64`, C and C++ that mix pointers with 32-bit integers do
  not compile: `CMakeLists.txt` gives 64-bit C `-Werror=int-conversion
  -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast` and 64-bit C++
  `-Werror=int-to-pointer-cast` (C++ rejects the other two by itself) instead
  of `-w`, and switches off by name each other warning class the 64-bit build
  printed that has a `-W` option. The C log is clean; the C++ log keeps
  eleven warnings that have no option, in `port/fast3d` (the comment beside
  the flags lists the classes, with counts). CI's tests job
  configures a 64-bit headless tree (`build/lp64/gate`) and a 64-bit video
  tree (`build/lp64/gate-video`), builds only their `ge_buildinfo` target, and
  runs `python3 scripts/port/lp64_diagnostics.py gate build/lp64/gate
  build/lp64/gate-video`, which refuses an entry lacking those flags or
  carrying `-w`, `-Wno-error`, a `-Wno-` for a gated kind or a response file,
  then compiles every C and C++ file of both trees with its own flags and fails
  naming each file that does not compile. Two more checks follow it on the same
  trees: `lp64_diagnostics.py narrowing --check` (implicit narrowing of a
  `uintptr_t`, `intptr_t` or `romptr_t`) and `unprototyped --check` (calls
  without a prototype to functions that take or return a pointer or 64-bit
  value). Each compares its places with an exact-match allowlist, and a
  missing file allows none (`scripts/port/lp64_static_checks.py`): no
  narrowing place is allowed, so `lp64_narrowing_allowed.txt` exists only if
  one is deliberately allowed, with its reason; `lp64_unprototyped_allowed.txt`
  lists the remaining calls with theirs. Allowlists are lowered as places are
  fixed and never raised without a reason. Run the
  same lines in the x86_64 or aarch64 container (GCC 13) before pushing a
  change to C, C++ or the build; `lp64_diagnostics.py sites [--messages]
  [FILE...]` lists the conversions of those trees as `file:line: kind`. A new
  warning class in the 64-bit log gets fixed, or a named `-Wno-` flag with its
  count in that comment; never `-w`.
- All ten ROM asset classes are converted at 64 bits: each entry of
  `port/src/port_lp64.c`'s table is 1. Each in-place conversion still starts
  with `portLp64RequireConverter` (`port/include/port_lp64.h`), so a 64-bit
  game stops with exit status 5, naming the class, only when an entry is set
  back to 0 or when a test driver is built with `GE_LP64_TEST_UNCONVERTED`
  naming a class (`test_lp64_address.sh` does, so that the stop and
  `lp64_boot.py --expect-stop "NAME"` stay tested); CMake never defines it.
  A 64-bit build has no compiled-in asset copy to fall back to, and the
  `GE_ROM*=0` switches are ignored there: it stops by name, with exit status
  1, on a file of one of these classes that the ROM does not serve and on a
  missing ROM segment. A converter that refuses a file logs `ge: lp64:
  cannot convert CLASS "FILE": ...`; the game then stops by name, except that
  a refused text bank is zeroed and the game carries on, and a refused model
  loads with empty tables (or stops by name if even those do not fit its
  room). A pool whose overrun is fatal
  (the music heap, the graphics buffer) stops it with exit status 6.
- Fonts, sharp text rendering and other original game assets follow the
  accepted [open assets plan](scripts/port/open_assets_plan.md). Read it before
  resuming, keep its provenance rules (drawn from scratch, MIT, no ROM-derived
  data in the assets), and update its milestone status table before handing off.
- Application icons (the Windows executable, window, Linux and Steam Deck menu,
  browser favicon, Home Screen and web app manifest), the link-preview image
  and the Steam library art all come from
  `python scripts/port/make_brand_art.py`. Change the design there, rerun it,
  and commit its outputs together with `scripts/port/brand-art.sha256`;
  `test_steam_art.py` fails if an output no longer matches the generator. A new
  place that shows an icon (a web app manifest, a Flatpak, a macOS `.icns`)
  gets a new output in the generator, never a separately drawn image.
- Text styles (Original, Enhanced, Iron Sights) follow the
  [font styles plan](scripts/port/font_styles_plan.md). Never bundle, commit or
  host the Enhanced pack's files; the game downloads them only after the
  player consents. Update its milestone status table before handing off.
- The immersive browser room follows its accepted
  [experience plan](scripts/port/web_room_plan.md) and WEB-007 through WEB-010 in
  the same progress file. Keep the game canvas/storage lifecycle intact; record
  model licenses and distinguish procedural stand-ins from approved final props.
- This is a C/C++ CMake game port. Bun runs development standards checks and the
  browser tooling subpackage under `web/`. Use `bun install --frozen-lockfile`
  inside `web/` for its tests/audit tooling; the repository root has no Bun package.
- `.githooks/pre-commit` runs `bun scripts/bright-builds-check.ts all`, the settings placement check, then staged gallery verification. Manual runs for installation, debugging or review are allowed; a commit hook rerun needs no separate confirmation.
- Source changes use the relevant `scripts/port/test_*.sh` checks. Wardrobe/render changes require the arm contracts and gallery workflow above. Documentation/rules-only changes need no game rebuild or render regeneration.
- Sound volume is not linear in loudness: the mixer squares each voice's volume (`src/libultrare/audio/env.c`), so two partial voices do not add up to one full voice. Verify audible changes by recording them, not by arithmetic: `GE_AUDIO_CAPTURE=<path.wav>` records the mixed output, and `scripts/port/audit_hit_sound.ps1` is a worked capture-and-measure audit.
- Performance work is measured, not estimated: `python scripts/port/bench/bench.py run` times fixed work across builds, including stock 1.0.2b from `scripts/port/bench/stock.py`. Build the commit from before a change with `stock.py --ref <commit> --name before`, measure before and after together in one session with a visible window on a quiet machine, and treat a difference inside the runs' own range as noise. See [the benchmark guide](scripts/port/benchmarks.md).
- Existing long files have a documented adoption baseline in `.bright-builds-rules-checks.tsv`; new source files remain checked. See `standards-overrides.md` before changing an exempt file.
- When a PR is needed, use the managed PR template together with [the preserved project checklist](scripts/port/project_pr_checklist.md), `CONTRIBUTING.md` and the fork inventory requirement.
- Standards maintenance and Windows setup: [Bright Builds workflow](scripts/port/bright_builds_rules.md). Managed file updates are reviewed manually and update `FORK_CHANGES.md`; unattended updates are disabled.

<!-- bright-builds-rules-managed:begin -->

# Bright Builds Rules

`AGENTS.md` is the entrypoint for repo-local instructions, not the complete Bright Builds Rules specification.

This managed block is owned upstream by `bright-builds-rules`. If this block needs a fix, open an upstream PR or issue instead of editing the managed text in a downstream repo. Keep downstream-specific instructions outside this managed block.

Before plan, review, implementation, or audit work:

1. Read the repo-local instructions in `AGENTS.md`, including any `## Repo-Local Guidance` section and any instructions outside this managed block.
1. Read `AGENTS.bright-builds.md`.
1. Read `standards-overrides.md` when present.
1. Read the local managed standards pages under `standards/` relevant to the task.
1. If you have not done that yet, stop and load those sources before continuing.

Use this routing map when deciding what to load next:

- For repo-specific commands, prerequisites, generated-file ownership, CI-only suites, or recurring workflow facts, use the local `AGENTS.md`, especially `## Repo-Local Guidance`.
- For the Bright Builds default workflow and high-signal cross-cutting rules used in most tasks, use `AGENTS.bright-builds.md`.
- For deliberate repo-specific exceptions to the Bright Builds defaults, use `standards-overrides.md`.
- To choose the right managed standards page, start with the local Bright Builds entrypoint `standards/index.md`.
- For business-logic structure, domain modeling, and functional-core versus imperative-shell decisions, use the managed standards page `standards/core/architecture.md`.
- For control flow, naming, function/file size, and readability rules, use the managed standards page `standards/core/code-shape.md`.
- For frontend visual defaults, theme defaults, and dark-mode decisions, use the managed standards page `standards/core/frontend-ui.md`.
- For sync, bootstrap, and pre-commit verification rules, use the managed standards page `standards/core/verification.md`.
- For the managed starter checks, run `bun scripts/bright-builds-check.ts all`; use its `--help` output for check-specific commands, exact-file exceptions, and trailing-slash `file-lengths` directory exceptions.
- For unit-test expectations, use the managed standards page `standards/core/testing.md`.
- For Rust or TypeScript/JavaScript-specific rules, use the matching managed standards page under `standards/languages/`.
- For TypeScript/JavaScript frontend framework and UI-library defaults, use `standards/languages/typescript-javascript.md`.
- Keep recurring repo-specific workflow facts, commands, and links in a `## Repo-Local Guidance` section elsewhere in this file.
- Record deliberate repo-specific exceptions and override decisions in `standards-overrides.md`.
- If instructions elsewhere in `AGENTS.md` conflict with `AGENTS.bright-builds.md`, follow the repo-local instructions and treat them as an explicit local exception.

<!-- bright-builds-rules-managed:end -->
