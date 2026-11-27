# The native macOS build

Iron Sights builds as a native 64-bit program on Apple Silicon macOS with
Apple clang. The work and its decisions are in the
[macOS plan](macos_plan.md), [MAC-001's plan](macos_mac_001.md) and the
inventory's FORK-093. This guide is for developers: how the macOS game is
built and checked today, and what it does not have yet. It is finished in
MAC-006.

**What exists (MAC-001): the headless game.** It has no window, no sound
and no input: it loads the ROM, runs the simulation and logs. That is
enough to boot, to run the 20 missions and the play sets, to run under the
sanitizers, and to compare its simulation with the Linux builds'. The
window, picture, sound and input are MAC-002; packaging, crash reporting
and signing are MAC-004 to MAC-006. Nothing on macOS is shipped.

## Where it has run

- **The owner's Mac:** Apple M4 Max, macOS 27.2.1, Xcode 27.2 (Apple clang
  21.0.0, `clang-2100.4.1.1`), CMake 3.27, Ninja, Homebrew Bash 5.
- **CI:** GitHub's Apple Silicon runner, label `macos-26` (image
  `macos-26-arm64`: macOS 26.7.2, Xcode 26.6, Apple clang 21.0.0
  `clang-2100.1.1.101` when the job first ran), in the job
  `macos-arm64-tests` of `.github/workflows/windows-release.yml`. It has no
  ROM, so it builds and checks but never plays; the `publish` job does not
  wait for it.

Nothing has run on an Intel Mac, and CMake refuses to configure one.

## What you need

- Xcode's command-line tools (Apple clang, `nm`), CMake 3.16 or later,
  Ninja and Python 3.
- Bash 4 or later for the harnesses (`brew install bash`; the system's is
  3.2) and, for `test_start_input` alone, SDL2 (`brew install sdl2`).
- The US ROM, for anything that plays. Pass it with `--rom` or `GE_ROM`;
  it is never copied into the tree. The build itself needs no ROM.
- For the comparison with Linux, and for the Linux and Windows halves of
  the check below, the containers of `scripts/port/lp64_env.py` (Docker).

## Build and check

Build trees and run output live under `build/mac/`, which Git ignores.
Game logs and anything else made with the ROM stay there.

```bash
ROM="$PWD/GoldenEye 007 (U) [!].z64"

# Configure and build. The link check runs after every link (see below).
cmake -S . -B build/mac/headless -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mac/headless

# The 64-bit compile gate and clang's narrowing check.
python3 scripts/port/lp64_diagnostics.py gate build/mac/headless
python3 scripts/port/lp64_diagnostics.py narrowing --check build/mac/headless

# Boot 600 frames, then the 20 missions.
python3 scripts/port/lp64_boot.py --rom "$ROM" --expect-run --frames 600 build/mac/headless/IronSights
python3 scripts/port/audit_buffers.py --mission-smoke build/mac/headless/IronSights --rom "$ROM" \
    --frames 1800 --fixed-tick --jobs 4 --output build/mac/smoke/missions.json

# Every harness, and the stop without a ROM (these two need no ROM).
bash scripts/port/macos_harnesses.sh
bash scripts/port/macos_no_rom.sh build/mac/headless/IronSights
```

Expected: the build prints `OK: N weak references, 0 strong` (705 when this
was written) and no compiler warning; the gate `N files compiled, 0
failed`; `OK: 33 narrowings in 19 files, all allowed`; `OK: ran 600
frames`; 20 `PASS`; `macos_harnesses: P passed, 2 skipped by name, 0
failed`; `macos_no_rom: OK (exit 3, ...)`.

A macOS configure must be 64-bit (`-DGE_POINTER_BITS=64`) and arm64; CMake
says so otherwise. Always pass `--output` under `build/mac/` to
`audit_buffers.py` (its default folder is the Windows tree's).

### The play sets

```bash
python3 scripts/port/lp64_play.py run --rom "$ROM" --set demos,stages,stops,menus,tours,attract \
    --output build/mac/play/plain build/mac/headless/IronSights
python3 scripts/port/lp64_play.py peaks --budget scripts/port/lp64_budgets.txt build/mac/play/plain
```

They are paced in real time, about a quarter of an hour, and need a quiet
machine: a demo ends at its measured tick only at the right pace.
`lp64_play.py traces --expect scripts/port/lp64_demo_traces.txt DIR_REF
build/mac/play/plain` compares the demos with an i686 reference run, made
in the x86 container as [the Linux guide](linux_64bit.md) and FORK-089
describe.

### The sanitizer build

```bash
cmake -S . -B build/mac/san -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_SANITIZE=ON
cmake --build build/mac/san
```

Run the boot check, the mission smoke and the play sets on
`build/mac/san/IronSights` (the play sets with `--frames-scale 3`, about 50
minutes). A report is fatal. Under clang, three undefined-behaviour checks
are switched off by name (`array-bounds`, `object-size`,
`float-cast-overflow`) and a fourth for `src/game/model.c`
(`pointer-overflow`): they fire on 41 decompiled lines that GCC's build
never reported. The comment beside `GE_SANITIZE` in `CMakeLists.txt` lists
the sites; three are in strict-tier files and belong to MAC-002. A report
from AddressSanitizer is never switched off.

### The comparison with Linux

A comparison build (`-DGE_COMPARE=ON`, never shipped) computes its six
single-precision maths functions itself, so that its simulation can be
compared tick for tick with another host's: `tanf`, `atanf` and `cosf`
from `port/src/port_compare_math.c`, and `sinf`, `expf` and `powf` from
Arm's optimized-routines (`port/src/port_compare_math_arm.c` and
`port_compare_math_arm_sinf.c`, MIT). macOS is why the last three are
there: its C library rounds them otherwise than glibc, and five of the 38
streams differed until both builds used Arm's.

```bash
cmake -S . -B build/mac/cmp -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 \
    -DGE_HEADLESS=ON -DGE_COMPARE=ON
cmake --build build/mac/cmp
python3 scripts/port/lp64_equiv.py run --rom "$ROM" --set demos,solo,mp,menus,reload --repeat 2 \
    --output build/mac/equiv/RUN/mac build/mac/cmp/IronSights
```

Expected: `OK: 38 of 38 cases ran` and `OK: 38 of 38 cases agree with
themselves`. Make the same run of `build/lp64/cmp-a64` in the aarch64
container ([the Linux guide](linux_64bit.md)) into
`build/lp64/equiv/RUN/a64`, then:

```bash
python3 scripts/port/lp64_equiv.py compare --streams sim \
    --expect scripts/port/lp64_equiv_expected.txt build/lp64/equiv/RUN/a64 build/mac/equiv/RUN/mac
```

Every stream must be identical: `a64:mac` has no line in
`lp64_equiv_expected.txt`, and none may be added without tracing the
difference to its cause. The streams are ROM-derived and stay under
`build/`. Only the simulation is compared; pictures and sound wait for
MAC-002. The three Arm functions are not glibc's bits on every input:
glibc fuses multiply-adds on aarch64 and on x86_64 processors with FMA,
and differs from them, and from its own i686 build, by one unit in the
last place on a handful of inputs (FORK-093 has the counts). A comparison
build never fuses, so every host now agrees on all of them.

## What is different on macOS

- **macOS code is in one file.** `port/src/port_macos.c`
  (`port/include/port_macos.h`) is compiled only on macOS, under `-Wall
  -Wextra -Werror`: the executable's loaded range, from dyld, for the
  64-bit address check, and a sleep to an absolute time, for the frame
  pacing (macOS has no `clock_nanosleep`). `scripts/port/test_macos_host.sh`
  tests both. Every edit to the game's and the port's shared sources is
  behind `__APPLE__`, or CMake's `APPLE` or a test of the compiler being
  clang, so the other builds compile what they compiled before; a change
  to a shared file still owes the Linux gate and `compare_win32.py`. (The
  harness scripts and the static-check tools changed for every host, each
  neutral or stricter there.)
- **The link check.** A 64-bit build has no compiled-in assets, so several
  hundred weak references are undefined. ELF makes such a reference null
  when it links; Mach-O does so only with `-Wl,-undefined,dynamic_lookup`,
  which would also let a strong undefined symbol through to fail when the
  game first uses it. `scripts/port/macos_link_check.py` runs after every
  link and fails the build naming any strong one.
- **Warnings.** Clang prints 18 classes of warning where GCC's log is
  clean. `CMakeLists.txt` switches each off by name, with its count; a new
  class in the macOS log is fixed or named there with its count, never
  `-w`. The link prints one warning, about the alignment of `__common`.
- **The static checks.** `narrowing` reads clang's wording and checks a
  clang tree against `scripts/port/lp64_narrowing_clang.txt`: clang keeps a
  typedef's name through arithmetic, so it lists places GCC's check cannot
  see. A new place fails until it is fixed, or read and given its line and
  reason. `unprototyped` needs GCC and exits 2 on a clang tree; run it,
  `lp64_pointer_types.py`, `lp64_lto.py` and the rest of CI's tests step in
  the x86_64 container (`AGENTS.md`, "Repo-Local Guidance").
- **Harnesses.** A Mac has no 32-bit half: `harness_widths` offers 64
  only, and harnesses that read a 32-bit tree say what they skipped.
  Flags that differ by compiler or linker come from helpers in
  `scripts/port/harness_widths.sh` (`harness_gc_flags`,
  `harness_quiet_flags`, `harness_game_header_flags`,
  `harness_weak_undefined`, `harness_host_sources`, `harness_wrap_flags`);
  a new harness uses them instead of naming a GNU flag.
  `macos_harnesses.sh` holds every line that speaks of skipping, in any
  spelling, to its list: two harnesses skip whole (`test_web_touch` without
  `emcc`, `test_xwayland_pointer`), and five skip a part (the 32-bit and
  MinGW cases of `test_pointer_bits` and `test_abi_probe`, the other
  width's half of `test_save_widths`, `test_lp64_address`'s `-no-pie` case,
  two Windows-only tests of `test_save_transfer`). A new skip fails the
  run until it is read and listed. Off macOS a missing tool is never a
  reason to skip.
- **Paths through a symlink.** `/tmp` and `/var` are symlinks on macOS. A
  tree configured under one records its files that way; the static checks
  compare real paths, so they read it.

## What it lacks so far

- A window, picture, sound and input (MAC-002), and everything under
  `port/fast3d/`: a headless build compiles none of it.
- The platform features of the Linux packages (the player's folder,
  downloads, the weapon icon worker, file pickers): MAC-003.
- A package, crash reporting, a signature: MAC-004 to MAC-006.
- Suspend points, as every 64-bit build.
