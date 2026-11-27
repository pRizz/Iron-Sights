# Native macOS build plan

Iron Sights has no macOS build. This plan makes the 64-bit source build and
run natively on Apple Silicon macOS, and ends with a signed, notarized
download. It is the second of three projects: the
[64-bit port](native_64bit_plan.md) (done on 2026-11-26), this one, then an
Apple TV build, which gets its own plan once this one has a playable Mac
build.

The owner chose the audience, the graphics path and the outline on
2026-11-26 and approved the design on 2026-11-27. Approval is not evidence
that any milestone works; the [milestone status](#milestone-status) table
records what has been built and verified. **Nothing is built yet.**

This plan was drafted under `AGENTS.md` (the fork inventory, the two-tier
gallery rule, render performance, load before play, direct-to-main
delivery), `standards-overrides.md`, the 64-bit plan's decisions and its
"After the 64-bit plan" section, and FORK-089 and FORK-031 of
`FORK_CHANGES.md`.

## Why

- Iron Sights ships for 32-bit Windows, i686 and x86_64 Linux and the
  browser. On the owner's Apple Silicon Mac it runs only as an aarch64 Linux
  build in a container, drawn in software.
- A native build gives Mac players a download and gives the project its
  first build on an Apple GPU.
- The Apple TV build depends on it: tvOS needs the same compiler, the same
  system libraries and the same signing.

## Goals

1. Iron Sights runs natively on Apple Silicon macOS: built by Apple clang
   from this repository, drawing with the Mac's GPU, with sound, keyboard,
   mouse and gamepad.
2. It is the same game as the 64-bit Linux build: its simulation matches the
   aarch64 Linux build tick for tick, or each difference is traced and
   recorded.
3. Players get a Developer ID signed, notarized download that opens with a
   double click, published beside the other packages by CI.
4. It has what the Linux 64-bit packages have: campaign saves and settings
   that move between platforms, crash reporting, the Enhanced font and HD
   texture downloads, the weapon icon cache, save backup and restore.
5. The 32-bit Windows, Linux and browser builds and the 64-bit Linux build
   are unchanged in behaviour.
6. The plan ends with a hand-over the Apple TV project can start from.

## Non-goals

- Intel Macs and a universal binary.
- Apple TV, iOS, and any choice of their graphics path.
- A Metal backend, ANGLE, Vulkan or MoltenVK. ANGLE is the recorded
  fallback; see [Approaches](#approaches).
- The Mac App Store, the App Sandbox, a Homebrew formula.
- HDR output (DXGI only, `port/fast3d/gfx_hdr_dxgi.cpp:18`), suspend points
  (unavailable in every 64-bit build), the sampling profiler
  (`port/src/port_profile.c`, Windows code).
- Making the Mac a machine that regenerates `review/hand-gallery`.
- The 64-bit plan's open Linux and Windows items (its hand-over list).

## Anti-goals

These would make the project a failure even if a Mac build runs:

- A build that exists only on a branch when the plan closes.
- A download a player must override Gatekeeper to open.
- Signing material, ROM-derived data or generated game data in Git.
- A strict-tier file committed to `main` from the Mac.
- A change to a shipping build's maths, pools or timing made to suit macOS
  without the owner's say.
- Code copied from upstream's macOS pull requests (#42, #95). They are read
  for approach only; the 64-bit plan decided to write our own.

## Constraints

- **Strict tier.** Every file under `port/fast3d/` and the 16 files of
  `STRICT_FILES` (`scripts/port/hand_gallery.py`) needs a fresh, reviewed
  gallery. The Mac's clone has `midas.galleryDefer` set, so it may commit
  such a change on a branch other than `main`; the branch reaches `main`
  through a gallery regeneration on the owner's Windows laptop. A headless
  build compiles nothing under `port/fast3d/` (`CMakeLists.txt:582-644` adds
  those files only under `GE_VIDEO`). `port/src/main.c`, `CMakeLists.txt`,
  `port/crash/` and the headers under `include/` and `port/include/` are
  not strict tier.
- **The ROM** is in the checkout root, ignored by Git, used through
  `GE_ROM`. CI has no ROM, so every check that plays the game runs on the
  Mac.
- **Signing.** The Mac has one "Apple Development" identity and no Developer
  ID (`security find-identity -v -p codesigning`). Developer ID and
  notarization need the paid Apple Developer Program.
- **OpenGL.** macOS gives 4.1 core and 2.1 legacy and nothing else. Apple
  deprecated OpenGL in 2018 and still ships it in the macOS 27.2 SDK.
- **Recorded decisions stand:** no App Store distribution; widen pointers;
  write our own; no suspend points at 64 bits; a shipping build's maths does
  not change without the owner's say; the two-tier gallery rule.

## What was measured

On 2026-11-26, `main` at 843c143, on the owner's Mac: macOS 27.2.1, Apple M4
Max, Xcode 27.2 (Apple clang 21.0.0, `arm64-apple-darwin27.2.0`), CMake
3.27.9, Ninja, Homebrew SDL2 2.32.10. Scratch trees outside the repository;
nothing in it was changed.

1. **Apple clang nearly compiles the headless game.** `cmake -G Ninja
   -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON` configures
   (no `APPLE` branch exists; it takes the generic 64-bit branch). `cmake
   --build ... -- -k 0`: 61 of 361 compile steps fail and the link is not
   reached. Six causes:
   - `include/PR/os.h:1019-1033` redeclares `bcopy`, `bcmp`, `bzero` and
     `sprintf`, which the macOS SDK defines as checked-builtin macros (52
     files, plus a local `extern int sprintf` at `src/game/image.c:2690`);
   - `port/include/asmsym.h:29-31` emits ELF section names in top-level
     assembly (`ramrom_data.c`, `port_weapon_images.c`, `segments.c`,
     `ucode_dl_stubs.c`: "unexpected token in '.section' directive");
   - `port/src/port_lp64.c:29-33` is a deliberate `#error` off Linux: the
     executable's address range comes from `dl_iterate_phdr`;
   - `port/src/pcsched.c:1074,1083` uses `clock_nanosleep` with
     `TIMER_ABSTIME`;
   - `port/src/romdata.c:87` uses `__errno_location` (at `:195` and `:199`);
   - `src/game/lv.c:1017-1020` and `src/game/spectrum.c:272` declare one
     assembly name twice ("reference to ... is ambiguous").

   Every command also warns `unknown warning option
   '-Wno-discarded-qualifiers'` (`CMakeLists.txt:51-85` names GCC's classes).
2. **The video build fails in 14 more files.** With `-DGE_VIDEO=ON
   -DSDL2_LINUX_ROOT=/opt/homebrew`, 75 of 393 steps fail. Seven `port/src`
   files (`audio_pc.c`, `input_pc.c`, `video.c`, `raphnet.c`,
   `port_music_export.c`, `port_projectile_arc_audit.c`,
   `port_state_effects_audit.c`) fail on the same `os.h` lines and nothing
   else. Seven renderer files (`about.cpp`, `gfx_cc.cpp`, `gfx_opengl.cpp`,
   `gfx_pc.cpp`, `gfx_shiny.cpp`, `hd_textures.cpp`, `weapon_cache.cpp`)
   fail on `fabsf`, `acosf` and `expf` undeclared, unresolved
   using-declarations and an ambiguous `to_string`: the game's headers are
   found in front of libc++'s. A video build takes the Linux link line
   (`CMakeLists.txt:777-792`), and `-Wl,-Map=` (`:708`) is GNU ld's.
3. **The renderer already has the macOS path, never yet run.** A throwaway
   SDL2 probe got "4.1 Metal - 91.7", GLSL 4.10, renderer "Apple M4 Max" for
   a 4.1, 3.3 or 3.2 core request, and no context for 3.0 or 3.3
   compatibility, 4.3 or 4.6. An ES 3.0 request fails ("Could not initialize
   OpenGL / GLES library"; no ANGLE is installed). Maximum texture size
   16384, 4 samples; `KHR_debug`, `ARB_clip_control`, `ARB_buffer_storage`
   and `ARB_copy_image` are absent. `port/fast3d/gfx_sdl2.cpp:404-415` asks
   for 3.0 compatibility and then 4.1 core; `gfx_opengl.cpp:1797-1816` writes
   `#version 410 core`; `glad/glad.h:1-34` is generated for GL 4.1 core. A
   survey of the renderer found no unguarded call past 4.1 and no
   compatibility-only call. No shipped build has run that path: native
   builds get 3.0 compatibility first and the browser uses ES.
4. **Addresses.** A 64-bit build stops if a host address is below 4 GB. A
   macOS arm64 executable has a 4 GB `__PAGEZERO` (`otool -l /bin/ls`), so
   nothing maps below it.
5. **What stops working on macOS as the code is:**
   - `port/fast3d/save_transfer.cpp:141-146` reads `/proc/self/exe` and
     throws; `port/src/main.c:780-781` then returns 1, so the game would
     exit at startup;
   - crash reporting is Windows and Linux only
     (`scripts/port/sentry.cmake:4`, `port/crash/reporting_win.c`,
     `reporting_linux.c`; Linux builds sentry-native with
     `-DSENTRY_BACKEND=native`, `scripts/port/linux_build.py:71`); the plain
     signal handler (`main.c:821-831`) is portable;
   - downloads use WinHTTP or `dlopen("libcurl.so.4")`
     (`port/fast3d/enhanced_fonts_http.cpp:149-160`), else an empty
     transport. macOS has a system `libcurl.4.dylib` (loaded with Python's
     `ctypes`: libcurl 8.7.1);
   - the weapon icon worker is compiled for `_WIN32 || __linux__`
     (`port/fast3d/weapon_worker.cpp:4`, `weapon_cache.cpp:361`);
   - file pickers run zenity or kdialog
     (`port/fast3d/posix_dialog.cpp:51-100`);
   - the data root is the executable's folder, found through
     `/proc/self/exe` with `argv[0]` as the fallback (`main.c:621-639`). An
     absolute `GE_DATA_ROOT` already overrides it on every build that is not
     Windows or the browser (`main.c:627-630`). The ROM is looked for in
     `<root>/Data` (`port/src/romdata.c:17-23`). The AppImage's launcher
     script, not the game, makes the folder and copies four default files
     into it (`scripts/port/linux/AppRun`).
6. **Risks read in the code, not yet run:**
   - the render thread (`port/src/video.c:5570-5669`), on at the shipped 120
     Hz tick, takes the GL context and swaps buffers off the main thread;
   - HiDPI is switched off (`gfx_sdl2.cpp:289-300`). The probe's 320x240
     window had a 640x480 drawable when HiDPI was allowed, so today a Retina
     display would be drawn at half its resolution and scaled up;
   - borderless fullscreen is a display-sized window
     (`gfx_sdl2.cpp:219-243`), which leaves the macOS menu bar and Dock;
   - macOS arm64 pages are 16 KB and a bad access raises SIGBUS where Linux
     raises SIGSEGV (FORK-089); the tree has only run on 4 KB pages.
7. **Tools.** `lp64_boot.py`, `lp64_play.py`, `lp64_equiv.py` and
   `audit_buffers.py` are Python with no container in them. Their `--video`
   modes set `SDL_VIDEODRIVER=offscreen`; the Mac's SDL lists that driver,
   but under it the probe got no GL window for any version, so picture
   checks on the Mac need a real window. There are 46
   `scripts/port/test_*.sh` harnesses; the last count on macOS was 10 of 33,
   during LP64-005 (`native_64bit_lp64_005.md:265`). `package_linux.py`
   reads ELF only; `make_brand_art.py` has no `.icns` output; no workflow
   names macOS (`grep -rn macos .github/` is empty).
8. **Prior art.** Upstream's open pull request murkantor/Project-Midas#95
   changes the same places (`asmsym.h`, `main.c`, `pcsched.c`, `romdata.c`,
   `lv.c`, `spectrum.c`, `gfx_sdl2.cpp`, `CMakeLists.txt`) and reports
   playing on an M4 Pro with macOS 27.2, shipped as an ad-hoc signed
   universal zip.

## Approaches

### Graphics

| Approach | Cost | Verdict |
|---|---|---|
| OpenGL 4.1 core as it is | No new renderer code: the context list, shaders and loader already reach it. Deprecated by Apple; does not carry to tvOS, which has no desktop OpenGL. | **Chosen by the owner.** |
| ANGLE (OpenGL ES 3 over Metal) | The ES 3 path only the browser build uses today; two ANGLE libraries to build, pin, bundle and sign. Not measured: which fork features the ES path lacks, and whether ANGLE builds for tvOS. | Fallback, if Apple's GL has a defect that cannot be worked around or is removed. It is one more entry in the context list; nothing else in this plan changes. |
| A new backend (Metal, or Vulkan through MoltenVK) | About 5,740 lines of GL rewritten behind `struct GfxRenderingAPI` (`port/fast3d/gfx_rendering_api.h:35`), all strict tier. No Vulkan backend exists. | Rejected. |

### Who it is for

| Approach | Cost | Verdict |
|---|---|---|
| Self-build only | No packager, signing or CI. | The early milestones. |
| Ad-hoc signed zip | Players clear the quarantine by hand. | Rejected. |
| Developer ID signed, notarized download | The paid membership, signing secrets in CI, player data outside the app. | **Chosen by the owner.** |

### Order of work

| Approach | Cost | Verdict |
|---|---|---|
| Window first | Every early step is a strict-tier branch. | Rejected. |
| Headless first, then the window | One milestone without a picture. | **Chosen:** it lands on `main` from the Mac and proves the simulation before graphics are in question. |

## Design

### Build

`CMakeLists.txt` gains an `APPLE` branch: arm64 only, 64-bit pointers only,
clang's names for the warning classes, the same `-Werror=` pointer checks
and `-ffp-contract=off` as the 64-bit Linux build. Each of the six compile
causes is fixed so the other builds compile the lines they compile today.
Fixes in upstream-owned files (`include/PR/os.h`, `src/game/*.c`) are the
smallest that work. `port_weapon_images.c` is strict tier, so the section
names are fixed in `asmsym.h`. Where a renderer file fails to compile, a fix
outside `port/fast3d/` (an include path, a flag) is preferred to an edit
inside it.

### Addresses and timing

`port_lp64.c` learns the executable's range from dyld. The frame pacing
gets a macOS absolute sleep in place of `clock_nanosleep`. The crash handler
and the harnesses treat SIGBUS as they treat SIGSEGV. Anything that assumed
a 4 KB page is found by running the play sets and the sanitizer build, not
by reading.

### Maths

The comparison build replaces `tanf`, `atanf` and `cosf` with in-tree
versions (`port/src/port_compare_math.c`); the others were swept only on
glibc (`scripts/port/lp64_libm_sweep.c`). MAC-001 runs the sweep on macOS,
and a function that differs there joins the comparison build's in-tree set.
A shipped build keeps the system's maths.

### Landing

Changes outside the strict tier land on `main` from the Mac. Strict-tier
changes are committed on one branch, kept rebased on `main`. The window,
picture and platform checks run at that branch's tip, and later milestones
may be worked there before it lands. The owner lands it from the laptop when
a session allows: at least once before MAC-006's release, expected after
MAC-002 and after MAC-003. A milestone closes only when its strict-tier part
is on `main`, and the plan closes with nothing left on the branch.

Every edit under `port/fast3d/` is either compiled only on macOS or shown to
leave the Windows build's behaviour as it is. An edit that can be neither
also owes the laptop session the before-and-after benchmark `AGENTS.md`
asks for.

### Window and picture

SDL2 with Apple's OpenGL 4.1 core, through the context list as it is. On
macOS the window allows HiDPI and the framebuffer is sized from the
drawable, so a Retina display is drawn at its own resolution. Fullscreen
uses SDL's desktop fullscreen, which hides the menu bar and Dock. The render
thread is kept if a run at 120 Hz with resizing, fullscreen changes and a
display change shows no fault; if it cannot be made sound, macOS runs
without it and the status table records the measured cost. Sound
(`port/src/audio_pc.c:767-853`, SDL's queue at 22,050 Hz stereo) and input
are SDL's, unchanged.

### Player data

Run from an app bundle, the game's root is
`~/Library/Application Support/Iron Sights` (the ROM in `Data` there, saves,
`midas.ini`, logs), as the AppImage uses its own folder (FORK-031). Run from
a build tree it stays beside the executable. `GE_DATA_ROOT` overrides both.
With no ROM, the message box names the folder and the game offers to show it
in Finder.

The game itself makes the folder and copies the packaged defaults on first
run, since an app bundle has no launcher script. It refreshes
`shaders_base.txt` when the packaged one is newer, so it does not repeat the
AppImage's seed-once defect the 64-bit plan keeps deferred. The folder's
path has a space in it, which no Linux package's has; every path the game
hands to a helper process is checked for that.

### Platform features

Downloads load the system `libcurl.4.dylib`. The weapon icon worker takes
the Linux path. File pickers keep the helper-process shape of
`posix_dialog.cpp`, with `/usr/bin/osascript` as the helper.
`save_transfer.cpp` finds the executable through the system call for it.
The four defects the 64-bit plan's final review recorded in
`save_transfer.cpp` and `posix_dialog.cpp`
([its hand-over](native_64bit_plan.md#after-the-64-bit-plan)) are fixed in
the same landing, since it edits both files.

### Package

`Iron Sights.app` holds the executable, SDL2 built from the pinned 2.30.11
source the Linux packages use (`scripts/port/linux_build.py:20-21`), the
shader list and notices, an `.icns` that `make_brand_art.py` generates
(with `brand-art.sha256`), and an `Info.plist` filled from the build
information. A `package_macos.py` beside `package_linux.py` builds it,
checks it (arm64 only, links only system and bundled libraries, never a
comparison build, no ROM-derived file), and writes
`Iron-Sights-macos-arm64.dmg` with its sums and build-information files.

### Crash reporting

`scripts/port/sentry.cmake` and `port/crash/` gain macOS, with the consent
flow as it is and symbols uploaded as on the other platforms. A macOS list
of crash cases runs on the real package: the cases of
`scripts/port/test_sentry_linux.py` that are not about the AppImage or the
Steam Deck zip, and one for the app bundle.

### Signing and release

The app, its SDL2 and any crash handler are signed with the owner's
Developer ID under the hardened runtime, notarized and stapled. A
`macos-arm64` job in `.github/workflows/windows-release.yml` on GitHub's
Apple Silicon runner builds, packages, signs and notarizes on a push to
`main`, and the release attaches and attests the download like the others.
The `publish` job does not need the macOS job: a signing or notarization
failure, or an Apple outage, fails the workflow run visibly but never holds
back the Windows and Linux packages. Secrets live in GitHub, never in the
repository.

## Verification

Each item names what it proves. All game runs are on the Mac, with the ROM
through `GE_ROM`; their output is ROM-derived and stays out of Git.

1. **It compiles under the 64-bit rules.** A headless macOS tree (MAC-001)
   and a video one (MAC-002) pass `lp64_diagnostics.py gate`, and the
   narrowing and unprototyped checks run on them. This is the 64-bit plan's
   hand-over item "the aarch64 gate on macOS clang".
2. **It boots.** `lp64_boot.py --expect-run --frames 600` prints `OK: ran
   600 frames`, and the log has `ge: lp64: address space OK`.
3. **It plays.** `lp64_play.py run --set
   demos,stages,stops,menus,tours,attract` and `audit_buffers.py
   --mission-smoke` (20 of 20), headless (MAC-001), then in a window
   (MAC-002).
4. **It is the same game.** `lp64_equiv.py run` on a macOS comparison build:
   38 of 38 cases ran and agree with themselves, and `compare --streams sim`
   against the aarch64 Linux comparison build passes. A difference is listed
   in `lp64_equiv_expected.txt` with its first tick and cause, and with no
   hash pin unless the owner allows one for it.
5. **Memory is sound.** A clang sanitizer build (`GE_SANITIZE`) runs the
   boot check and the play sets with no report.
6. **It draws the same.** The picture digest (`CASE.gfx`) of a macOS video
   comparison build against the aarch64 Linux one is equal, or each
   difference is explained. Screenshots (`GE_SHOT`) of the cases are
   reviewed by eye.
7. **It sounds right.** The `sound` set's recordings pass on macOS.
8. **The harnesses hold.** Each of the 46 `scripts/port/test_*.sh` passes on
   macOS at 64 bits or is named with its reason.
9. **The other builds did not change.** CI passes, and the x86_64 and
   aarch64 Linux play sets and comparison pass again after each milestone
   that touches shared code. The 32-bit Windows program is unchanged or
   every changed function is named.
10. **It costs what it should.** A change to per-frame work gets the smoke
    benchmark and profile `AGENTS.md` asks for, and the late-load audit runs
    on macOS for the stages.
11. **The package is the game.** The packaged app boots, runs the mission
    smoke with its data in a folder whose path has a space, and sends the
    macOS crash cases.
12. **A player can open it.** `spctl` and `stapler validate` pass on CI's
    output, and the owner downloads the release in a browser and opens it
    with a double click.
13. **A person played it.** The owner plays a mission to its end with a
    gamepad and with mouse-look, in a window and fullscreen, on the built-in
    display and an external one. The session's log goes through
    `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt`; a pool past
    half its size is a finding for the owner, not a pool this plan resizes.

### What no item above exercises

The cases most likely to meet a player that the list does not run. Each
milestone plan gives its own a check or records why not.

- The app is opened from the mounted disk image or the Downloads folder,
  where macOS runs it from a read-only, randomised path (MAC-004).
- The Mac sleeps and wakes, or a display is unplugged, while the game holds
  its GL context (MAC-002).
- The sound device changes during play, such as headphones connecting
  (MAC-002).
- A campaign save and `midas.ini` copied from a Windows install into the
  player's folder (MAC-003).
- A user name or folder with non-ASCII characters in the data root's path
  (MAC-003).

## Working machine

The owner's Apple Silicon Mac, as in the 64-bit plan, now running the game
itself as well as its containers.

- **Native macOS** is the build under test: Apple clang, the Mac's GPU, a
  real window. No Linux-only tool (`lp64_env.py`, the Docker images) is
  needed to build or run it.
- **The Linux containers** still supply the reference: the aarch64
  comparison build runs natively in its container and is what the macOS
  build is compared with; x86_64 runs under Rosetta 2.
- **The owner's Windows laptop** regenerates the gallery and lands the
  strict-tier branch.
- **CI** has no ROM and no GPU; from MAC-006 its Apple Silicon runner
  builds, packages and signs.

## Milestones

Each milestone gets its own detailed plan before it starts, accepted by the
owner, as LP64-003 to LP64-007 did.

1. **MAC-001 Headless on Apple clang.** The `APPLE` branch, the six compile
   causes, the executable's range, the sleep, SIGBUS, the maths sweep, the
   harnesses and the gate on clang. No strict-tier file. Done when, on
   `main`, a headless 64-bit macOS tree builds with Apple clang and passes
   verification 2, 4, 5 and 8 and the headless halves of 1 and 3, and
   verification 9 holds.
2. **MAC-002 Window, picture, sound and input.** The video tree builds and
   links SDL2 and Apple's OpenGL; the executable is found without
   `/proc/self/exe`, so the game starts; Retina resolution, desktop
   fullscreen, the render thread decision; the `--video` tools run in a real
   window. Done when the video halves of verification 1 and 3, and 6, 7, 10
   and 13 hold at the strict branch's tip, verification 9 holds, and the
   branch is on `main` through the laptop.
3. **MAC-003 Platform features.** The player's folder and its first-run
   defaults, the no-ROM message, downloads, the weapon icon worker, the file
   pickers, the four recorded defects. Done when each works on macOS from a
   data root whose path has a space, verification 9 holds, and the branch is
   on `main` through the laptop.
4. **MAC-004 App bundle and packager.** `Iron Sights.app`, the generated
   `.icns`, the pinned SDL2 build, `package_macos.py` and its checks, an
   ad-hoc signature for local use. Done when the packaged app passes
   verification 11 without the crash cases.
5. **MAC-005 Crash reporting.** Sentry on macOS and its symbols. Done when
   the packaged app sends the macOS crash cases of verification 11.
6. **MAC-006 Signing, release and hand-over.** Developer ID signing,
   notarization, the `macos-arm64` CI job, the release, a developer guide
   beside `linux_64bit.md`, the README's download, and the Apple TV
   hand-over. Done when one push to `main` publishes the notarized download
   beside the other packages, attested; verification 12 holds; nothing is
   left on the strict branch; and the status table says Done with the
   hand-over written.

Every milestone leaves the 32-bit, web and 64-bit Linux builds passing their
existing checks, updates `FORK_CHANGES.md` with each change, and updates the
status table before handing off.

## Decisions

Owner's decisions:

- The first macOS build is a Developer ID signed, notarized download
  (2026-11-26). Self-build on the owner's Mac comes first, as the early
  milestones.
- Graphics are OpenGL 4.1 core as it is (2026-11-26). ANGLE over Metal is
  the fallback, and the choice of Apple TV's graphics stays with that
  project.
- Six milestones, Apple Silicon only (2026-11-26).
- The design, with the calls below (2026-11-27).

Calls made in drafting, which the owner accepted with the design and can
still correct:

- Headless first.
- Strict-tier work sits on one branch that later milestones may build on,
  expected to land twice (after MAC-002 and after MAC-003); a milestone
  closes when its strict-tier part is on `main`.
- Player data is in `~/Library/Application Support/Iron Sights` when the
  game runs from a bundle, and the game, not a launcher script, makes that
  folder and copies the packaged defaults.
- The same features as the Linux 64-bit packages; no HDR, suspend points or
  profiler.
- The download is a disk image, `Iron-Sights-macos-arm64.dmg`.
- CI signs and notarizes; the owner creates the certificate and key.
- A macOS signing or notarization failure does not hold back the Windows
  and Linux release.
- SDL2 is the 2.30.11 the Linux packages pin.
- File pickers go through `osascript`, not a new Objective-C file.
- MAC-003 also fixes the four recorded strict-tier defects in the two files
  it edits.
- The macOS build joins the comparison as a fourth build, compared with
  aarch64 Linux; its expected-difference lines carry no hash pin.
- A maths function that differs on macOS joins the comparison build's
  in-tree set; shipped maths is untouched.
- The plan lives here, with the status table below, a pointer from
  `AGENTS.md` and a `FORK_CHANGES.md` entry (FORK-093).

## Left to the builder

- How each compile cause is fixed, and the dyld and sleep calls used.
- Where macOS build trees live under `build/`.
- How the `--video` tools run in a real window on the Mac.
- Whether the context list starts at its core entry on macOS.
- The sentry-native backend on macOS and how its symbols are made.
- The deployment target: the oldest macOS a check can run on (the Mac and
  GitHub's runners); nothing older is claimed.
- Whether `lp64_lto.py` has a clang equivalent; if not, the GCC check in the
  containers stays the check (the 64-bit plan's hand-over item).
- Whether `bench.py` and `late_load_audit.py` run on macOS as they are.
- The app menu and keyboard conventions (Command-Q, fullscreen).

## Needed from the owner

- **Now:** start the Apple Developer Program enrolment. It can take days,
  and the plan cannot close without it.
- **Before MAC-004:** the app's bundle identifier. It goes in `Info.plist`
  and names the app to macOS (its signature, permissions and preferences),
  so it should not change after a release.
- **Before MAC-006:** a Developer ID Application certificate and a
  notarization key, stored as GitHub secrets.
- **After MAC-002 and MAC-003:** a laptop session each, to regenerate the
  gallery and land the branch. The first also inherits the gallery review
  the 64-bit plan left pending (item 4 of its hand-over's laptop list).
- **During MAC-002:** the play session of verification 13.

Questions the milestone plans bring to the owner:

- MAC-002: whether the render thread stays on macOS, once measured.
- MAC-003: whether the About dialog names the platform and architecture.
  The 64-bit plan left it because `about.cpp` is strict tier, and MAC-003 is
  its cheapest landing.
- MAC-006: the oldest macOS the download claims to support.

## Milestone status

Update this table before handing off. Record evidence in the notes; do not
mark a milestone done without it.

| ID | Status | Notes |
|---|---|---|
| MAC-001 | Not started | Next: its detailed plan. Measured starting point: 61 of 361 headless compile steps fail under Apple clang, from the six causes in "What was measured". |
| MAC-002 | Not started | Needs MAC-001. Strict tier: lands through the laptop. |
| MAC-003 | Not started | Needs MAC-002. Strict tier: lands through the laptop. |
| MAC-004 | Not started | Needs the bundle identifier from the owner. |
| MAC-005 | Not started | |
| MAC-006 | Not started | Needs the Apple Developer Program membership, the certificate and the notarization key from the owner. |
