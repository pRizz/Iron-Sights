# The native 64-bit Linux build

Iron Sights builds as a native 64-bit (LP64) Linux program beside the 32-bit
(i686) one. The work and its decisions are in the
[64-bit plan](native_64bit_plan.md) and the inventory's FORK-089 (the port)
and FORK-031 (the Linux packages). This guide is for developers: how the
64-bit game is built, checked and packaged, and what it does not have.

## Where it has run

- **The Mac (Apple Silicon), in the project's containers:** aarch64
  natively, x86_64 under Rosetta 2, i686 under qemu emulation. Video runs
  there draw offscreen with Mesa's software GL (`llvmpipe`), not a GPU.
- **CI:** GitHub's x86_64 runner (`ubuntu-24.04`) builds, packages and
  crash-tests the x86_64 packages on every push to `main` (the job
  `linux-x86_64` of `.github/workflows/windows-release.yml`). Nothing there
  runs on a GPU.
- **One session on real hardware:** the owner's laptop (Intel Core
  i7-10870H, NVIDIA GeForce RTX 3070 Laptop GPU beside an Intel chip),
  Windows 11 with WSL 2, WSLg and Ubuntu 24.04.1, on 2026-11-25 (LP64-007
  Task 10, recorded in FORK-089).

Nothing has run on a Steam Deck or on a native Linux desktop.

## The two images

The x86_64 build has two images, both reached through
`python3 scripts/port/lp64_env.py run --arch x86_64 [--release] -- COMMAND`
(the checkout at `/home/dev/project`, the ROM at `/rom/baserom.u.z64`).

| Image | Dockerfile | Compiler, glibc | For |
| --- | --- | --- | --- |
| `goldeneye` | `Dockerfile` (repository root) | Ubuntu 24.04, GCC 13, glibc 2.39 | development: the gate trees, the play sets, the comparison builds, and running a packaged executable on a current system (P4 below) |
| `midas-linux-release-x86_64` | `scripts/port/linux-release-x86_64.Dockerfile` | Ubuntu 22.04 (digest-pinned), GCC 11.4, glibc 2.35 | the shipped packages: `linux_build.py --arch x86_64` and `package_linux.py --arch x86_64`, here and in CI |

`python3 scripts/port/lp64_env.py build-images --release` builds both (and
the aarch64 and i686 video images). The aarch64 image, `midas-lp64-arm64`,
is for development only; no aarch64 package is made. The release image is
the i686 release image's base with the 64-bit development packages and no
i386 architecture; `linux-release.Dockerfile`, the i686 one, is unchanged.

## The development build

In the development container (`DEV` is
`python3 scripts/port/lp64_env.py run --arch x86_64 --`):

```bash
DEV sh -c 'cmake -S . -B build/lp64/x64-video -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON && cmake --build build/lp64/x64-video -j"$(nproc)"'
```

`-DGE_POINTER_BITS=64` is the switch; it needs runtime ROM loading and is
refused for the web build. Use `-DGE_HEADLESS=ON` instead of `-DGE_VIDEO=ON`
for a headless tree. `AGENTS.md` ("Repo-Local Guidance") lists the gate,
LTO, sanitizer and comparison trees and the static checks that run on them.

The boot check, then the play sets:

```bash
DEV python3 scripts/port/lp64_boot.py --rom /rom/baserom.u.z64 --expect-run --frames 600 build/lp64/x64-video/IronSights
DEV python3 scripts/port/lp64_play.py run --rom /rom/baserom.u.z64 --video --set demos,stages,stops,menus,tours,attract --output build/lp64/play/x64-video build/lp64/x64-video/IronSights
```

The boot check passes with `OK: ran 600 frames`. Each play case passes only
with its expected exit status and the lines that prove it ran; under
Rosetta 2 or emulation add `--timeout-scale F`. All run output stays under
`build/lp64/`.

## The release build and its packages

The package check (P1 to P4 of the
[LP64-007 plan](native_64bit_lp64_007.md), "Checks"). `REL` is
`python3 scripts/port/lp64_env.py run --arch x86_64 --release --`, `DEV`
the same without `--release`:

```bash
# P1. Build in the release image.
REL python3 scripts/port/linux_build.py --arch x86_64 --work build/lp64/ship/x64/deps --output build/lp64/ship/x64
# P2. Package into a fresh folder.
rm -rf build/lp64/ship/x64/dist && REL python3 scripts/port/package_linux.py --arch x86_64 --build build/lp64/ship/x64 --output build/lp64/ship/x64/dist
# P3. The ten crash-report cases on the real packages.
REL python3 scripts/port/test_sentry_linux.py --arch x86_64 --package build/lp64/ship/x64/dist/Iron-Sights-linux-x86_64.tar.gz --appimage build/lp64/ship/x64/dist/Iron-Sights-linux-x86_64.AppImage --steamdeck build/lp64/ship/x64/dist/Iron-Sights-steamdeck-x86_64.zip --output build/lp64/ship/x64/smoke
# P4. The packaged executable boots and plays, in the newer container.
rm -rf build/lp64/ship/x64/run && mkdir -p build/lp64/ship/x64/run && tar -xzf build/lp64/ship/x64/dist/Iron-Sights-linux-x86_64.tar.gz -C build/lp64/ship/x64/run
DEV env LD_LIBRARY_PATH=/home/dev/project/build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/lib python3 scripts/port/lp64_boot.py --rom /rom/baserom.u.z64 --expect-run --frames 600 build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/IronSights
DEV env LD_LIBRARY_PATH=/home/dev/project/build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/lib python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ship/x64/run/Iron-Sights-linux-x86_64/IronSights --rom /rom/baserom.u.z64 --video --frames 1800 --jobs 4 --output build/lp64/ship/x64/smoke-missions/missions.json
```

P2 writes `Iron-Sights-linux-x86_64.tar.gz`,
`Iron-Sights-linux-x86_64.AppImage`, `Iron-Sights-steamdeck-x86_64.zip`,
`SHA256SUMS-linux-x86_64.txt`, `linux-x86_64-build-info.json` and
`linux-x86_64-build-inputs.json`. P4 passes with `OK: ran 600 frames` and
20 of 20 missions. Both P4 tools run a copy of the executable from a
temporary folder, so the package's `lib/` is given as `LD_LIBRARY_PATH`.
Under Rosetta 2 the real AppImage cannot start (`Exec format error`); CI's
runner judges it. The release workflow attaches the three packages, their
sums file and the two JSON files to each release beside the i686 ones, and
attests the three packages (FORK-018, FORK-031).

What a player's system provides: glibc 2.35 or newer, OpenGL 3.3 drivers,
and the C++ runtime (`libstdc++.so.6` and `libgcc_s.so.1`, GCC 12 or newer:
the shipped files need at most `GLIBCXX_3.4.30`). The packager does not
bundle those two at either width (`SYSTEM_CXX_RUNTIME`, the
`system_libraries` of both the i686 and the x86_64 target in
`package_linux.py`), because a system GPU driver built with a newer GCC
(Mesa 25.2's `libLLVM` needs `GLIBCXX_3.4.32`) loads into the game's
process. No 32-bit library is needed.

## What a 64-bit build lacks and prints

- **No suspend points.** A suspend image records 32-bit addresses and the
  exact executable's layout ([save states](save_states.md)), so a 64-bit
  build has none: the watch row reads `Suspend Points: unavailable`, and
  the four suspend keys (F5, F6, F8, F9) and that row give the message
  "Suspend points are unavailable in 64-bit builds. Campaign saves work as
  usual." (also logged as a `ge: suspend:` line); play carries on. Campaign saves, profiles and settings
  are the same files at both widths (a 32-bit Windows campaign save loads).
- **`ge: lp64:` lines.** A 64-bit build logs informational lines: `address
  space OK`, `audio bank`, `music sequence table`, `bg tables` and `peak POOL
  USED of SIZE bytes` (each pool's largest use, at each stage unload and at
  exit). Any other `ge: lp64:` line marks a failure (a refused converter, an
  exhausted pool); `INFORMATIONAL_PREFIXES` in `lp64_boot.py` is the list
  the checks use.
- **Stops by name.** A host address below 4 GB, an unconverted asset class,
  a file the ROM does not serve, a missing ROM segment, a converter that
  refuses a file or an exhausted fatal pool stops the game with the log's
  sentence and a nonzero exit status (`AGENTS.md` and FORK-089's Task 6 note
  have the cases). The build also asks SDL for a message box with that
  sentence, unless the run has no one to dismiss it, but nobody has yet seen
  the box on a real display, and under WSLg SDL cannot open a message box at
  all (`No message system available`, FORK-092), so there the notice is the
  log line only.
- **The play harness** is compiled in (decision 4 of the LP64-007 plan) and
  reached only through environment variables; `build-inputs.json` records
  `play_harness: true`.

## Known limits

- **NVIDIA's WSL driver crashes at exit.** Every quit of the x86_64 package
  on NVIDIA's WSL driver ends in a segfault in `libnvwgf2umx.so` (status
  139), because the game exits with its GL context alive. This is an open
  defect, not fixed; its fix needs that driver and is a follow-up for a
  session on the laptop.
- **WSL needs two variables for the GPU.** Under WSLg the x86_64 package
  reaches the GPU through Mesa's d3d12 driver only with
  `GALLIUM_DRIVER=d3d12` (with Mesa 25.2.8; otherwise `llvmpipe`) and, beside
  an Intel chip, `MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA`. Its frames are
  then paced by WSLg's presentation (78-98 fps at 1920x1080).
- **Mouse-look cannot be played under WSLg** (reversed motion that stops at
  the screen edge; FORK-091). A mission played to its end, Jungle walked
  through and a mission with the HD pack were not done in Task 10's session.
- **The i686 package on a current system.** Task 10 measured the i686
  package's bundled `libstdc++` (GLIBCXX_3.4.30) as older than Mesa 25's
  i386 `libLLVM` needs (3.4.32): on Ubuntu 24.04 with Mesa 25.2.8 no GLX
  window opened until the system's `libstdc++` and `libgcc_s` were preloaded
  (`LD_PRELOAD=/usr/lib/i386-linux-gnu/libstdc++.so.6:/usr/lib/i386-linux-gnu/libgcc_s.so.1`).
  By the owner's decision of 2026-11-26, Task 11 makes the i686 packages take
  those two from the system too (`SYSTEM_CXX_RUNTIME` in
  `package_linux.py`), so the player's system needs its 32-bit C++ runtime
  (GCC 12 or newer; `libstdc++6:i386` and `libgcc-s1:i386` on Ubuntu or
  Debian): by `readelf -V`, build 416's i686 `IronSights` needs at most
  `GLIBCXX_3.4.30` and `GCC_7.0.0`, and no other packaged file needs
  either. The fix is not yet confirmed on
  a system with Mesa 25; the laptop can confirm it with the published
  package. WSL also has no 32-bit GPU path, so the i686 package draws with
  `llvmpipe` there.
- **Steam Deck.** `Iron-Sights-steamdeck-x86_64.zip` has not been run on a
  Steam Deck; `STEAM-DECK-x86_64.txt` says so.

## What is not built

No Windows x64, macOS (including Apple TV) or aarch64 package is built or
published. The 64-bit build compiles and runs at aarch64 in the development
containers. The macOS build has its own [plan](macos_plan.md), accepted on
2026-11-27 and not yet started.
