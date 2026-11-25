# LP64-007 Shipping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-24 from measurements on `main` at f04e0a7,
and accepted by the owner the same day with all thirteen recommendations
("Accept all thirteen"); "Decisions for the owner" at the end lists them,
and the tasks are written for them. Execution started on 2026-11-24. The status table of
[native_64bit_plan.md](native_64bit_plan.md) records what is built and
verified as the tasks land.

**Goal:** Players can download a 64-bit Linux game: an x86_64 tarball,
AppImage and Steam Deck ZIP, built and attested by CI from the same commit
as the Windows and i686 packages, with working crash reports, an honest
notice about suspend points, and documents that say what each package is.

**Architecture:** The 64-bit packages are a second pass through the
release path the i686 packages already use, not a new path. The build
script and the packager each gain an architecture table whose `i686` row
reproduces today's commands exactly; a second pinned Ubuntu 22.04 image
holds the x86_64 toolchain, so the i686 image and its packages do not
change. CI builds and tests the new packages as artifacts from the first
task that can make them, and they join the published release only in the
last shipping task, after the release compiler's build has passed the
checks the development builds passed and after one session on real
graphics hardware. Every push to `main` publishes a release, so that
order is the plan's safety rule.

**Tech Stack:** C and C++ (GCC 11.4 on Ubuntu 22.04 for packages, GCC
13.3 on Ubuntu 24.04 for the development trees), CMake, Python 3 standard
library, Bash test harnesses, Docker Desktop on Apple Silicon (x86_64
through Rosetta 2), SDL 2.30.11 built from its pinned source, sentry-native
0.17.4, GitHub Actions.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md): milestone LP64-007
("Linux x86_64 packages beside the i686 ones, a native Steam Deck package,
Sentry and crash reports, CI jobs, player and developer documentation, the
suspend-point notice"), "Why", "Goals", "Anti-goals", "Constraints",
"Suspend points", "Crash reports and the rest of the port layer",
"Working machine" and "Decisions". Read it first, then `AGENTS.md`,
[releases.md](releases.md), [crash_reporting.md](crash_reporting.md),
[save_states.md](save_states.md) ("64-bit builds"), and in
`FORK_CHANGES.md` (FORK-089) the passage "LP64-006 hand-over to LP64-007".
Search that file; its lines are very long.

## What was measured for this plan (2026-11-24, `main` at f04e0a7)

Hosts, named as they were: x86_64 in the project's containers on the Mac,
translated by Rosetta 2; video through SDL's offscreen driver and Mesa's
llvmpipe. Nothing here ran on a GPU or on real x86 hardware.

### How the Linux packages are made today

- One CI job, `linux` (`.github/workflows/windows-release.yml:301-375`),
  on every push to `main`: it builds `scripts/port/linux-release.Dockerfile`
  (Ubuntu 22.04 pinned by digest, GCC 11.4, glibc 2.35, i386 `-dev`
  packages only), runs `linux_build.py` (SDL 2.30.11 from pinned source
  with `-m32`, sentry-native with `SENTRY_BUILD_FORCE32=ON`, then the game
  as `-DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DGE_ROMDATA_FALLBACK=OFF`),
  uploads debug files to Sentry, runs `package_linux.py` and then
  `test_sentry_linux.py` (ten crash-report cases on the real packages).
- `package_linux.py` writes `Iron-Sights-linux-i686.tar.gz`,
  `Iron-Sights-linux-i686.AppImage` and `Iron-Sights-steamdeck-i686.zip`
  from one AppDir. The Deck ZIP is the same game with `run.sh`,
  `STEAM-DECK.txt`, `set-steam-art.py` and four artwork files.
- The `publish` job (`:398-467`) needs `tests`, `windows`, `sentry-smoke`
  and `linux`; it attests five named files and creates a release, marked
  latest, with eleven named assets. **Every push to `main` publishes.**
- The shipped build type is `Debug`: `CMakeLists.txt` adds `-g -O2` for
  every build type, so `Debug` means optimised with assertions and
  symbols. Every 64-bit tree of LP64-001 to LP64-006 was built that way.
  There is no `Release` build of the game and this plan adds none.
- Neither script can be run for i686 on the Mac: `package_linux.py` calls
  `ldd` on 32-bit files, which Rosetta 2 cannot execute. The i686 path is
  checked by CI.

### i686 assumptions the 64-bit packages trip over

- `linux_build.py`: `-m32` for SDL (`:39`), `lib/i386-linux-gnu` (`:40`,
  `:78`), `SENTRY_BUILD_FORCE32=ON` (`:55`), `architecture="i686"`
  (`:95`); no `GE_POINTER_BITS`.
- `package_linux.py`: the AppImage `runtime-i686` pin (`:16-17`),
  `/usr/lib/i386-linux-gnu/` for libcurl, libasound and libpulse
  (`:33-34`), "Expected an ELF32 runtime" (`:46-47`), every output name,
  `architecture="i686"` and the Dockerfile hash (`:127-128`).
- `test_sentry_linux.py`: ELF class 1 and machine 3 (`:57`), the `i686`
  prefixes, and the event tag `architecture == "i686"` (`:143`).
- `port/crash/reporting_linux.c:163` tags every report `"i686"`.
- `scripts/port/linux/set-steam-art.py:26` writes one menu entry,
  `iron-sights-steamdeck.desktop`, whichever ZIP it came from.
- The workflow's attest list, asset list, checksum step and release notes
  name the i686 files.

### A release-style 64-bit build, tried for the first time

In the x86_64 development container (Ubuntu 24.04, GCC 13.3), configured
as the shipped build is plus `-DGE_POINTER_BITS=64`, that is `-DCMAKE_BUILD_TYPE=Debug
-DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DGE_ROMDATA_FALLBACK=OFF` (a scratch
tree, deleted after planning):

- Configure 2.5 s, build 32 s (391 objects), the same eleven optionless
  C++ warnings as every 64-bit tree and no new class. Its compile flags
  equal those of CI's `build/lp64/gate-video` on all 389 shared files.
- ELF 64-bit PIE (the 32-bit build links `-no-pie`), 28.3 MB with debug
  information, RUNPATH `$ORIGIN/lib`. Through the container's own SDL
  2.30.0 it pulls in 49 libraries. It needs **GLIBC_2.38** and
  **GLIBCXX_3.4.32**, above the packages' stated baseline of glibc 2.35.
- `lp64_boot.py --expect-run --frames 600`: `OK: ran 600 frames`. Run
  directly with a video case's environment: exit 0, `frames=600
  gfxtasks=595`, every `ge: lp64:` line informational, every pool under
  half.
- `lp64_play.py run --video --set demos`: `OK: 14 of 14 cases` in 225 s.
- `package_linux.py` on that tree: `FileNotFoundError: .../sentry-crash`.
  The tree has none of what `linux_build.py` makes (`sentry-crash`,
  `lib/libsentry.so`, the pinned SDL, the licences, `symbols/`), and the
  development image has neither `patchelf` nor `mksquashfs`.
- Sentry was off (`MIDAS_SENTRY_INCLUDE_DIR` empty); the log said
  `ge: crash reporting preference=ON active=0`, and the Crash Reporting
  row would read ON while nothing can be sent.

### The release compiler

Nobody had compiled the 64-bit game with the packages' compiler. In the
unmodified release image (`midas-linux-release`: GCC 11.4.0, CMake
3.22.1, glibc 2.35, Python 3.10), a 64-bit headless tree (`-DGE_HEADLESS=ON`;
a scratch tree, deleted after planning):

- Configured without a warning and built all 360 C objects in 19 s with
  0 errors and 0 warnings; GCC 11 knows every flag, the three
  `-Werror=` gates included. The executable needs `GLIBC_2.34` at most.
- `lp64_boot.py --expect-run --frames 600`: `OK: ran 600 frames`.
- `lp64_play.py run --set demos`: `OK: 14 of 14 cases` in 101 s.
- **Not compiled there:** the video sources. The image has only i386
  `-dev` libraries, so a video tree cannot be configured in it, and the
  C++ renderer (24 files, `-std=gnu++20`, which GCC 11 accepts) has never
  met GCC 11's compiler or its libstdc++ at 64 bits. The i686 packages
  compile the same files with it at 32 bits. Task 1 is where that is
  found out.

### What a 64-bit build does differently for a player

- **Suspend points.** 64-bit builds link `port/src/port_state_unavailable.c`.
  The watch row `Suspend Points` (`port/src/port_watchpages.c:153`), the
  four bind rows (`:575-581`) and the keys F5, F6, F8 and F9 are all
  present and say nothing until used; using one prints "Suspend points
  are unavailable in 64-bit builds." `tools/midas_ini_help.txt`, the
  README (`:98`, `:336-358`) and `scripts/port/linux/START-HERE.txt`
  describe suspend points as working. The Options folder has no such row.
  The browser build has the same silence.
- **Stops.** A 64-bit build can stop where a 32-bit one does not: status
  4 (an address below 4 GB), 5 (an unconverted class, test builds only),
  6 (the graphics buffer or music heap exhausted) and `sysFatalError`'s
  64-bit messages (a file or segment the ROM does not serve). All of them
  write to the log and exit. A player who started the game from a
  desktop sees the window close.
- **Crash handling.** The Linux signal handler (`port/src/main.c:536-545`,
  glibc `backtrace`) is the same at both widths; the x86 frame walks the
  design wanted replaced are Windows-only code, and Windows x64 is out of
  scope. Nothing needs replacing.
- **The play harness.** `port/include/port_playlog.h` sets
  `PORT_PLAY_HARNESS` to 1 in every 64-bit build, video included, with no
  CMake switch: `GE_PLAYLOG`, `GE_PLAYKILL`, `GE_PLAYSTALL`, `GE_PLAYHURT`,
  `GE_LOCKSTEP`, `GE_SIMTRACE` and `GE_MENU=briefing`. With none set its
  cost is three cached-flag tests a tick and two log lines at exit.
- **Not different:** external and HD textures, the icon cache, the font
  and pack downloads and save transfer have no width conditional. None
  has run at 64 bits with a pack: the comparison's profile left
  `texture_pack` and `crash_reporting` out.
- **The icon cache is shared.** Its file is named by the ROM's
  fingerprint alone (`port/fast3d/weapon_cache.cpp:404`), in SDL's
  per-user folder, so an i686 and an x86_64 install read the same file.
  Whether each accepts the other's has not been tried.

### Tools that cannot yet judge a Linux package

- `bench.py`, `stock.py` and the profilers are Windows tools: the default
  executable, the quiet-machine guard, the CPU probe, the copied
  `SDL2.dll` and the sampler (`port_profile.c`) are all `_WIN32`.
- `late_load_audit.py` runs in the containers (LP64-006 used it) and
  compares two audits by kind and name with `--compare`.
- No device has run any Linux package: `STEAM-DECK.txt` says "A real
  Steam Deck has not yet been used to validate this package", and every
  Linux video run on record drew with a software renderer.

### What could not be measured

A GPU; a Steam Deck; the x86_64 Sentry runtime (it does not exist yet);
whether the x86_64 AppImage runtime of release `20251108` has the same
licence file (Task 3 pins both); the i686 packaging on the Mac.

## The hand-over, item by item

| Hand-over item (FORK-089, "LP64-006 hand-over to LP64-007") | Here |
|---|---|
| The owner's eight open questions | Decision 12; Task 12 applies the ones answered yes, after the packages are public |
| Whether a shipped 64-bit package keeps the play harness | Decision 4; Task 3 records it in the package |
| A 64-bit run on a GPU; the frame-end check's cost | Decisions 5 and 10; Tasks 9 and 10 |
| Hardening against crafted files; fix wave C's two damaged-ROM paths | Decision 9; Task 7 |
| The suspend-point notice | Decision 7; Task 5 |
| 32-bit pixels for the 16 unsampled cases | Task 10, optional step |
| `lp64_env.py build-images` and `os.getuid` on Windows; the `goldeneye` image's missing 64-bit SDL | Task 1 |
| `compare` reads `record['video']` unguarded | Task 8 Step 1 |
| A pool raised past half is checked against every stream | Global Constraints |
| Budgets at exactly half on another host | Task 10's rule |
| The demos' headless-against-video difference; a demo's sound; routes that do not turn; `portPlaySleepMs` on Windows; the aarch64 gate on macOS clang; `FTOFIX32`; part-cache forgetting; the renderer's trigonometry; `lp64_lto.py` on clang; the "Kept deferred" list | Not in this plan; carried to the close's hand-over (Task 13) |

## File map

Created:

- `scripts/port/linux-release-x86_64.Dockerfile`: the x86_64 release image.
- `scripts/port/linux/START-HERE-x86_64.txt`, `STEAM-DECK-x86_64.txt`:
  the 64-bit packages' instructions.
- `scripts/port/test_linux_build.py`: the build script's argument lists.
- `port/include/port_build_arch.h`: the build's architecture name.
- `port/src/port_lp64_notice.c`: the message a 64-bit stop shows.
- `scripts/port/test_build_arch.sh` and `.c`, `test_lp64_notice.sh` and
  `.c`: ROM-free harnesses at both widths.
- `scripts/port/linux_64bit.md`: the developer's guide to the 64-bit
  Linux build and package.

Modified: `scripts/port/linux_build.py`, `package_linux.py`,
`test_sentry_linux.py`, `test_distribution.py`, `test_steam_art.py`,
`lp64_env.py`, `lp64_equiv_compare.py`, `bench/bench.py` and its tests,
`scripts/port/linux/set-steam-art.py`, `port/crash/reporting_linux.c`,
`port/src/sysstubs.c`, `port/src/port_startup.c`, `port/src/port_lp64.c`,
`port/include/port_lp64.h`, `port/include/port_state.h`,
`port/src/port_state_unavailable.c`, `port/src/port_watchpages.c`,
`port/src/romconv_*.c` (Task 7), `CMakeLists.txt`,
`.github/workflows/windows-release.yml`, `.github/ISSUE_TEMPLATE/bug_report.md`,
`tools/midas_ini_help.txt`, `README.md`, `CONTRIBUTING.md`, `AGENTS.md`,
`FORK_CHANGES.md`, and the guides `releases.md`, `crash_reporting.md`,
`save_states.md`, `late_loads.md`, `benchmarks.md`,
`native_64bit_plan.md`, `docs/port/abi-and-portability.md`.

Not touched: `scripts/port/linux-release.Dockerfile`, `hand_gallery.py`,
its template, `review/hand-gallery`, and every strict-tier file
(`STRICT_FILES` and `port/fast3d/`). No task needs one; a task that finds
it does stops and says so.

## Global Constraints

The LP64-006 plan's constraints continue unchanged: the 32-bit program
does not change (`python3 scripts/port/compare_win32.py commits BASE HEAD`
prints `identical in loaded code and data`, or every changed function is
named and explained); fix the declaration, not the symptom; 64-bit
branches sit behind `IS_64_BIT`; runs are judged by what they did; no
`ge: lp64:` line but the informational ones; no ROM-derived data in Git;
upstream-owned files get small mechanical edits; every commit updates
`FORK_CHANGES.md` (FORK-089, or the entry of the feature it changes:
FORK-031 for packaging, FORK-030 for crash reports, FORK-014 for suspend
points, FORK-046 for Steam artwork, FORK-085 for the benchmark); delivery
is direct to `main`, one push a task after its review is clean; never
`git commit --no-verify`; commit messages end with the trailer the
committing session's own model names. This plan adds:

- **Nothing 64-bit is published before Task 11.** Until then the
  `publish` job's `needs`, attest list, asset list and notes are not
  edited, and the README carries no link to a 64-bit file.
- **The i686 packages do not change.** `linux-release.Dockerfile` is not
  edited. `linux_build.py` and `package_linux.py` default to `i686`, and
  for `i686` they run the same commands with the same arguments as at
  f04e0a7 (`test_linux_build.py` holds those lists). The `linux` job's
  lines are not edited before Task 11.
- **One baseline.** Every shipped ELF file of the x86_64 packages is
  built in `linux-release-x86_64.Dockerfile`, needs no glibc symbol above
  2.35, and resolves every library from the package's `lib/` or the
  packager's system list.
- **A package holds only what its allowlist names.** No ROM, save,
  profile, `midas.ini`, texture pack, font pack, debug file or crash
  envelope; the Enhanced pack's files are never bundled.
- **What ships is what was checked.** A check that names "the packaged
  executable" runs the `IronSights` extracted from the tarball, with the
  package's own `lib/`.
- **A pool raised past half** is checked, before it lands, for a change
  in any comparison stream and in the `synthesizer updates dropped`
  counts (the hand-over's pointer).
- **Settings, input idioms, load before play, render performance**
  (`AGENTS.md`). No task adds a setting, a per-frame cost or first-use
  work. Task 5 changes one watch row's text and Task 2 one stamp at 64
  bits; neither adds an input a supported device cannot send.
- **The Mac's rules** (LP64-006, unchanged): containers only through
  `lp64_env.py`; pull no image but the project's own pinned ones; leave
  every other container alone; all output under `build/lp64/`; never
  create `build_win`; never read or write a real save, profile or
  settings file; the ROM only through `GE_ROM` or `/rom`.
- New Python uses the standard library only; new source files stay under
  500 lines; shell scripts start with `#!/usr/bin/env bash` and
  `set -euo pipefail`; every waiting command has a deadline.

### Checks

**Task check.** The LP64-006 plan's task check, steps 1 to 9, on the
GCC 13 development trees, with that plan's rule for when it runs in full.
Here it runs in full twice: in Task 8 and before the final review. Between
them a task that changes game C or C++ runs the plain aarch64 play sets
and the mission smoke; a task that changes only tools, tests or documents
runs neither, and says so. Every task runs `compare_win32.py`, the static
checks and every harness line of the CI tests step in the x86_64
container.

**Package check.** Exists from Task 3; every later task that changes C,
C++, the build script or the packager runs it. `REL` is
`python3 scripts/port/lp64_env.py run --arch x86_64 --release --`, `DEV`
the same without `--release`.

```bash
# P1. Build in the release image.
REL python3 scripts/port/linux_build.py --arch x86_64 --work build/lp64/ship/x64/deps --output build/lp64/ship/x64
# P2. Package into a fresh folder.
rm -rf build/lp64/ship/x64/dist && REL python3 scripts/port/package_linux.py --arch x86_64 --build build/lp64/ship/x64 --output build/lp64/ship/x64/dist
# P3. The ten crash-report cases on the real packages.
REL python3 scripts/port/test_sentry_linux.py --arch x86_64 --package build/lp64/ship/x64/dist/Iron-Sights-linux-x86_64.tar.gz --appimage build/lp64/ship/x64/dist/Iron-Sights-linux-x86_64.AppImage --steamdeck build/lp64/ship/x64/dist/Iron-Sights-steamdeck-x86_64.zip --output build/lp64/ship/x64/smoke
# P4. The packaged executable boots and plays, in the newer container.
rm -rf build/lp64/ship/x64/run && mkdir -p build/lp64/ship/x64/run && tar -xzf build/lp64/ship/x64/dist/Iron-Sights-linux-x86_64.tar.gz -C build/lp64/ship/x64/run
DEV python3 scripts/port/lp64_boot.py --rom /rom/baserom.u.z64 --expect-run --frames 600 build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/IronSights
DEV python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/IronSights --rom /rom/baserom.u.z64 --video --frames 1800 --jobs 4 --output build/lp64/ship/x64/smoke-missions/missions.json
```

Expected: P1 and P2 exit 0 and P2 prints the three package paths; P3
prints its pass line for all ten cases (a case that fails only under
Rosetta 2 is named in FORK-089 and judged by CI's run, which is on real
x86_64); P4 prints `OK: ran 600 frames` and 20 of 20 missions.

## Review Focus

The five conditions the spec implies and no existing test exercises, most
likely first. Each has its test in the task named.

1. **An x86_64 package on a system with no 32-bit libraries.** Expected:
   it starts; nothing it loads resolves outside its `lib/` and the system
   list. Task 3: the packager fails on any `ldd` line that is "not found"
   or that names a path under an `i386` or `lib32` folder, in an image
   that has no i386 architecture.
2. **Both Deck ZIPs installed side by side.** Expected: two menu entries
   and two sets of artwork; neither install rewrites the other's. Task 3:
   `test_steam_art.py`.
3. **A folder or cache an i686 build wrote, opened by the x86_64 build.**
   Expected: campaign saves and `midas.ini` load (LP64-006 Task 13 covers
   saves); 32-bit suspend images are left untouched and unused; an icon
   cache written at the other width is used correctly or rebuilt, never
   misread. Task 8 Step 5.
4. **A suspend key or row used with each input at 64 bits.** Expected:
   the notice shows, nothing waits for an answer, and play continues, by
   keyboard, mouse and pad. Task 5.
5. **A 64-bit stop with no one to dismiss a message.** Expected: under
   `GE_NOINPUT`, the offscreen or dummy video driver, or a headless
   build, no message box opens and the exit status is unchanged. Task 6.

---

### Task 1: The x86_64 release image and build

**Files:**
- Create: `scripts/port/linux-release-x86_64.Dockerfile`,
  `scripts/port/test_linux_build.py`
- Modify: `scripts/port/linux_build.py`, `scripts/port/lp64_env.py`,
  `scripts/port/test_lp64_env.py`, `.github/workflows/windows-release.yml`
  (one line in the tests step), `FORK_CHANGES.md`

**Interfaces:**
- Produces: `linux_build.py --arch {i686,x86_64}` (default `i686`);
  `linux_build.ARCHITECTURES`, a mapping from those names to records with
  `name`, `pointer_bits`, `sdl_libdir`, `sdl_flags`, `sentry_force32`;
  pure functions `sdl_configure_args(arch, source, build, prefix)`,
  `sdk_configure_args(arch, source, build)` and
  `game_configure_args(arch, output, prefix, sentry_include)`, each
  returning the argument list `run()` receives; `build(work, output,
  arch="i686")`. `sentry-build.json` gets `architecture` from the record.
- Produces: `lp64_env.py run --arch x86_64 --release -- CMD` runs
  `midas-linux-release-x86_64`; `lp64_env.py build-images --release`
  builds it, and plain `build-images` does not.
- Values: the Dockerfile uses the same pinned base line as
  `linux-release.Dockerfile` and installs `ca-certificates cmake git
  python3 build-essential libgl-dev libcurl4-openssl-dev libx11-dev
  libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev
  libxss-dev libasound2-dev libpulse-dev libudev-dev squashfs-tools
  patchelf file ccache`, with no i386 architecture. For `x86_64`: SDL has
  no `-m32` flags and `CMAKE_INSTALL_LIBDIR=lib` (the directory
  `CMakeLists.txt:745-748` links); the SDK has no `SENTRY_BUILD_FORCE32`;
  the game adds `-DGE_POINTER_BITS=64`; the SDL options (Wayland, IBus,
  Fcitx, DBus, Jack, sndio, libsamplerate off; X11, OpenGL, ALSA, Pulse
  on) are the i686 ones.

- [ ] **Step 1: Write the failing tests** in `test_linux_build.py`:
  `test_i686_sdl_arguments_are_unchanged`,
  `test_i686_sdk_arguments_are_unchanged` and
  `test_i686_game_arguments_are_unchanged` compare each function's list
  with the literal list `linux_build.py` passes at f04e0a7 (copy the
  three lists from that commit into the test, paths replaced by
  placeholders); `test_x86_64_game_asks_for_64_bit_pointers` asserts
  `-DGE_POINTER_BITS=64` is present and no argument contains `m32` or
  `i386`; `test_x86_64_sdk_is_not_forced_to_32_bits`;
  `test_unknown_architecture_is_refused` (argparse exits 2). In
  `test_lp64_env.py`: `test_release_image_is_chosen_only_with_the_flag`
  and `test_plain_build_images_does_not_build_the_release_image`.
- [ ] **Step 2: Run them.** `python3 scripts/port/test_linux_build.py`.
  Expected: errors, the functions do not exist.
- [ ] **Step 3: Implement** the table, the three functions, `--arch`, the
  Dockerfile and the `--release` flag. In `lp64_env.py`, read the user id
  through `getattr(os, "getuid", None)` so `build-images` runs on Windows
  (the hand-over's pointer).
- [ ] **Step 4: Run the tests.** Expected: all pass. Add the test's line
  to the workflow's tests step.
- [ ] **Step 5: Build the image and the game.**
  `python3 scripts/port/lp64_env.py build-images --release`, then package
  check P1. Expected: exit 0; `build/lp64/ship/x64/IronSights` is ELF
  64-bit; `sentry-crash`, `lib/libsentry.so` and `lib/libSDL2-2.0.so.0`
  exist and are ELF 64-bit.
- [ ] **Step 6: Fix what GCC 11 refuses** in the video sources, if
  anything (the planning probe compiled only the headless sources there).
  Each fix is at the declaration and compiles unchanged with GCC 13; a
  fix that would change the 32-bit program stops the task for a ruling.
- [ ] **Step 7: Check the baseline.** For each of the four ELF files:
  `objdump -T FILE | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1` prints
  at most `GLIBC_2.35`. Then `lp64_boot.py --expect-run --frames 600` on
  `build/lp64/ship/x64/IronSights` in the development container with
  `LD_LIBRARY_PATH=build/lp64/ship/x64/lib`. Expected: `OK: ran 600 frames`.
- [ ] **Step 8:** `compare_win32.py`, the static checks, the CI harness
  lines; record in FORK-031 and FORK-089; commit.

### Task 2: The build says which architecture it is

**Files:**
- Create: `port/include/port_build_arch.h`,
  `scripts/port/test_build_arch.sh`, `scripts/port/test_build_arch.c`
- Modify: `port/crash/reporting_linux.c:163`, `port/src/sysstubs.c` (the
  `ge: sys: build` line), `port/src/port_startup.c` (the legal-screen
  stamp), `scripts/port/build_info.py`, the workflow's tests step

**Interfaces:**
- Produces: `static inline const char *portBuildArchitecture(void)`,
  returning `"x86_64"`, `"i686"`, `"aarch64"` or `"wasm32"` from the
  compiler's predefined macros, and `"unknown"` otherwise.
- Behaviour: the Sentry tag `architecture` is that string (so i686 stays
  `"i686"`). At 64 bits only, inside `#if IS_64_BIT`: the `ge: sys:
  build` log line ends with ` arch NAME`, and the legal-screen stamp
  reads `Build N - COMMIT - 64-bit`. A comparison build keeps its fixed
  stamp. `build-info.json` gains `"architecture"` for every build (a
  field in a JSON file, not in the program). The About dialog is not
  edited: `port/fast3d/about.cpp` is strict tier.

- [ ] **Step 1: Write the failing harness.** `test_build_arch.sh`
  sources `harness_widths.sh`; its driver asserts the string is `"i686"`
  when `sizeof(void *) == 4` on x86, and `"x86_64"` or `"aarch64"` when
  8, matching `__x86_64__` or `__aarch64__`.
- [ ] **Step 2: Run it** at both widths in the x86_64 container, and at
  64 bits in the aarch64 one. Expected: it does not compile.
- [ ] **Step 3: Implement.** Run again. Expected: pass at every width.
- [ ] **Step 4: Check the 32-bit program.** `compare_win32.py commits
  BASE WORKTREE`. Expected: identical (the header is used on Windows by
  nothing, and the two 64-bit edits are behind `IS_64_BIT`).
- [ ] **Step 5: See it.** P1, then run the tree offscreen to the legal
  screen with `GE_SHOT` set and keep one screenshot of the stamp under
  `build/lp64/ship/`; the log's `ge: sys: build` line ends `arch x86_64`.
  Plain aarch64 play sets and the mission smoke.
- [ ] **Step 6:** Name the harness in the workflow, record in FORK-020
  and FORK-089, commit.

### Task 3: The packager at two architectures

**Files:**
- Create: `scripts/port/linux/START-HERE-x86_64.txt`,
  `scripts/port/linux/STEAM-DECK-x86_64.txt`
- Modify: `scripts/port/package_linux.py`, `test_distribution.py`,
  `test_sentry_linux.py`, `test_steam_art.py`,
  `scripts/port/linux/set-steam-art.py`, `FORK_CHANGES.md`

**Interfaces:**
- Consumes: Task 1's tree layout and `linux_build.download`.
- Produces: `package_linux.py --arch {i686,x86_64}` (default `i686`);
  `package(build, output, arch="i686")`; `package_linux.TARGETS`, a
  mapping to records with `suffix`, `elf_class` (1 or 2), `elf_machine`
  (3 or 62), `system_libdir`, `runtime_url`, `runtime_sha256`,
  `dockerfile`, `baseline`, `start_here`, `steam_deck`, `sums_name`,
  `info_prefix`.
- Values for `x86_64`: outputs `Iron-Sights-linux-x86_64.tar.gz` (top
  folder `Iron-Sights-linux-x86_64`), `Iron-Sights-linux-x86_64.AppImage`,
  `Iron-Sights-steamdeck-x86_64.zip` (top folder
  `Iron-Sights-steamdeck-x86_64`), `SHA256SUMS-linux-x86_64.txt`,
  `linux-x86_64-build-info.json`, `linux-x86_64-build-inputs.json`;
  `system_libdir` `/usr/lib/x86_64-linux-gnu`; runtime
  `https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64`.
  The i686 record holds today's values, so its outputs keep their names
  (`SHA256SUMS-linux.txt`, `linux-build-info.json`,
  `linux-build-inputs.json`).
- Produces: `refuse_unshippable_build(build, arch)`, called first; it
  calls today's `refuse_comparison_build` (name and message kept, its
  tests untouched) and adds refusals for a cache with
  `GE_SANITIZE`, `GE_LP64_ORACLE` or `GE_HEADLESS` on, `GE_VIDEO` off or
  `GE_ROMDATA_FALLBACK` on, and for an `IronSights` whose ELF class or
  machine is not the target's. A tree without a cache still passes the
  cache checks, as today.
- `build-inputs.json` gains `"play_harness": true|false`, read from
  whether the executable holds the symbol `portPlayPlayerTick`
  (decision 4: `true` at 64 bits).
- `test_sentry_linux.py --arch {i686,x86_64}` (default `i686`) takes its
  ELF bytes, prefixes and expected `architecture` tag from the same table.
- `set-steam-art.py`: when the `IronSights` beside it is ELF class 2 the
  menu entry is `iron-sights-steamdeck-x86_64.desktop` with
  `Name=Iron Sights (64-bit)`; class 1 keeps today's name and text.

- [ ] **Step 1: Write the failing tests.** In `test_distribution.py`:
  `test_i686_target_keeps_its_names_and_runtime` (the record equals the
  literals at f04e0a7); `test_x86_64_outputs_are_named_for_the_architecture`;
  `test_packager_refuses_a_sanitizer_build`, `_an_oracle_build`,
  `_a_headless_build`, `_a_build_with_compiled_in_assets` (one cache line
  each; the output folder is not created);
  `test_packager_refuses_an_executable_of_the_other_width` (a 20-byte
  ELF header of class 1 with `--arch x86_64`, and the reverse);
  `test_library_from_a_32_bit_folder_is_refused_at_x86_64` (an `ldd` text
  naming `/usr/lib/i386-linux-gnu/libfoo.so`). In `test_steam_art.py`:
  `test_a_64_bit_folder_gets_its_own_menu_entry` and
  `test_two_installs_keep_two_entries_and_two_artwork_sets`.
- [ ] **Step 2: Run them.** Expected: fail on missing names.
- [ ] **Step 3: Pin the x86_64 runtime.** Download it once into
  `build/lp64/ship/downloads/`, take its sha256, and confirm it equals
  the digest GitHub lists for that asset (`gh api
  repos/AppImage/type2-runtime/releases/tags/20251108 --jq '.assets[] |
  select(.name=="runtime-x86_64") | .digest'`). Write the hash into the
  table only if the two agree; otherwise stop. Confirm the runtime's
  licence file is the one already pinned (`RUNTIME_SOURCE`).
- [ ] **Step 4: Implement** the table, `--arch`, the refusals, the
  `ldd` path rule, the Deck names and the two text files. The x86_64
  texts say: a native 64-bit build; needs glibc 2.35 or newer and
  OpenGL 3.3 drivers, no 32-bit libraries; suspend points are not
  available in this build and campaign saves are (one sentence, from
  `portStateMessage`); the i686 package remains for 32-bit systems and
  for suspend points. `STEAM-DECK-x86_64.txt` keeps the sentence that no
  Deck has run it until Task 10 says otherwise.
- [ ] **Step 5: Run the tests.** Expected: all pass.
- [ ] **Step 6: Run the package check, P1 to P4.** Record the tarball's
  file list and sizes in FORK-031. Assert by hand once, then in
  `test_sentry_linux.py`: no member is `midas.ini`, a ROM, a `.debug` or
  an envelope; nothing under `Data/TexturePacks` or `Data/FontPacks`.
- [ ] **Step 7:** `compare_win32.py`, static checks, CI harness lines;
  record in FORK-031, FORK-046 and FORK-089; commit.

### Task 4: Crash reports from a 64-bit build, and the CI job

**Files:**
- Modify: `scripts/port/test_sentry_linux.py`, `scripts/port/sentry_tools.py`
  (only if `upload` assumes an i686 file), `.github/workflows/windows-release.yml`,
  `scripts/port/crash_reporting.md`, `scripts/port/releases.md`,
  `FORK_CHANGES.md`

**Interfaces:**
- Consumes: Tasks 1 to 3.
- Produces: the CI job `linux-x86_64`, a copy of `linux` with these
  differences and no others: image `midas-linux-release-x86_64` from the
  new Dockerfile; build folder `build_linux_x86_64_ci`; `--arch x86_64`
  on the three scripts; cache keys `linux-x86_64-ccache-...` and
  `linux-x86_64-downloads-...` hashed from the new Dockerfile and the two
  scripts; artifacts `linux-x86_64-player-package` (the three packages,
  `SHA256SUMS-linux-x86_64.txt`, `linux-x86_64-build-*.json`) and
  `linux-x86_64-smoke-diagnostics`; `timeout-minutes: 35`. It is **not**
  added to `publish`'s `needs`.

- [ ] **Step 1: Tighten the crash test, failing first.** In
  `test_sentry_linux.py`, for the `enabled` case at either architecture,
  assert that the received event's debug image for `IronSights` carries
  the build id of `symbols/IronSights.debug` (read with `readelf -n`),
  and that at least one frame's instruction address lies inside that
  image. Run P3. Expected: it fails only if the report is not
  symbolicable; if it passes at once, say so in the report and keep it.
- [ ] **Step 2: Run P3** for x86_64. Expected: ten cases pass, the tag is
  `x86_64`. A failure that also occurs with the crash daemon's tracing
  under Rosetta 2 and nowhere else is recorded and left to Step 5.
- [ ] **Step 3: Check the upload tool without uploading.** Read
  `sentry_tools.py upload`; run it with `--help` and, if it has a dry
  mode, on `build/lp64/ship/x64/symbols/IronSights.debug`. No token is
  used on the Mac and nothing is sent.
- [ ] **Step 4: Add the job** and update `releases.md` ("What is built
  and shipped": the x86_64 artifacts are built and tested on every push
  and are not yet published) and `crash_reporting.md` ("Linux releases":
  the x86_64 runtime, the `architecture` tag, the bundled 64-bit
  libcurl; delete the stale paragraph at `:158-162` that says texture
  entries reject 64-bit pointers).
- [ ] **Step 5: Push and read CI.** Expected: `linux-x86_64` passes on
  the runner's real x86_64 processor, its ten cases included, and the
  release that push publishes holds exactly the eleven assets it held
  before. Download the artifact's `SHA256SUMS-linux-x86_64.txt` and
  `linux-x86_64-build-inputs.json` with `gh run download` into
  `build/lp64/ship/ci/` and record the compiler line and the bundled
  library list in FORK-031.
- [ ] **Step 6:** Record in FORK-030, FORK-031 and FORK-089.

### Task 5: The suspend-point notice

**Files:**
- Modify: `port/include/port_state.h`, `port/src/port_state_unavailable.c`,
  `port/src/port_watchpages.c`, `tools/midas_ini_help.txt`,
  `scripts/port/save_states.md`, `scripts/port/test_states.sh` and its
  driver, `FORK_CHANGES.md`

**Interfaces:**
- Produces: in `port_state.h`, `PORT_STATE_AVAILABLE`, 0 when
  `IS_64_BIT` and 1 otherwise (the browser build is not changed here:
  decision 7). `portStateMessage()` at 64 bits returns "Suspend points
  are unavailable in 64-bit builds. Campaign saves work as usual."
- Produces: `const char *portStateRowLabel(void)` in `port_state.h`,
  which the watch page calls only inside `#if !PORT_STATE_AVAILABLE`, so
  the 32-bit program keeps its bytes.
- Behaviour at 64 bits: on Watch > Gameplay > Iron Sights the row reads
  `Suspend Points: unavailable`; choosing it, or pressing a suspend key,
  shows the message as today and returns to play. The four bind rows
  stay, so a `midas.ini` shared with a 32-bit install keeps its keys.
  No ini key or section changes. At 32 bits nothing changes.

- [ ] **Step 1: Write the failing test.** Extend the `test_states`
  driver: at 64 bits, `portStateRowLabel()` contains `unavailable` and
  `portStateMessage()` contains `Campaign saves`; at 32 bits the label is
  exactly `Suspend Points\n`.
- [ ] **Step 2: Run** `bash scripts/port/test_states.sh` at both widths.
  Expected: the 64-bit half fails.
- [ ] **Step 3: Implement.** Run again. Expected: both halves pass.
- [ ] **Step 4: Check each input** on the 64-bit video tree with the
  `menus` set's watch walk plus one recorded pad press of the row and one
  keyboard F5 (`GE_INPUTPLAY`): the log shows the message each time and
  the run reaches its frame limit. Keep one screenshot of the row under
  `build/lp64/ship/`.
- [ ] **Step 5: Write the manual.** `tools/midas_ini_help.txt` (the
  suspend keys: "not available in 64-bit builds"), `save_states.md`
  ("64-bit builds": the notice exists; remove "is LP64-007's").
- [ ] **Step 6:** `settings_placement.py`, `compare_win32.py`
  (identical: the macro is 1 at 32 bits), static checks, plain aarch64
  play sets, mission smoke, package check; record in FORK-014 and
  FORK-089; commit.

### Task 6: A 64-bit stop tells the player why

**Files:**
- Create: `port/src/port_lp64_notice.c`, `scripts/port/test_lp64_notice.sh`,
  `scripts/port/test_lp64_notice.c`
- Modify: `port/include/port_lp64.h`, `port/src/port_lp64.c` (before each
  of its `exit` calls), `port/src/sysstubs.c` (`sysFatalError`, inside
  `#if IS_64_BIT`), `CMakeLists.txt` (the file compiles at 64 bits only),
  the workflow's tests step

**Interfaces:**
- Produces: `s32 portLp64StopNoticeWanted(const char *noInput, const char
  *videoDriver, s32 haveVideo)`, pure: 0 when `haveVideo` is 0, when
  `noInput` is set and not `"0"`, or when `videoDriver` is `"offscreen"`
  or `"dummy"`; else 1. `void portLp64StopNotice(const char *text)`: if
  wanted, `SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Iron Sights",
  text, NULL)`; in a headless build it does nothing. Exit statuses and
  log lines do not change.

- [ ] **Step 1: Write the failing harness**: a table test of
  `portLp64StopNoticeWanted` over the eight combinations, at 64 bits
  (`harness_widths.sh`, 32 bits listed on `HARNESS_WIDTH_EXEMPT` with
  the reason "the file compiles at 64 bits only").
- [ ] **Step 2: Run it.** Expected: it does not link.
- [ ] **Step 3: Implement**, following how the no-ROM message
  (`port/src/romdata.c:1554-1565`) opens its box.
- [ ] **Step 4: Check that nothing waits.** `lp64_play.py run --set
  stops` plain and with `--video` on the 64-bit trees, and
  `test_lp64_address.sh`. Expected: every case passes with its status and
  none reaches its time limit.
- [ ] **Step 5:** `compare_win32.py` (identical), static checks, plain
  aarch64 play sets, mission smoke, package check; record in FORK-089;
  commit.

### Task 7: Converters refuse a damaged file

**Files:**
- Modify: the `port/src/romconv_*.c` files that hold the six reads,
  their fixture tests under `scripts/port/test_romconv*`, `FORK_CHANGES.md`

**Interfaces:**
- Consumes: the converters' existing refusal (`ge: lp64: cannot convert
  CLASS "FILE": ...`, then the stop by name).
- Scope (decision 9): the six reads FORK-089 lists as unbounded at both
  widths (an animation's stream length and entries offset, a sequence's
  address and lengths, the end of an AI list, a glyph's extent, a wave of
  an unknown type, a `G_TEXRECT` tail in a model list), bounded in the
  64-bit converters only; and fixtures for fix wave C's two paths (a
  missing ROM segment stops by name; a compressed text bank that fails to
  inflate is zeroed). The 32-bit loaders are not edited.

- [ ] **Step 1: Locate each read.** `grep -n` FORK-089 for each phrase
  and write the file and line of each of the six into the task's report.
  A read that is in game code shared with the 32-bit build, not in a
  converter, is listed and left: this task does not edit it.
- [ ] **Step 2: One failing fixture a read.** Each builds the smallest
  record that sends the read past its buffer and asserts the converter
  returns its refusal. Under the sanitizer harness the unfixed read is
  reported; record that output as the failure.
- [ ] **Step 3: Bound each read**, refusing the file. Run the fixtures.
  Expected: pass, plain and under the sanitizers.
- [ ] **Step 4: Prove the stock ROM is untouched.** The oracle (`AGENTS.md`'s
  line): every class `N files, 0 differ`. The 20-mission smoke at 64
  bits, plain and sanitizer. `lp64_census.py compare`: equal.
- [ ] **Step 5:** `compare_win32.py` (identical: converters are not in a
  32-bit shipped build), static checks, plain aarch64 play sets, package
  check; record in FORK-089, with any read left in shared code named for
  upstream; commit.

### Task 8: The release compiler's build is the checked build

No source changes unless a check fails. This is the plan's first full
checkpoint.

**Files:**
- Modify: `scripts/port/lp64_equiv_compare.py` and its test (Step 1),
  `FORK_CHANGES.md`

- [ ] **Step 1: Guard `record['video']`** (the hand-over's pointer):
  `compare` treats an `equiv.json` without the field as a headless run
  and refuses folders of mixed kinds with `--expect`, naming them. Test
  first: `test_a_record_without_the_field_reads_as_headless`,
  `test_mixed_kinds_are_refused_with_expect`.
- [ ] **Step 2: The task check in full** on the GCC 13 trees (steps 1 to
  9). Expected: as LP64-006's close recorded, with the comparison's
  counts `74 identical, 2 as recorded` headless, `192 identical, 8 as
  recorded` default and `190 identical, 10 as recorded` with every
  option.
- [ ] **Step 3: The play sets on the packaged executable.** After the
  package check: `DEV python3 scripts/port/lp64_play.py run --video --rom
  /rom/baserom.u.z64 --set demos,stages,stops,menus,tours,attract,sound
  --output build/lp64/ship/play
  build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/IronSights` (the
  package is a video build, so there is no headless pass of it).
  Expected: `OK: 148 of 148 cases`. Then `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt
  build/lp64/ship/play`. Expected: `OK: ... within budget`.
- [ ] **Step 4: Same streams from both compilers.** In the release
  image, configure `build/lp64/ship/cmp-x64-gcc11` and
  `build/lp64/ship/cmp-x64-gcc11-video` as `cmp-x64` and `cmp-x64-video`
  are (`-DGE_COMPARE=ON`, the video one with the pinned SDL prefix), and
  build them. In the development container run step 9's x86_64 line for
  each of `sim`, `video-default` and `video-all` on both the GCC 13 and
  the GCC 11 trees, then `lp64_equiv.py compare --streams sim,gfx,snd
  DIR_GCC13 DIR_GCC11` for each. Expected: every stream identical. A
  difference is traced to its first index and its cause before the task
  goes on; a cause that is the compiler's choice in undefined or
  unspecified code is fixed at the declaration.
- [ ] **Step 5: A folder from the other width** (Review Focus 3). With
  one `HOME` and `GE_ICON_CACHE_DIR`: run `cmp-i686-video` to the title
  so it writes the icon cache, then the 64-bit video tree with `--shots`
  on a case that draws the HUD's weapon icons, and compare that
  screenshot with one from a fresh cache. Then the reverse order.
  Expected: equal pictures, or a `ge:` line saying the cache was rebuilt.
  Put a file named like a 32-bit suspend image in the run's states
  folder first and assert its bytes are unchanged afterwards. A misread
  cache stops the task: `weapon_cache.cpp` is strict tier.
- [ ] **Step 6: Late loads.** `late_load_audit.py --no-pack
  --video-driver offscreen --keep` for all stages on the GCC 13 x86_64
  video tree and on the packaged executable, then `--compare`. Expected:
  the same events by kind and name in every stage.
- [ ] **Step 7: The sanitizers with the release compiler.** A
  `-DGE_SANITIZE=ON` video tree in the release image
  (`build/lp64/ship/x64-san`), then the mission smoke with video.
  Expected: 20 of 20 with no report.
- [ ] **Step 8:** Record every figure in FORK-089 with the host named.

### Task 9: The benchmark drives a Linux package

**Files:**
- Modify: `scripts/port/bench/bench.py`, `bench_plan.py`, their tests,
  `scripts/port/benchmarks.md`, `FORK_CHANGES.md`

**Interfaces:**
- Produces: `bench.py run --smoke --build NAME=PATH` accepts a Linux
  executable: it copies the `lib/` folder beside it into the isolated
  run folder (as it copies `SDL2.dll` on Windows) and sets
  `LD_LIBRARY_PATH` to it; it accepts the log line `ge: sys: Linux`; the
  quiet-machine guard reads `/proc/loadavg` and the CPU probe
  `/proc/stat`; the process list uses `IronSights`. Windows behaviour is
  unchanged. The profilers stay Windows-only, and the guide says so.

- [ ] **Step 1: Failing tests** in the bench tests:
  `test_linux_run_copies_the_library_folder`,
  `test_linux_run_sets_the_library_path`,
  `test_linux_guard_reads_loadavg` (from a fixture string),
  `test_windows_plan_is_unchanged` (the plan for an `.exe` equals the
  literal it gives at f04e0a7).
- [ ] **Step 2: Run them.** Expected: fail.
- [ ] **Step 3: Implement.** Run. Expected: pass.
- [ ] **Step 4: Try it offscreen** on the packaged executable in the
  development container (`SDL_VIDEODRIVER=offscreen` passed through), one
  scenario. Expected: it completes and prints a table. Its figures are a
  software renderer's under translation and are not recorded as results.
- [ ] **Step 5:** Add "Linux packages" to `benchmarks.md` (pass `--out`
  on Linux: the default is under `build_win`); add `test_bench.py` to the
  workflow's tests step if it is not there; record in FORK-085 and
  FORK-089; commit.

### Task 10: One session on real hardware (the owner's laptop)

A stop point: this task is the owner's hands, on the Windows laptop
(Core i7-10870H, RTX 3070), in its WSL distribution, not in Docker.
Decision 5 says which hardware; if the owner has a Steam Deck the same
list runs there too and `STEAM-DECK-x86_64.txt` loses its "not yet
tested" sentence. The session's results come back as a commit to
FORK-089, as the LP64-006 laptop step did.

**Files:**
- Modify: `review/hand-gallery/` (by `hand_gallery.py generate` only),
  `FORK_CHANGES.md`, `scripts/port/linux/STEAM-DECK*.txt` (only if a Deck ran)

- [ ] **Step 1: Regenerate the gallery** for the rendering inputs this
  plan changed (`port_startup.c`, `port_watchpages.c`), review the
  changed cells, commit. Expected: no hand, arm or weapon cell changes.
- [ ] **Step 2: Get the packages CI built** for the commit under test:
  `gh run download` of `linux-x86_64-player-package` and
  `linux-player-package`; `sha256sum -c` both sums files.
- [ ] **Step 3: Name the renderer.** Start the x86_64 tarball's game in
  WSL with the owner's ROM. The log's OpenGL renderer line must name the
  graphics card (through WSL, `D3D12 (NVIDIA GeForce RTX 3070 ...)`). If
  it names `llvmpipe`, the session records that no GPU was reached, does
  Steps 4 to 6 anyway, and the GPU items stay open for decision 5's
  other way.
- [ ] **Step 4: Play.** With the tarball, then the AppImage: the title,
  one mission to its end, the watch and Options pages, a campaign save
  made by the Windows 32-bit game loading here, the suspend notice by key
  and by row, Jungle walked from start to end (owner question 1: say
  whether the room loads are felt), an HD texture pack copied from the
  laptop's own `build_win/Data/TexturePacks` into this install and a
  mission played with it, the Enhanced font download and removal from the
  Fonts page, and save transfer. Each is a line in FORK-089: seen, or
  what went wrong.
- [ ] **Step 5: A real crash report.** Run the game's
  `--test-crash-reporting`, and confirm in Sentry that the event is
  tagged `x86_64` and its frames carry function names.
- [ ] **Step 6: Measure.** `bench.py run --smoke` with the i686 and the
  x86_64 packaged executables in one session, visible window, quiet
  machine; then `late_load_audit.py --keep` on both with the pack and
  `--compare`. Expected: frame times within the runs' own range of each
  other or the x86_64 build faster; late loads equal by kind and name
  except Jungle. A cost beyond the range is recorded as the 64-bit
  build's price or sent back as a defect. `lp64_play.py peaks --budget`
  on any play folder made here: a line a few bytes past half is the
  ratchet (raise at 64 bits to twice the new peak), not a failure,
  unless an exhaustion line accompanies it.
- [ ] **Step 7 (optional): 32-bit pixels.** `lp64_equiv.py run --video
  --draw --shots` for the 22 cases on the i686 and x86_64 comparison
  builds in Docker there, then `lp64_equiv_pictures.py shots`.
- [ ] **Step 8:** Commit the record and push.

### Task 11: Publish, with the documents

**Files:**
- Create: `scripts/port/linux_64bit.md`
- Modify: `.github/workflows/windows-release.yml` (`publish`),
  `README.md`, `scripts/port/linux/START-HERE.txt` and
  `START-HERE-x86_64.txt` (verification names), `scripts/port/releases.md`,
  `scripts/port/release/DATA-README.txt`, `CONTRIBUTING.md`, `AGENTS.md`,
  `scripts/port/late_loads.md`, `scripts/port/native_64bit_plan.md`,
  `docs/port/abi-and-portability.md`,
  `.github/ISSUE_TEMPLATE/bug_report.md`, `FORK_CHANGES.md`

**Interfaces:**
- `publish` needs `linux-x86_64` too and downloads its artifact; the
  attest list gains the three x86_64 packages; the checksum step also
  runs `sha256sum -c SHA256SUMS-linux-x86_64.txt`; the Python block
  asserts `linux-x86_64-build-info.json` has the same commit, `ci` and
  not `dirty`; `gh release create` gains the three packages, the sums
  file and the two JSON files (seventeen assets). The notes gain one
  paragraph: the 64-bit Linux and Steam Deck downloads, glibc 2.35 or
  newer, no 32-bit libraries, no suspend points in 64-bit builds, the
  i686 downloads unchanged. Which Deck ZIP the notes and README name
  first follows decision 2 and Task 10's result.

- [ ] **Step 1: The developer's guide.** `linux_64bit.md`: the two
  images and what each is for; the development build (`lp64_env.py run
  --arch x86_64`, the configure line, the boot check and play sets); the
  release build and package (the package check's commands); what a
  64-bit build lacks (suspend points) and prints (`ge: lp64:` lines);
  what is not built (Windows x64, macOS, aarch64 packages). Point to it
  from `README.md`'s build section, `CONTRIBUTING.md` and `AGENTS.md`.
- [ ] **Step 2: The player's documents.** README: the Linux and Steam
  Deck download paragraphs (`:40`, `:42`) name both architectures and
  what each needs; `:98` and the suspend section (`:336-358`) say 64-bit
  builds have none. `DATA-README.txt`'s Windows-only sentence gains the
  Linux wording. The issue template asks which package (Windows x86,
  Linux i686, Linux x86_64, Steam Deck i686 or x86_64, browser).
- [ ] **Step 3: Stale statements.** `native_64bit_plan.md`, "Crash
  reports and the rest of the port layer": the Linux handler needed no
  replacement and external textures were widened in LP64-003. A note at
  the top of `docs/port/abi-and-portability.md` says it predates the
  64-bit port and points to the plan. `late_loads.md` names the packaged
  executable's audit.
- [ ] **Step 4: Edit the `publish` job** as "Interfaces" says. Check it
  offline: the Python tests that read the workflow's text,
  `bash scripts/port/test_harness_widths.sh` and
  `bun scripts/bright-builds-check.ts all`; then read the job's diff
  line by line against "Interfaces".
- [ ] **Step 5: Push and read the release.** `gh release view --json
  assets` lists seventeen assets, the six packages among them. Download
  `Iron-Sights-linux-x86_64.tar.gz` and its sums file into
  `build/lp64/ship/published/`; `sha256sum -c`; `gh attestation verify
  Iron-Sights-linux-x86_64.tar.gz -R OWNER/REPO`; extract and run the
  package check's P4 on it. Expected: all pass; the published bytes are
  the checked build's.
- [ ] **Step 6:** Record in FORK-018, FORK-031 and FORK-089. If the push
  fails to publish, fix forward; a release is never replaced.

### Task 12: The carried questions

Runs only for questions the owner has answered yes; with none, the task
is a note in FORK-089 and the hand-over keeps the list.

**Files:**
- Modify: the files each applied patch names, `scripts/port/lp64_equiv_expected.txt`
  (only where a recorded difference now matches), `FORK_CHANGES.md`

- [ ] **Step 1: Apply the patches answered yes** (decision 12), in
  one batch on one commit: apply each patch from
  `scripts/port/lp64_patches/`; `compare_win32.py` names every changed
  32-bit function, and each is one the patch's README names; the i686
  mission smoke; then the task check's step 9 in full, since a 32-bit fix
  can turn a recorded difference into a match, which fails until its line
  leaves `lp64_equiv_expected.txt`. A question answered no, or not
  answered, changes nothing and stays in the hand-over. `t15-fabsf.patch`
  is strict tier and is not applied here.
- [ ] **Step 2:** The package check (a patch may touch code both widths
  compile); record in FORK-089; commit and push; read CI, and that the
  release it publishes still holds seventeen assets.

### Task 13: Close

- [ ] **Step 1: The task check and the package check in full**, on
  `main` as pushed.
- [ ] **Step 2: The final whole-milestone review** and its one fix wave.
- [ ] **Step 3: The record.** `native_64bit_plan.md`: LP64-007 Done with
  what ran where; milestone 7's settled reading; the plan's own status
  line. FORK-089's title names LP64-006 and LP64-007. A hand-over block,
  "After the 64-bit plan", for the macOS and Apple TV project and for
  upstream: what is not built (macOS, tvOS, Windows x64, an aarch64
  package, a relocating suspend format), the hand-over items this plan
  did not take (the table above), every open owner question, and the
  "Kept deferred" list.
- [ ] **Step 4: Delete the run folders** under `build/lp64/ship/` that
  no record names; keep the trees.
- [ ] **Step 5:** Commit, push, read CI on all three workflows.

## What this plan does not do

- No macOS, Apple TV, iOS, Windows x64 or 64-bit browser build, and no
  aarch64 Linux package (the design's non-goals and decisions).
- No suspend points at 64 bits; a relocating format is a separate project.
- No Wayland-native window: SDL is built for X11 as the i686 one is.
- No profile of a Linux build: the sampler is Windows code.
- No architecture in the About dialog (`about.cpp` is strict tier); the
  legal screen, the log, `build-info.json`, the package's name and its
  instructions carry it.
- No notice in the browser build, which has the same silent suspend row
  (decision 7).
- No change to the i686 or Windows packages, their image or their job.
- No fix to reads the 32-bit loaders share, to the demos' headless
  against video difference, or to anything on the "Kept deferred" list.
- No claim about a Steam Deck unless one ran in Task 10.

## Decisions for the owner

Thirteen, each with a recommendation and what the other way costs. The
tasks are written for the recommendations. **Accepted on 2026-11-24: all
thirteen recommendations**, decision 12 with its picks as written (yes to
questions 2, 4, 6 except `fabsf`, and 7; no for now to 3 and 5; 1 and 8
wait for Task 10's session). Whether the owner has a Steam Deck is not
yet answered; Task 10 asks.

1. **What ships at 64 bits.** *Recommended:* all three formats for
   x86_64 (tarball, AppImage, Steam Deck ZIP), beside the three i686
   ones, which do not change. *The other way:* a tarball only is about a
   third less packaging work and leaves the Deck on the 32-bit build.
2. **Which Steam Deck ZIP is the one to recommend.** No Deck has run
   either. *Recommended:* publish both; the README names the 64-bit one
   first only if Task 10 ran it on a GPU, and says plainly which was
   tried where. *The other way:* replacing the i686 Deck ZIP outright
   breaks the README's old download link and removes the only Deck build
   with suspend points.
3. **The baseline.** The 64-bit evidence so far is GCC 13 on Ubuntu
   24.04, whose build needs glibc 2.38. *Recommended:* build the packages
   on the i686 packages' baseline (Ubuntu 22.04, GCC 11, glibc 2.35), in
   a second image so the i686 one is untouched, and pay for it in Task 8
   by showing both compilers give the same streams. *The other way:*
   shipping the GCC 13 build needs no second compiler check and will not
   start on Ubuntu 22.04 or Debian 12.
4. **The play harness in the shipped 64-bit build.** *Recommended:* keep
   it. Then the file players run is the one the play sets judged; it is
   reached only through environment variables, like the test switches
   the 32-bit builds already carry, and costs three flag tests a tick.
   The package records it. *The other way:* compiling it out matches the
   32-bit builds and ships a configuration that no play set has run; it
   costs a CMake option and a second round of the checks that do not
   need the harness.
5. **Real graphics hardware.** *Recommended:* one session on your
   laptop, the 64-bit package running in WSL on the RTX 3070 (Task 10),
   and publish after it. If you have a Steam Deck, say so and the same
   list runs there. *The other way:* publishing on the software-renderer
   evidence alone is what the i686 Linux packages did; it costs nothing
   now and leaves the GPU, the benchmark and the frame-end check's cost
   unmeasured.
6. **When the 64-bit packages become public.** Every push to `main`
   publishes a release. *Recommended:* CI builds and tests them as
   artifacts from Task 4, and they join the release in Task 11, after
   Tasks 8 and 10. *The other way:* publishing from the first green CI
   build, marked experimental, gets them to players about a week of work
   earlier and before any GPU has drawn a frame.
7. **The suspend-point notice.** *Recommended:* at 64 bits the watch row
   reads `Suspend Points: unavailable`, the message adds that campaign
   saves work, the keys and bind rows stay, and the manual, README and
   package instructions say so. The browser build, which is silent in
   the same way, is left for the web plan. *The other way:* doing the
   browser in the same change is one macro and costs a browser build and
   the touch audit on a machine that has not run them.
8. **A message when a 64-bit build stops.** *Recommended:* at 64 bits a
   stop shows a message box with the log's sentence, unless the run has
   no one to dismiss it (Task 6). *The other way:* leaving the stops
   log-only matches the 32-bit builds, where the window just closes.
9. **Hardening against damaged files.** *Recommended:* bound the six
   known reads in the 64-bit converters, with a fixture each, and leave
   the 32-bit loaders as they are (Task 7). *The other way:* skipping it
   keeps the 64-bit build exactly as robust as the shipped 32-bit one,
   saves a task, and leaves the list for upstream.
10. **A performance figure.** *Recommended:* teach the benchmark to run
    a Linux package (Task 9) and compare the i686 and x86_64 packages
    once, in Task 10's session. *The other way:* shipping without a
    figure saves a task; nobody would know whether the 64-bit build is
    slower.
11. **Texture packs and fonts at 64 bits.** No run has loaded a pack at
    64 bits. *Recommended:* try them in Task 10 with the pack already on
    your laptop; nothing is downloaded on the Mac and no pack goes near
    Git. *The other way:* downloading the pack into a container on the
    Mac would cover it earlier and brings a third party's files onto
    this machine.
12. **The eight carried questions.** *Recommended:* keep them off the
    path to publishing, and apply in Task 12, in one batch, those you
    answer yes. My picks: yes to 2 (`image.c`'s scratch buffer), 4 (save
    padding), 6 except `fabsf`, and 7 (the three read fixes); no for now
    to 3 (no patch exists) and 5 (the Arm volume ramp); 1 (Jungle's
    portal walk) after you have walked Jungle in Task 10; 8 (the
    synthesizer pool) after you have listened. *The other way:* answering
    none leaves the 32-bit game exactly as it is and the list in the
    hand-over.
13. **What "done" means.** *Recommended:* on `main`: one push publishes
    the Windows package and six Linux and Deck packages, all attested;
    the x86_64 packages pass the package check, the ten crash cases on
    CI's real x86_64 runner, and the play sets, mission smoke and budget
    check on the packaged executable; the GCC 11 and GCC 13 builds give
    the same streams in all 38 cases or each difference is fixed; one
    hardware session is recorded with its renderer named, or its gaps
    are listed and you have accepted them; the notice and the documents
    are in; the 32-bit Windows program is unchanged or every changed
    function is named; and the status table says Done with a hand-over
    for the macOS project.

Stop points, besides the four the execution skill names: Task 10 (your
session); Task 3 Step 3 if the runtime's two hashes disagree; Task 8
Step 4 if a compiler difference is not traced within a day; Task 8
Step 5 if the icon cache is misread across widths.
