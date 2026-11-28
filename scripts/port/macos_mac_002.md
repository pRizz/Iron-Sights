# MAC-002 Window, Picture, Sound and Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-27 from measurements on `main` at 567251f
and accepted by the owner on 2026-11-28 with all five recommendations
("Accept all five"); "Decisions for the owner" at the end lists them, and
the tasks are written for them. Nothing below is built yet: every
measurement was taken on a scratch copy outside the repository.

**Goal:** The 64-bit game runs in a window on Apple Silicon macOS, drawn by
the Mac's GPU through Apple's OpenGL 4.1, with sound, keyboard, mouse and
gamepad; its pictures, simulation and sound match the aarch64 Linux
build's; and its strict-tier part sits on one branch, ready for the laptop
session that lands it.

**Architecture:** The measurements below show the renderer's macOS path
works as written. Five small edits make the video tree compile, link,
start, draw every mission and match aarch64 Linux in all 38 comparison
cases: two outside the strict tier (a header and the link line) and three
inside it (a cast, the executable's path, and a switch that keeps the
window hidden for the tools). So the milestone is not a port of the
renderer. It is: put each edit on the right side of the strict
line, make the tools run the game in a window that never takes the
owner's screen, and then settle what only a real Mac window raises:
Retina, fullscreen, the render thread, pacing and cost. macOS-only code
keeps living in `port/src/port_macos.c`; every edit to a shared file sits
behind `__APPLE__`.

**Tech Stack:** C and C++ (Apple clang 21, Xcode 27.2), SDL2 (Homebrew's
2.32.10 for now; MAC-004 pins 2.30.11), Apple's OpenGL 4.1 core profile,
CMake 3.27 with Ninja, Python 3 standard library, Bash harnesses, the
Linux containers (`lp64_env.py`) for the reference builds and the Windows
comparison.

**Spec:** [macos_plan.md](macos_plan.md): "Design" (Build, Landing, Window
and picture), verification items 1, 3, 6, 7, 9, 10 and 13, the three
MAC-002 lines of "What no item above exercises", milestone MAC-002 and
"Left to the builder". Read it first, then `AGENTS.md` (render
performance, load before play, input idioms, local build artifacts, the
macOS paragraph), [the macOS guide](macos.md) and
[MAC-001's plan](macos_mac_001.md) for how the headless build is checked.

## What was measured for this plan (2026-11-27, `main` at 567251f)

On the owner's Mac (macOS 27.2.1, Apple M4 Max, built-in display 1728x1117
points at 120 Hz, Apple clang 21.0.0, Homebrew SDL2 2.32.10). A copy of the
tree (`git archive HEAD`) was edited and built in a scratch folder outside
the repository and then discarded; nothing below is on `main` or on any
branch. The ROM was read through `GE_ROM`. About ten of the runs opened a
window on the owner's screen. The first three did for a few seconds each
(one of them fullscreen: `GE_WINDOW` sets only the windowed size, and a
new `midas.ini` starts borderless). About seven later ones showed a
640x480 window for 20 to 25 seconds each, because they measured what only
a shown window has: the pacing runs at both tick rates, the Main Thread
Checker run, a Retina run and two same-frame screenshot runs. The play
sets, the mission smoke, the comparison and the benchmark ran hidden.

- **The video tree fails in seven files, for one cause, and then in one
  line.** `cmake -G Ninja -DGE_POINTER_BITS=64 -DGE_VIDEO=ON
  -DSDL2_LINUX_ROOT=/opt/homebrew` configures; 7 of 396 steps fail, all
  under `port/fast3d/` (`about.cpp`, `gfx_cc.cpp`, `gfx_opengl.cpp`,
  `gfx_pc.cpp`, `gfx_shiny.cpp`, `hd_textures.cpp`, `weapon_cache.cpp`):
  libc++'s `<cmath>` includes `<math.h>` expecting its own wrapper and
  finds the game's `include/math.h`, which `-I include` puts first. Three
  lines at the top of `include/math.h` (`#include_next <math.h>` for
  `__APPLE__` C++, as `include/string.h` and `include/stdlib.h` already do
  for every hosted C++ build) clear six of the seven. The seventh is
  `port/fast3d/hd_textures.cpp:252`: `std::to_string` of a file time's
  count is ambiguous on libc++, whose file clock counts in `__int128`; a
  `static_cast<long long>` clears it. That file is strict tier.
- **The link needs its own line.** The video branch
  (`CMakeLists.txt:908-923`) names `SDL2 GL dl pthread`; macOS has no
  `libGL`. Linking `SDL2` alone succeeds (glad resolves every GL function
  through SDL), and the link check prints `OK: 702 weak references, 0
  strong` (the headless build's 705 less the three video-only functions,
  now defined). Two new warning classes appear in C++: `macro-redefined`
  13 times (the game's `M_PI` family after the system's) and
  `null-character` once (`port/fast3d/gfx_sdl2.cpp:711`).
- **The gate passes on the video tree.** `lp64_diagnostics.py gate`: `393
  files compiled, 0 failed`. `narrowing --check`: `OK: 33 narrowings in 19
  files, all allowed`, the headless figure: the video-only files add none.
- **It exits at startup, as the plan read.** `ge: save transfer: Cannot
  locate Iron Sights for restart.`, exit status 1:
  `port/fast3d/save_transfer.cpp:143` reads `/proc/self/exe`. With the
  path taken from `_NSGetExecutablePath` instead, the game starts.
  `port/src/main.c:727` reads the same link for the data root and falls
  back to `argv[0]`, which worked because the runs gave a full path.
- **It draws.** The log shows `SDL: could not create GL3.0 context`, then
  `SDL: created GL4.1core context`, `GL: version: 4.1 Metal - 91.7`,
  `renderer: Apple M4 Max`, `using GLSL version 410 core`, `up to 4x
  MSAA`, and all 89 programs of `shaders_base.txt` compiled ahead. Dam at
  640x480, read by eye from `GE_SHOT` screenshots: the title card, the
  sky, the level, the gun and hand, the HUD and the watch arcs are right.
  Three things in the log are not:
  - two lines `Failed to link the retained program (...): ERROR: Input of
    fragment shader 'vGeSkyQ' not written by vertex shader`: the retained
    twin (`port/fast3d/gfx_opengl.cpp:2403-2564`) of the two sky programs.
    The compatibility profile lets a fragment input go unwritten; the core
    profile refuses the link. The game then skips retained draws of those
    programs, which the sky never makes, so the picture is unaffected;
  - 36 lines `GL: could not find function: glVertexP2ui` and its family
    (`gfx_opengl.cpp:1705`): glad's 4.1 loader asks for the packed-vertex
    entry points, which Apple's core context does not export and the game
    never calls;
  - the failed 3.0 request itself, which creates and destroys a window
    before the 4.1 one (`gfx_sdl2.cpp:404-465`).
  None of the three appears in the aarch64 Linux video logs.
- **A hidden window is a real one.** A throwaway SDL2 probe created its
  window with `SDL_WINDOW_HIDDEN`, never showed it, and got "4.1 Metal -
  91.7", a complete default framebuffer and correct `glReadPixels` from it
  and from a framebuffer object (with HiDPI allowed, a 640x480 drawable for
  a 320x240 window). Under `SDL_VIDEODRIVER=offscreen` there is still no
  GL window. The game already creates its window hidden
  (`gfx_sdl2.cpp:340`) and shows it at `:513` unless
  `GE_WEAPON_ICON_EXPORT` is set. With one more variable tested on that
  line, Dam ran 1,500 frames hidden at 60 retraces a second in 25 s, drew
  1,488 pictures, wrote a correct screenshot and a 24.4 s recording
  (22,050 Hz stereo, peak 15,200), and nothing appeared on screen.
- **The tools need one branch more.** `lp64_play_run.py:64-68` picks
  `windows` on Windows and `offscreen` elsewhere (WIN64-002 added the
  first); with `cocoa` and the hidden variable on macOS,
  `audit_buffers.py`, `lp64_play.py` and `lp64_equiv.py` run unchanged.
  `lp64_equiv_pictures.py:179` names `offscreen` on its own.
- **The play sets pass in hidden windows.** `lp64_play.py run --video
  --set demos,stages,stops,menus,tours,attract,sound`: `OK: 119 of 119
  cases` in 16 minutes (the 117 headless cases, each now with its
  pictures, and the two sound cases); `peaks --budget`: `OK: 318 pool and
  stage pairs within budget`; the two recordings are 27.87 s at 22,050 Hz
  stereo, peaking at 16,050 and 23,573. `audit_buffers.py --mission-smoke
  --video --frames 1800 --jobs 4`: 20 of 20 `PASS` in two and a half
  minutes. Nothing appeared on screen.
- **Pictures, simulation and sound match aarch64 Linux.** A macOS video
  comparison build (`-DGE_COMPARE=ON -DGE_VIDEO=ON`) ran `lp64_equiv.py
  run --video --set demos,solo,mp,menus,reload --repeat 2` hidden: `OK: 38
  of 38 cases ran`, `OK: 38 of 38 cases agree with themselves`. Against
  `build/lp64/cmp-a64-video`, rebuilt at 567251f and run the same way in
  its container: `compare --streams sim`: `38 streams, 38 identical`;
  `--streams gfx` (the picture digest): `38 streams, 38 identical`;
  `--streams snd`: `24 streams, 24 identical` (a demo's sound is never
  judged). So what the game asks the renderer to draw is the same on
  Apple's 4.1 core context as on Mesa's 3.0 compatibility one. The drawn
  pixels were not compared: the two rasterizers differ.
- **The sanitizer video build runs.** `-DGE_SANITIZE=ON -DGE_VIDEO=ON`
  builds, and Dam ran 1,500 frames in a window with no report under
  MAC-001's decision 2 checks. The play sets were not run on it.
- **Retina is off, and switching it on takes two edits.**
  `GfxWindowInitSettings.allow_hidpi`
  (`port/fast3d/gfx_window_manager_api.h:19`) is never set
  (`port/src/video.c:5061` zeroes the struct), so `gfx_sdl2.cpp:288-292`
  disables HiDPI and the log shows `drawable 1728x1117` on a panel of
  3,456x2,234 pixels. Setting the field is not enough:
  `gfx_sdl2.cpp:396-400` adds the window flag inside `#ifdef
  SDL_WINDOW_ALLOW_HIGHDPI`, and that name is an enumerator, not a macro,
  so no build has ever added it. With the field set and the flag added, a
  640x480 window has a 1,280x960 drawable and its screenshot shows the
  whole picture at that size: the renderer already sizes its framebuffer
  from the drawable each frame. A hidden 1,728x1,117 window drew Dam at
  3,456x2,234 and held 120 pictures a second, with 4.1 ms of GL a frame
  against 3.4 at one pixel a point. The pointer was not checked: the
  cursor's clamp, the mouse position and the warp are in points
  (`port/src/input_pc.c:8494`, `:8640`, `:8686`, `:8805`, `:8826`), and
  `video.c:575-581` compares a mouse position with a width in pixels.
- **Borderless is a display-sized window.** `gfx_sdl2.cpp:219-243` sizes a
  borderless window to the display bounds and never passes
  `SDL_WINDOW_FULLSCREEN_DESKTOP` to SDL (`:36` keeps it as a marker
  only), so the menu bar and Dock are not hidden by the system.
- **The render thread runs, and makes window calls off the main thread.**
  At the shipped 120 Hz tick it starts (`ge: video: render thread ON
  (auto; tickhz=120)`), drew 300 frames borderless and stopped cleanly.
  Read in the code: every frame it calls `SDL_GL_GetDrawableSize` and
  `SDL_GetWindowPosition` (`port/fast3d/gfx_pc.cpp:9071` through
  `gfx_sdl2.cpp:615-616`) and `SDL_GL_SwapWindow` (`:1068`), while the
  main thread pumps events, and on the first window-moved event the main
  thread calls `glGetString` with no context current
  (`gfx_sdl2.cpp:702-709`). AppKit expects window queries on the main
  thread. Measured: 20 s of Dam at 120 Hz in a shown 640x480 window with
  Xcode's Main Thread Checker loaded made no report, held 118 to 120
  pictures a second and logged one hitch, Dam's first frame. With
  `GE_RENDERTHREAD=0` it also held 120, and the main thread's idle share
  fell from 97% to 1%. No resize, fullscreen change or display change was
  made in any run.
- **Pacing.** At the shipped 120 Hz tick in a shown window with vsync on:
  120 pictures a second and one `ge: hitch:` line, the stage's first frame
  (115 to 133 ms). At `GE_TICKHZ=60`, which the tools set, in a shown
  window on the 120 Hz panel: 60 pictures a second on average, but 66
  hitch lines in 20 s of Dam, 50 of them frames of 25 ms (three retraces
  where two were due); hidden, 10. A player does not have this by default,
  but `tickhz=60` is a key a player can set.
- **A late load at the first frame.** `ge: late load: uploads 9.8 ms
  during play (stage 90, tick 4)`, of which `mips 9.5`: the first mipmap
  generation of the run costs Apple's driver about 10 ms. Dam's first
  frame logged a 375 ms hitch (`render: gl 289.5`).
- **The cost tools run on macOS as they are.** `late_load_audit.py --exe
  ... --stage dam --stage runway --no-pack`, hidden: both toured, `Frames
  over budget` 0 for both, no shader compiled during play. `bench.py run
  --smoke --repeats 1`, hidden: `dam-walk` averaged 1,066 pictures a
  second (render 0.47 ms), `cradle-fight` 705, `temple-4p` 705, in both
  presets' tables.
- **A screenshot wider than 4,096 pixels overruns its row buffer.**
  `gfx_sdl2.cpp:827` copies each row into `uint8_t row[4096 * 3]` with no
  check of the drawable's width. This is in every build today; Retina
  makes it reachable on a Mac with a 5K or 6K display. It was read, not
  run: the game clamps a window to the display, and this Mac's is 3,456
  pixels wide.

### What could not be measured

- Anything that needs the owner's hands or hardware: a gamepad, a second
  display, moving the window between displays, sleep and wake, a sound
  device connecting during play, sound through the speakers.
- Fullscreen changes, resizing and the app menu at run time: the session
  cannot press keys in the game's window.
- `compare_win32.py` on the scratch edits, and the Linux gates with them:
  the scratch copy is not a repository. Every edit was written to be
  macOS-only, so both are expected to pass; the tasks run them.
- The comparison under `--profile all`, the drawn pixels against Linux's,
  and the hand gallery's capture mode on macOS.
- The sanitizer build over the play sets.

## What MAC-002 owns, and what it leaves

It owns the video halves of verification 1 and 3, items 6, 7, 10 and 13,
and 9; and the three cases of "What no item above exercises" that the
macOS plan gives it (sleep and wake, a display unplugged, the sound device
changing).

The macOS plan says MAC-002 is done when those hold at the strict branch's
tip **and the branch is on `main` through the laptop**. This plan's tasks
end with the branch ready and a hand-over for that session; the status row
says "Ready for the laptop", not "Done", until the owner has landed it.

It leaves to MAC-003: the player's folder and first-run defaults, the
no-ROM message, downloads, the weapon icon worker (off on macOS today:
`port/fast3d/weapon_cache.cpp:435-451`), the file pickers (Export and
Import fail with a message until then), and the four recorded defects. To
MAC-004 and later: the bundle, the pinned SDL2, crash reporting, signing.

It does not reopen MAC-001's decision 2: a clang sanitizer build keeps
`array-bounds`, `object-size` and `float-cast-overflow` off by name and
`pointer-overflow` off for `model.c`, and the three strict-tier lines they
fire on (`gunfire.c`, `model.c:5959`, `bondview2.c:5505`) are not edited
although a strict branch now exists.

## Landing: the strict branch

The checkout at `/Users/peterryszkiewicz/Repos/Iron-Sights` stays on
`main`: other sessions work in it. Strict-tier work happens in a second
working tree on the branch `macos/strict`:

```bash
git worktree add -b macos/strict ../Iron-Sights-macos-strict main
```

- **`W`** below is that folder. Its build trees are `W/build/mac/...`.
  The Mac's clone has `midas.galleryDefer` set, so its pre-commit hook
  passes a strict-tier commit on this branch and prints that the gallery
  is stale. `review/hand-gallery` is never regenerated or edited on the
  Mac.
- **Work in `W`.** A task's changes are made and checked there, at the
  branch's tip. When its checks pass, its files outside the strict tier
  are committed on `main` from the checkout and pushed, `main` is merged
  into the branch, and its strict-tier files are committed on the branch
  (decision 2 says why a merge and not a rebase). Each task lists its
  files under **Main** and **Branch**.
- **A workflow or harness change that needs the branch to pass** (the CI
  video build of Task 12) is committed on the branch and lands with it.
- **Every edit under `port/fast3d/` is compiled only on macOS**, behind
  `#ifdef __APPLE__`, with two exceptions that the plan names and that
  `compare_win32.py` must show identical or decision 4 must cover:
  Task 1's cast and Task 6's screenshot row.
- The branch is pushed to `origin` after each task, never force-pushed,
  and never merged to `main` from the Mac.

## Global Constraints

- **No strict-tier file reaches `main` from the Mac:** nothing under
  `port/fast3d/`, and none of `port/src/port_arm_mesh.c`,
  `port_native_arms.c`, `port_grip_mesh.c`, `port_wardrobe.c`,
  `ads_hands.c`, `port_knife_ads.c`, `port_graphics.c`, `port_shiny.c`,
  `port_weapon_images.c`, `port_hud_weapons.c`, `joy_port.c`,
  `src/game/gunfire.c`, `model.c`, `bondview2.c`,
  `scripts/port/hand_gallery.py` and its template. Before each push of
  `main`, `git diff --name-only origin/main..main` lists none of them.
- **No other build changes.** Before every commit, `python3
  scripts/port/compare_win32.py commits main WORKTREE` (run in the tree
  being committed) exits 0 with `identical in loaded code and data`, and
  before each branch push `commits main macos/strict` does. Every edit to
  a shared source or header is inside `#if defined(__APPLE__)`, so the
  other compilers see the tokens they see today. Help and prompt text
  that changes for macOS changes only in the macOS build.
- **macOS code lives in `port/src/port_macos.c`** (`port_macos.h`),
  compiled only when CMake's `APPLE` is true, under `-Wall -Wextra
  -Werror`. Shared files call it from `__APPLE__` branches.
- **The owner's screen.** An automated run on the Mac is hidden
  (`GE_WINDOW_HIDDEN`, Task 3). A step that shows a window says "visible"
  and how long, uses a 640x480 window, never fullscreen, and never plays
  sound through the speakers unless it says "audible". Runtime fullscreen,
  resizing, displays, devices and sound by ear belong to the owner's
  session (Task 13).
- **Pictures stay comparable.** A run with `GE_WINDOW` set keeps one
  pixel a point (Task 6), so screenshots, digests and the Linux
  comparison do not change with the display the Mac happens to have.
- **Render performance and load before play** (`AGENTS.md`): no driver
  query in a per-frame path; a new late load is fixed or recorded. A
  change to per-frame work is measured (Task 9's commands) before it is
  committed.
- **No new setting, no new `midas.ini` key.** Retina and fullscreen follow
  the keys that exist.
- **Every commit updates `FORK_CHANGES.md`** (FORK-093): what changed,
  which side of the strict line it is on, and what ran.
- Build trees and run output live under `build/mac/` of the tree that
  made them; ROM-derived output (logs, screenshots, recordings, digests)
  never leaves it and is deleted before the close. Python uses the
  standard library only. Shell scripts start with `#!/usr/bin/env bash`
  and `set -euo pipefail`. New source files stay under 500 lines.
- Never `git commit --no-verify`. Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

### Task check

Every task ends by running this in `W` and recording the results in
FORK-093. `ROM` is the ROM's path. Step 2 exists from Task 3, step 3 from
Task 5.

```bash
# 1. The macOS video build, its link check, the gate and the narrowing check.
cmake -S . -B build/mac/video -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build/mac/video
python3 scripts/port/lp64_diagnostics.py gate build/mac/video
python3 scripts/port/lp64_diagnostics.py narrowing --check build/mac/video

# 2. The 20 missions, each in a hidden window.
python3 scripts/port/audit_buffers.py --mission-smoke build/mac/video/IronSights --rom "$ROM" --video --frames 1800 --jobs 4 --output build/mac/smoke-video/missions.json

# 3. Pictures, simulation and sound against aarch64 Linux (Task 5's commands).

# 4. MAC-001's check: the headless tree still builds, boots and plays.
cmake -S . -B build/mac/headless -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build/mac/headless
python3 scripts/port/lp64_boot.py --rom "$ROM" --expect-run --frames 600 build/mac/headless/IronSights
bash scripts/port/macos_harnesses.sh

# 5. The Linux 64-bit trees still pass their gate, and aarch64 still boots
#    (MAC-001's task check, step 3, unchanged).

# 6. The shipped Windows program is unchanged.
python3 scripts/port/compare_win32.py commits main WORKTREE
```

Expected: step 1 exits 0 with `OK: 702 weak references, 0 strong` (the
count may move with the tree; the strong count may not), `0 failed` and
the narrowing check `OK`; step 2 prints 20 `PASS`, each with its pictures;
step 3 prints `38 identical` for `sim` and `gfx` and `24 identical` for
`snd`; step 4 as MAC-001; step 5 `0 failed` and `OK: ran 600 frames`; step
6 exits 0. The browser build is checked by CI after a push to `main`.

## Review Focus

1. Points and pixels are mixed once Retina is on: the pointer highlights
   another menu row, the crosshair drifts from the mouse, or a screenshot
   overruns its row. Expected: the scale function's harness, a screenshot
   as wide as the drawable, and a 5,120-pixel row copied whole. (Task 6.)
2. The render thread and the main thread both touch the window, and the
   game hangs on a resize, a fullscreen change or a display change.
   Expected: no report from Xcode's Main Thread Checker over a 120 Hz run,
   and the owner's session with the thread on. (Tasks 8 and 13.)
3. A hidden run passes while the shown window is wrong: no first picture,
   no focus, another pace. Expected: one visible pass of the mission
   smoke, pacing measured in a shown window, and the owner's session.
   (Tasks 9 and 13.)
4. A strict-tier edit meant for macOS changes what Windows or Linux draws.
   Expected: the Windows comparison identical for every branch commit, and
   the Linux video gate and comparison unchanged. (Task check, steps 3, 5
   and 6.)
5. The sound device changes, or the Mac sleeps, while the game holds its
   audio device and GL context. Expected: the game carries on or reopens
   the device; the session's log has no `audio frame skipped` storm and
   no GL error. (Task 13.)

---

### Task 1: The strict branch, and the video tree builds and links

**Files:**
- Main: `include/math.h` (top of file), `CMakeLists.txt` (the video link
  branch at `:908-923`, `ge_save_transfer`'s includes at `:752-767`, the
  clang classes at `:134-142`), `scripts/port/test_pointer_bits.sh`,
  `FORK_CHANGES.md`
- Branch: `port/fast3d/hd_textures.cpp:252`

**Interfaces:**
- Produces: `SDL2_MACOS_ROOT`, a CMake cache path (default
  `/opt/homebrew`) holding `include/SDL2` and `lib/libSDL2.dylib`; the
  tree `build/mac/video`; the worktree `W` on `macos/strict`.

- [x] **Step 1: Make the worktree**

The command under "Landing". Check `git -C W config --get
midas.galleryDefer` prints `true` and `git -C W branch --show-current`
prints `macos/strict`.

- [x] **Step 2: Write the failing harness case**

In `test_pointer_bits.sh`, a Darwin-only case beside the existing ones
(it replaces nothing; `skip 8d` stays): configure a 64-bit video tree in
the harness's work folder with no SDL variable, and require that it
configures, that the `IronSights` link line in `build.ninja` names
`libSDL2.dylib` and neither `-lGL` nor `-ldl`, and that a configure with
`-DSDL2_MACOS_ROOT=/nonexistent` fails with a message naming the variable.
Run it. Expected: FAIL (the link line is `SDL2 GL dl pthread`).

- [x] **Step 3: The macOS video link and SDL2's place**

An `if(APPLE)` branch ahead of the Linux ones: include
`${SDL2_MACOS_ROOT}/include/SDL2`, link
`${SDL2_MACOS_ROOT}/lib/libSDL2.dylib` and nothing else; the same include
for `ge_save_transfer`; a `FATAL_ERROR` naming `SDL2_MACOS_ROOT` and `brew
install sdl2` when the library is missing. Run the harness. Expected:
PASS.

- [x] **Step 4: libc++ finds its own `<math.h>`**

At the top of `include/math.h`, before its guard: for `__APPLE__` C++,
`#include_next <math.h>`. C and every other platform read the file as
today. Name `macro-redefined` (13) and `null-character` (1) for C++ in
the clang list, with their counts in its comment.

- [x] **Step 5: The one strict-tier line**

`hd_textures.cpp:252`: `static_cast<long long>` on the file time's count.
It is the value's own type on Windows and Linux; `compare_win32.py` must
say `identical`, and if it does not, the cast goes behind `__APPLE__`.

- [x] **Step 6: Build and check**

Task check, steps 1, 4, 5 and 6. Expected: step 1 as written there, with
`393 files compiled, 0 failed` and `OK: 33 narrowings in 19 files, all
allowed`, and a compiler log with no warning.

- [x] **Step 7: Commit**

`main`: "Build the macOS video tree's C++ against libc++ and link SDL2".
FORK-093 says that on `main` alone the macOS video tree fails in exactly
one file until the branch lands. Branch: "Give libc++'s file time to
to_string as a long long".

### Task 2: The game finds itself and starts

**Files:**
- Main: `port/src/port_macos.c`, `port/include/port_macos.h`,
  `scripts/port/test_macos_host.sh` and its C source, `port/src/main.c`
  (`:727`), `FORK_CHANGES.md`
- Branch: `port/fast3d/save_transfer.cpp` (`:141-146`)

**Interfaces:**
- Produces: `size_t portMacExecutablePath(char *buffer, size_t size);` The
  running executable's real path, symlinks resolved, NUL-terminated, in
  `buffer`; returns its length, or 0 with `buffer` unchanged when it does
  not fit or cannot be read.

- [x] **Step 1: Write the failing host tests**

In `test_macos_host`: the path is absolute and names the same file as the
test's own executable (`st_dev` and `st_ino` equal); a 4-byte buffer
returns 0 and is unchanged; run through a symlink placed in a folder
whose name has a space, it returns the real file's path, not the link's.
Run. Expected: FAIL (the function does not exist).

- [x] **Step 2: Implement it**

`_NSGetExecutablePath` and `realpath`. Run. Expected: PASS, and
`test_macos_host: PASS` at 64 bits.

- [x] **Step 3: Use it in both places**

`main.c:727` and `save_transfer.cpp:143`, each in an `__APPLE__` branch
beside the `readlink`, which the other platforms keep.

- [x] **Step 4: See it start (visible, about 5 s)**

In a fresh folder holding a copy of `build/mac/video/IronSights` and a
`midas.ini` of `[display]` and `fullscreen=windowed`: run it with
`GE_ROM`, `GE_FRAMES=300`, `GE_NOINPUT=1`, `GE_WINDOW=640x480`,
`SDL_AUDIODRIVER=dummy`. Expected: exit 0; the log has `ge: video: window
+ GL context up`, `GL: version: 4.1`, and `ge: frame limit reached:
frames=300 gfxtasks=` with a count of 100 or more. Run it once more by a
relative path from another folder: the `ge: root:` line names the
executable's folder.

- [x] **Step 5: Commit**

`main`: "Find the macOS executable's path from dyld". Branch: "Start on
macOS: the restart path without /proc".

### Task 3: The tools run the game in a hidden window

**Files:**
- Main: `scripts/port/lp64_play_run.py` (`:64-77`),
  `scripts/port/lp64_equiv_pictures.py` (`:179`),
  `scripts/port/test_run_copies.py` (`:213-215`),
  `scripts/port/test_audit_buffers.py` (`:152`), the docstrings that say
  "offscreen" (`lp64_play.py:27,198`, `audit_buffers.py:6,195`),
  `FORK_CHANGES.md`
- Branch: `port/fast3d/gfx_sdl2.cpp` (`:513`)

**Interfaces:**
- Produces: the game variable `GE_WINDOW_HIDDEN` (macOS only): when set,
  the window is created and never shown. In `lp64_play_run.py`:
  `host_video_driver(os_name: str = os.name, platform: str =
  sys.platform) -> str`, which returns `'cocoa'` for `'darwin'`,
  `'windows'` for `os_name == 'nt'` and `'offscreen'` otherwise; and
  `host_video_variables(platform: str = sys.platform) -> dict[str, str]`,
  today's `VIDEO_VARIABLES` plus `GE_WINDOW_HIDDEN=1` for `'darwin'`.
  `VIDEO_VARIABLES` stays the module's name for the host's own set.

- [ ] **Step 1: Write the failing tool tests**

In `test_run_copies.py`: `host_video_driver('posix', 'darwin') ==
'cocoa'`; `('posix', 'linux') == 'offscreen'`; `('nt', 'win32') ==
'windows'`; `host_video_variables('darwin')['GE_WINDOW_HIDDEN'] == '1'`;
the key is absent for `'linux'` and `'win32'`; and `lp64_play.py`'s
`--env GE_WINDOW_HIDDEN=0` is refused with `--video` on a host whose set
holds it. In `test_lp64_equiv_pictures.py`: the gallery mode's
environment takes its driver from `host_video_driver`. Run. Expected:
FAIL.

- [ ] **Step 2: Implement the tool side**

Run the Python tests. Expected: PASS; on Linux and Windows the variables
are byte for byte what they are today (the existing tests hold that).

- [ ] **Step 3: The game side**

`gfx_sdl2.cpp:513`, inside `#ifdef __APPLE__`: the window is not shown
when `GE_WINDOW_HIDDEN` is set. `port/src/port_console_env.h` is a list
pasted by hand and is left alone.

- [ ] **Step 4: Play, hidden**

```bash
python3 scripts/port/lp64_play.py run --video --rom "$ROM" --set demos,stages,stops,menus,tours,attract,sound --output build/mac/play/video build/mac/video/IronSights
python3 scripts/port/lp64_play.py peaks --budget scripts/port/lp64_budgets.txt build/mac/play/video
python3 scripts/port/lp64_play.py sound build/mac/play/video
```

and the task check's step 2. Expected: `OK: 119 of 119 cases`, `OK: N
pool and stage pairs within budget` (318 when measured), two recordings
that pass; 20 `PASS`; no window on screen at any time.

- [ ] **Step 5: Commit**

`main`: "Run the video tools in a hidden Cocoa window on macOS". Branch:
"Keep the macOS window hidden when a tool asks".

### Task 4: The core profile first, and a clean log

**Files:**
- Main: `scripts/port/lp64_play_judge.py`,
  `scripts/port/test_lp64_play_video.py`, `FORK_CHANGES.md`
- Branch: `port/fast3d/gfx_sdl2.cpp` (`:404-415`),
  `port/fast3d/gfx_opengl.cpp` (`:1705`, and the retained twin's vertex
  shader, `:2403-2564`)

**Interfaces:**
- Produces: in `lp64_play_judge.py`, `RENDER_FAULTS`, the patterns a
  video case's log must not hold: `Failed to compile`, `Failed to link`,
  and `GL: could not find function`.

- [ ] **Step 1: Write the failing judge tests**

`test_video_case_with_a_shader_link_failure_fails`,
`test_video_case_with_a_shader_compile_failure_fails`,
`test_video_case_with_a_missing_gl_function_fails`, and
`test_headless_case_ignores_render_fault_lines`: each a passing video log
with one such line added. Run. Expected: the first three FAIL.

- [ ] **Step 2: Implement the judge's rule**

On every host: the aarch64 Linux video logs of this plan's measurement
hold none of the three. Run the Python tests. Expected: PASS. Rerun Task
3's play sets. Expected: every video case now FAILS on macOS, naming the
`GL: could not find function` lines.

- [ ] **Step 3: Ask for the core profile first**

Under `__APPLE__` the context list is 4.1 core, then 3.2 core, and
nothing else. Expected in a log: `SDL: created GL4.1core context` with no
`could not create` line before it.

- [ ] **Step 4: The loader's 36 lines**

Under `__APPLE__`, a function the context does not export is not logged
by name at error level; one line gives the count (`GL: N functions not
exported by this context`). Expected: 36, and none of them is a function
the renderer calls (`grep` each name over `port/fast3d/`, glad excepted).

- [ ] **Step 5: The sky programs' retained twin**

Under `__APPLE__`, no twin is linked for a program whose fragment shader
reads `vGeSkyQ`, or its vertex shader writes it: whichever leaves the
picture digest of Task 5 identical and logs no error. Record which, and
why the sky is never drawn retained.

- [ ] **Step 6: Check and commit**

Task 3's play sets and the task check. Expected: all pass, and `grep -c`
of the three patterns over the logs is 0. `main`: "Fail a video case on a
shader or GL loader fault". Branch: "Ask macOS for the core profile first
and link what it can".

### Task 5: The same pictures, simulation and sound as aarch64 Linux

**Files:**
- Main: `scripts/port/lp64_equiv_expected.txt` (its header comment only),
  `FORK_CHANGES.md`

**Interfaces:**
- Produces: the trees `build/mac/cmp-video` and (in the checkout, in the
  aarch64 container) `build/lp64/cmp-a64-video` at one commit of `main`;
  the folders `build/mac/equiv/RUN/mac` and `build/lp64/equiv/RUN/a64`.
  Later tasks rerun this as the task check's step 3.

- [ ] **Step 1: Build both and run both**

```bash
cmake -S . -B build/mac/cmp-video -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DGE_COMPARE=ON && cmake --build build/mac/cmp-video
python3 scripts/port/lp64_equiv.py run --video --rom "$ROM" --set demos,solo,mp,menus,reload --repeat 2 --output build/mac/equiv/RUN/mac build/mac/cmp-video/IronSights
```

and the same `run` on `build/lp64/cmp-a64-video` in the aarch64
container, from the checkout at the commit of `main` the branch last
merged. Expected, both: `OK: 38 of 38 cases ran`, `OK: 38 of 38 cases
agree with themselves`.

- [ ] **Step 2: Compare the three streams**

`lp64_equiv.py compare --streams sim --expect
scripts/port/lp64_equiv_expected.txt A64 MAC`, then `gfx`, then `snd`.
Expected: `38 identical`, `38 identical`, `24 identical`, and no
`a64:mac` line in the expected file. A difference is traced to its cause
before any line is written; no hash pin without the owner's word.

- [ ] **Step 3: Every fork option**

Both runs again with `--profile all`, and the same three comparisons.
Then `lp64_equiv.py sensitivity` on the macOS build. Expected: identical
streams, and every stream differs between two seeds. This was not
measured for the plan.

- [ ] **Step 4: The drawn pixels, by eye**

Both builds once more with `--draw --shots 600` (a screenshot every 600
pictures), then `lp64_equiv_pictures.py shots A64 MAC`. The two
rasterizers differ, so pixels will: record how many cases and how large.
Read the 38 macOS screenshots one by one beside Linux's. A picture that
is wrong (a missing surface, a wrong colour, a seam) is a defect and
stops the task; a difference of filtering or edges is recorded.

- [ ] **Step 5: The hands, by eye**

`lp64_equiv_pictures.py gallery --rom "$ROM" --output build/mac/gallery
build/mac/cmp-video/IronSights`. Expected: 216 captures. Read twelve of
them (both hands, three weapons, two wardrobes) beside the committed
images of `review/hand-gallery`. Nothing is written to that folder and
`hand_gallery.py generate` is never run.

- [ ] **Step 6: Commit**

`main`: "Compare the macOS pictures and sound with the aarch64 Linux
build". The expected file's header says `a64:mac` has no `gfx` or `snd`
line either.

### Task 6: Retina

**Files:**
- Main: `port/src/video.c` (`:5061`, `:575-581`, `:3644`, `:3783`),
  `port/src/input_pc.c` (`:8494`, `:8640`, `:8686`, `:8805`, `:8826`),
  new `port/src/port_hidpi.c` and `port/include/port_hidpi.h`, new
  `scripts/port/test_hidpi.sh` and its C source, the workflow line that
  names the new harness, `FORK_CHANGES.md`
- Branch: `port/fast3d/gfx_sdl2.cpp` (`:67`, `:186`, `:224`, `:286`,
  `:396-400`, `:445`, `:732`, `:813-835`), `port/fast3d/gfx_pc.cpp`
  (`:8987-8992`)

**Interfaces:**
- Produces: `s32 portHidpiToPixels(s32 value, s32 points, s32 pixels);`
  (`value` scaled by `pixels / points`, rounded to nearest; `value`
  itself when the two are equal or `points` is not positive) and `s32
  portHidpiToPoints(s32 value, s32 points, s32 pixels);`, its inverse;
  and `portBmpPackRows` (Step 5). Rule for the rest of the task: sizes saved to `midas.ini` and positions
  given to SDL are points; sizes given to the renderer and compared with
  the framebuffer are pixels.

- [ ] **Step 1: Write the failing harness**

`test_hidpi.sh` (it sources `harness_widths.sh` and runs at every width
it offers): 100 of 640 points on 1,280 pixels is 200; 0 stays 0; the last
point maps inside the last pixel pair; equal sizes return the value;
`points == 0` returns the value; and to-points of to-pixels returns the
value for every coordinate of a 1,728-point row. Run. Expected: FAIL.

- [ ] **Step 2: Implement the two functions**

Run. Expected: `test_hidpi: PASS` at each width.

- [ ] **Step 3: Allow HiDPI on macOS, except in a test run**

`video.c:5061`, under `__APPLE__`: `allow_hidpi` is set unless `GE_WINDOW`
is set. `gfx_sdl2.cpp:396`: the flag's guard becomes `#if
defined(SDL_WINDOW_ALLOW_HIGHDPI) || defined(__APPLE__)`, so macOS adds
the flag and the other builds compile what they compile today (the name
is an enumerator, so their guard has always been false). Expected (visible, 5 s, no `GE_WINDOW`, a `midas.ini` with
`fullscreen=windowed`, `window_width=640`, `window_height=480`): the log
shows `SDL: window 640x480` and `SDL: drawable 1280x960`; with `GE_WINDOW`
set, `drawable 640x480`.

- [ ] **Step 4: Each place points meet pixels**

The picture was measured right as the code is; the pointer was not
checked. The sites under **Files**, each under `__APPLE__` where the code
is shared: the pointer and its clamp, the mouse position the menus read,
the warp, the size compared with display modes, the size saved to the ini
(points), the framebuffer first made from the settings (pixels).
Expected: the harness still passes; a `GE_SHOT` screenshot of the Retina
window is 1,280x960 and shows the whole picture; and with the pointer
moved by hand to each corner of a menu (visible, the builder asks the
owner for one minute, or leaves it on Task 13's list), the row under the
pointer is the one highlighted.

- [ ] **Step 5: The screenshot's rows**

The repacking of `gfx_sdl2.cpp:824-835` moves to `void
portBmpPackRows(uint8_t *pixels, int width, int height);` in
`port_hidpi.h` (in place: `height` rows of packed RGB, as `glReadPixels`
returns them with a pack alignment of 1, become BMP rows, BGR and padded
to 4 bytes), with no fixed-size row. Test first, in `test_hidpi.sh`:
widths 1, 2, 3, 4, 4,096, 4,097 and 5,120 by 2 rows, every pixel's bytes
swapped and every row at its padded offset. Expected: FAIL (no such
function), then PASS. `gfx_sdl2.cpp` calls it (decision 4 says in which
builds). This Mac cannot reach the overrun end to end: the game clamps a
window to the display, 3,456 pixels wide at most.

- [ ] **Step 6: Check, measure and commit**

The task check (step 3 is unchanged: those runs set `GE_WINDOW`). Task
9's cost commands with Retina on and with it off. `main`: "Draw at the
display's own resolution on macOS". Branch: "Size the macOS framebuffer
from the drawable".

### Task 7: Fullscreen, and what a Mac user expects of a window

**Files:**
- Main: `port/src/port_option_help.c` and `tools/midas_ini_help.txt` (the
  fullscreen row, macOS wording in the macOS build only),
  `FORK_CHANGES.md`
- Branch: `port/fast3d/gfx_sdl2.cpp` (`:219-261`, `:351-387`, `:670-679`)

- [ ] **Step 1: Borderless is SDL's desktop fullscreen on macOS**

Under `__APPLE__`, `fullscreen=borderless` (the default) and
`fullscreen=fullscreen` enter and leave through
`SDL_SetWindowFullscreen(wnd, SDL_WINDOW_FULLSCREEN_DESKTOP)`, which hides
the menu bar and the Dock; the windowed size and position are restored on
leaving. Expected (visible, 5 s, started with each ini value): the log
names the mode, and the drawable is the display's pixel size.

- [ ] **Step 2: Exclusive mode**

Start with `fullscreen=exclusive` (visible, 5 s). If it enters, draws
and leaves with the desktop as it was, it stays SDL's. If it faults or
leaves the display in another mode, exclusive means desktop fullscreen on
macOS and the help line says so. Record which.

- [ ] **Step 3: The keys**

Command-Q reaches the game as `SDL_QUIT` and takes the saving quit path
(`video.c:2717-2738`): check that `midas.ini` is written. The fullscreen
toggle (`:670-679`, Alt+Enter, which is Option-Return on a Mac keyboard)
also answers Control-Command-F. Command-Tab, Mission Control and
Command-H release the mouse and do not leave the game drawing over
another app. Prompts and help text name keys as SDL names them on macOS
(Option, Command); a line that says "Alt" or "Windows key" to a Mac
player is corrected in the macOS build. A convention left unsupported is
recorded in FORK-093 with its reason.

- [ ] **Step 4: Check and commit**

The task check. The keys and the mode changes at run time are on Task
13's list for the owner; this task leaves each one written there with
what should happen. Branch: "Use the system's fullscreen on macOS".
`main`: the help text, if any changed.

### Task 8: The render thread

**Files:**
- Main: `port/src/video.c` (`:5570-5585`, only if the thread is switched
  off on macOS), `FORK_CHANGES.md`
- Branch: `port/fast3d/gfx_sdl2.cpp` (`:615-616`, `:702-709`, `:732`)

- [ ] **Step 1: Measure it as it is (visible, 60 s)**

Dam at the shipped 120 Hz tick in a 640x480 window, from a folder made
as in Task 2's Step 4, with Xcode's checker loaded:

```bash
DYLD_INSERT_LIBRARIES=/Applications/Xcode.app/Contents/Developer/usr/lib/libMainThreadChecker.dylib \
  GE_ROM="$ROM" GE_STAGE=33 GE_FRAMES=7200 GE_NOINPUT=1 SDL_AUDIODRIVER=dummy ./IronSights
```

Record every `Main Thread Checker:` report with its call, the `ge:
stats:` lines' frame rate, and the count of `ge: hitch:` lines. The same
run on the sanitizer build.

- [ ] **Step 2: Keep window queries on the main thread**

Under `__APPLE__`: the drawable's size and the window's position are
read on the main thread when the size or position changes and kept in
two atomics that the render thread reads (`gfx_sdl2.cpp:615-616`); the
`glGetString` of `:702-709` is not called from the main thread while the
render thread holds the context. Expected: Step 1's run again with no
report.

- [ ] **Step 3: The cost of going without**

The same run with `GE_RENDERTHREAD=0`. Record frame rate, hitches and the
`gl`, `swap` and `idle` figures of the stats lines beside Step 2's.

- [ ] **Step 4: Check and commit; the decision waits for the owner**

The task check. The macOS plan keeps the thread if a 120 Hz run with
resizing, fullscreen changes and a display change shows no fault: those
three are on Task 13's list, with the thread on. This task puts Steps 1
to 3's figures in FORK-093. If Step 2 cannot clear the reports, macOS
runs without the thread (`portRenderThreadWanted` returns 0 under
`__APPLE__`), and the status table records Step 3's cost. Branch: "Read
the macOS window's size on the main thread".

### Task 9: Pacing and cost

**Files:**
- Main: `scripts/port/macos.md`, `FORK_CHANGES.md`, and whatever a
  finding's fix needs (each fix is its own failing check first)

- [ ] **Step 1: Pacing in a shown window (visible, 60 s each)**

Dam for 60 s at 120 Hz and at `GE_TICKHZ=60`, windowed, vsync on, with
and without Retina. Record frame rate and the count and size of `ge:
hitch:` lines. Measured for the plan: 120 Hz holds with one hitch, the
stage's first frame; 60 Hz logs 66 hitch lines in 20 s, 50 of them 25 ms
frames. Find the cause of the 60 Hz pattern (a sleep to the next retrace
against a swap that waits for the panel's 120 Hz is the first thing to
read). Fix it if the fix is macOS-only; otherwise record it with the
setting that avoids it.

- [ ] **Step 2: The smoke benchmark as a baseline**

`python3 scripts/port/bench/bench.py run --smoke --build
mac=build/mac/video/IronSights --out build/mac/bench/smoke.json` with the
macOS video build. macOS has no earlier build to compare
with and `stock.py` builds only Windows executables, so the figures are
the first macOS baseline (decision 5): Retina on against off, the render
thread on against off. `GE_PROFILE` is Windows code; when a figure needs
explaining, `sample` the process and say what it showed. Run it shown
(visible; tell the owner, and say how long the three scenarios, two
presets and three repeats take): on Windows a hidden window's frames are
slower and noisier, and whether that holds here is part of the baseline.
Measured hidden with one repeat: `dam-walk` 1,066 pictures a second,
`cradle-fight` 705, `temple-4p` 705.

- [ ] **Step 3: The late-load audit, all 20 stages**

`python3 scripts/port/late_load_audit.py --exe
build/mac/video/IronSights --no-pack --env GE_WINDOW_HIDDEN=1 --keep
--out build/mac/late-loads`, hidden. Each `ge: late load:` line of a stage is fixed (prefetched in the
stage load, or recorded for later runs) or written in FORK-093 with its
reason. Measured for two stages: 0 frames over budget, no shader compiled
during play. Keep the audit (`--keep`) and `--compare` it with an aarch64
Linux audit of the same commit, made in its container with
`--video-driver offscreen`: a late event macOS has and Linux does not is a
finding. One is known outside the tours: the first mipmap generation of a
run, about 10 ms at the title screen, which every macOS log shows as a
`ge: late load:` line. It is warmed up at boot, or recorded.

- [ ] **Step 4: One visible pass of the missions**

The task check's step 2 once without `GE_WINDOW_HIDDEN` (`--jobs 1`,
visible, about 12 minutes of 640x480 windows; tell the owner before
starting). Expected: 20 `PASS` with picture counts within a tenth of the
hidden run's.

- [ ] **Step 5: Commit**

`main`: "Measure the macOS window's pace and cost", with the figures in
FORK-093 and the commands in the guide.

### Task 10: The sanitizers, with a window

**Files:**
- Main: `FORK_CHANGES.md`; `CMakeLists.txt` only if a new site needs
  naming under MAC-001's decision 2

- [ ] **Step 1: Build and play**

`-DGE_SANITIZE=ON -DGE_VIDEO=ON` into `build/mac/san-video`; the task
check's step 2 on it; then `lp64_play.py run --video --frames-scale 3
--set demos,stages,stops,menus,tours,attract`. Expected: 20 `PASS`, `OK:
117 of 117 cases`, and no sanitizer report. Measured for the plan: Dam,
1,500 frames, no report.

- [ ] **Step 2: A report is fixed or stops the task**

A memory error is fixed where it is, test first, on the side of the
strict line its file is on. An undefined-behaviour report of the three
kinds decision 2 names, on a decompiled line, is added to the comment
beside `GE_SANITIZE` with its site; any other kind goes to the owner.

- [ ] **Step 3: Commit**

`main`: "Run the macOS video build under the sanitizers".

### Task 11: Sound and input, as far as a session can check

**Files:**
- Main: `FORK_CHANGES.md`, and whatever a finding's fix needs

- [ ] **Step 1: The real sound device (audible, 30 s; tell the owner)**

Dam for 30 s hidden with `SDL_AUDIODRIVER` unset and `GE_AUDIO_CAPTURE`
set. Expected: `ge: audio: opened the default device on driver
'coreaudio': 22050 Hz, 2 ch`, no `audio frame skipped` line, and a
recording that passes `lp64_play.py sound`'s checks (22,050 Hz stereo, 10
s or more, peak 1,000 or more).

- [ ] **Step 2: What the game calls the Mac's controls**

Read every prompt and help line that names a key or a mouse button
(`port/src/port_option_help.c`, the control pages, the bind screen) in
the macOS build: a name comes from SDL (`SDL_GetKeyName` gives "Left
Option", "Left Command") or is fixed for macOS. The aim prompt says
"right mouse button"; a trackpad's two-finger click sends it. Count the
`platform:Mac OS X` lines of `tools/gamecontrollerdb.txt`: a pad without
one falls to SDL's own mappings.

- [ ] **Step 3: A pad, if one is connected**

With the owner's gamepad plugged in: the log's `ge: input:` lines name it
and its mapping. Without one, this step is Task 13's.

- [ ] **Step 4: Commit**

`main`: "Check sound and control names on macOS", with what was found
and what waits for the owner.

### Task 12: CI, the harness list and the guide

**Files:**
- Branch: `.github/workflows/windows-release.yml` (job
  `macos-arm64-tests`), `scripts/port/macos_harnesses.sh` (its list, if
  Task 1's case changed a skip line)
- Main: `scripts/port/macos.md`, `AGENTS.md` (the macOS paragraph),
  `FORK_CHANGES.md`

- [ ] **Step 1: The job builds the video tree too**

After its headless steps: configure and build `build/mac/video` with
`-DSDL2_MACOS_ROOT="$(brew --prefix)"` (the job already installs `sdl2`),
then the gate and the narrowing check over both trees. It runs nothing:
the runner has no ROM and no display. On the branch, because on `main`
alone that build fails in one file.

- [ ] **Step 2: The guide and the rules**

`macos.md` gains "The game in a window": the video tree, `SDL2_MACOS_ROOT`,
hidden and visible runs, the play sets and the comparison with `--video`,
the worktree and what may be committed where, Retina and `GE_WINDOW`,
what MAC-003 still lacks. `AGENTS.md`'s macOS paragraph gains the video
tree's line of the task check, `GE_WINDOW_HIDDEN`, and the rule that an
automated run on a Mac is hidden.

- [ ] **Step 3: Commit**

Branch: "Build the macOS video tree in CI". `main`: "Guide: the macOS
game in a window".

### Task 13: The owner's session, and the hand-over to the laptop

**Files:**
- Main: `scripts/port/macos_plan.md` (status table, MAC-002 row; the
  decisions of this plan), `scripts/port/macos_mac_002.md`,
  `FORK_CHANGES.md`

- [ ] **Step 1: The whole check at the tip**

The task check, Task 3's play sets, Task 5's three comparisons, Task
10's sanitizer runs, and verification 9: in the containers, the x86_64
and aarch64 play sets and the `x64:a64` comparison at the commit of
`main` the branch holds, as [the Linux guide](linux_64bit.md) runs them;
`compare_win32.py commits main macos/strict` identical; CI green on
`main`.

- [ ] **Step 2: The owner plays (verification 13)**

A written list, each line with what should happen, built from Tasks 6,
7, 8 and 11. The owner, with `build/mac/video/IronSights` from `W`:

1. plays one mission to its end with a gamepad, and one with mouse-look;
2. does so in a window and in fullscreen, on the built-in display and on
   an external one, and drags the window from one to the other;
3. resizes the window, toggles fullscreen several times, and uses
   Command-Tab, Command-H and Command-Q, with the render thread on;
4. connects headphones during play, then removes them;
5. closes the lid or sleeps the Mac during play and wakes it;
6. unplugs the external display while the game is on it;
7. says whether sound, pace and pointer feel right.

Then: `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt` on the
session's log; a pool past half its size is a finding for the owner. A
fault in 3 to 6 is a defect: fixed test-first if this milestone owns it,
or recorded with its log.

- [ ] **Step 3: The render thread's question**

Put Task 8's figures and the session's result to the owner: does the
thread stay on macOS? Record the answer in the macOS plan's decisions.

- [ ] **Step 4: Hand over**

The MAC-002 row says "Ready for the laptop", with what ran and its
figures, and the laptop session's list: fetch `macos/strict`; regenerate
and review the gallery there (it also inherits the review the 64-bit plan
left pending); commit it on the branch; fast-forward `main` to the
branch; push. It names every strict-tier file the branch changed and says
each change is compiled only on macOS, or, for one that is not, what
decision 4 settled. After the landing, a session on the Mac rebuilds
`main`'s video tree, reruns the task check there, and marks the row
Done. Delete `W/build/mac/` and the checkout's `build/mac/`.

```bash
git commit -m "MAC-002: ready for the laptop"
```

---

## What this plan does not do

- It lands nothing strict-tier on `main`; the laptop session does.
- It does not regenerate or edit `review/hand-gallery`.
- It adds no setting and changes no `midas.ini` key.
- It does not edit the three strict-tier lines clang's sanitizer names;
  MAC-001's decision 2 stands.
- It does not make downloads, the weapon icon worker, the file pickers or
  the player's folder work on macOS: MAC-003.
- It does not package, sign or report crashes.
- It does not compare drawn pixels by number with Linux's: the rasterizers
  differ, and the digest is the check.

## Decisions for the owner

**The owner accepted all five recommendations on 2026-11-28** ("Accept
all five"), in the session that wrote this plan. They are kept here as
put, each with the alternative that was not taken; the tasks are written
for the recommendations. Decision 1 defines "a real window" for the
automated runs; decision 2 replaces the macOS plan's "kept rebased" with
merging; decision 3 adds no setting; decision 4 puts one change that is
not macOS-only on the strict branch; decision 5 defines what verification
10 means on a platform with no earlier build.

1. **What "in a real window" means for the automated runs.** The macOS
   plan asks for the play sets and the `--video` tools in a real window.
   *Recommended:* a real Cocoa window that is created and never shown
   (`GE_WINDOW_HIDDEN`, compiled only on macOS). The Mac's GPU draws it,
   screenshots and recordings come from it, seven can run at once, and
   nothing takes the owner's screen or focus. One visible pass of the 20
   missions (Task 9) and the owner's session cover the shown window. The
   alternative is every run in a shown window, one at a time: the play
   sets alone would hold the screen for more than an hour a run.
2. **How the branch is kept current.** The macOS plan says the strict
   branch is "kept rebased on `main`". The laptop needs the branch on
   `origin`, and a pushed branch that is rebased must be force-pushed,
   which `AGENTS.md` bars. *Recommended:* `macos/strict`, pushed after
   each task, brought up to date by merging `main` into it and never
   rebased; the laptop lands it by fast-forwarding `main`. The
   alternatives: keep the branch on the Mac and push it once, at the
   hand-over (work after that needs a force push or a second branch); or
   allow `--force-with-lease` on this one branch.
3. **Retina.** *Recommended:* on macOS the window always allows HiDPI and
   draws at the display's pixels, as the macOS plan's design says, except
   in a run with `GE_WINDOW` set, which keeps one pixel a point so that
   pictures and digests compare with Linux's. No new setting: a player
   who wants fewer pixels has the existing `low_res` row. Task 6 reports
   the measured cost; if it breaks 120 pictures a second on this Mac, the
   question comes back. The alternative is a new row in Iron Sights >
   Graphics. Measured: Dam at 3,456x2,234 holds 120 pictures a second on
   this Mac, 4.1 ms of GL a frame against 3.4.
4. **The screenshot row buffer.** `gfx_sdl2.cpp:827` overruns a stack
   buffer when the drawable is wider than 4,096 pixels, in every build
   today (a 5K display on Windows reaches it). *Recommended:* fix it for
   every build on the branch. It is not macOS-only, so the landing rule
   asks the laptop for a before-and-after benchmark; the code runs only
   when a screenshot is taken, so that benchmark is expected to show
   nothing, and the hand-over says so. The alternative is a fix compiled
   only on macOS, with the Windows and Linux defect recorded for a later
   change.
5. **Cost without a "before".** Verification 10 asks for the smoke
   benchmark and a profile of a change to per-frame work. macOS has no
   earlier build, `stock.py` builds only Windows executables, and the
   profiler is Windows code the macOS plan excludes. *Recommended:* Task
   9's figures are the first macOS baseline (Retina on against off, the
   render thread on against off, the late-load audit over 20 stages), a
   late load macOS has that Linux does not is a finding, and `sample` is
   used only when a figure needs explaining. The alternative is a macOS
   profiler first, which is its own piece of work.

**Coming back to you during the milestone:** whether the render thread
stays on macOS (Task 13, Step 3, with Task 8's figures), as the macOS plan
says.

**Needed from you:** the session of Task 13, Step 2 (about an hour, with
a gamepad and an external display), and after it the laptop session that
lands `macos/strict`.
