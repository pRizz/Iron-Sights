# MAC-001 Headless on Apple Clang Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-27. Not yet accepted: the owner's answers to
"Decisions for the owner" at the end are open, and the plan is written for
the recommendations. Nothing is built.

**Goal:** The 64-bit headless game builds with Apple clang on Apple Silicon
macOS, boots, plays its automated sets, passes the sanitizers and matches
the aarch64 Linux build's simulation, without changing any other build and
without touching a strict-tier file.

**Architecture:** The measurements below show the distance is short: seven
guarded edits and two host functions make the tree compile, link, boot and
play all 20 missions. What remains is making that state checkable and
honest on a second compiler: clang's warnings named, the harnesses and
static checks made to run (one of them passes today without reading
anything), clang's stricter sanitizer checks decided, and three maths
functions that differ from Linux's brought into the comparison build so the
two simulations can be compared whole. macOS-only code goes in one new file
compiled only there; every edit to a shared file sits behind `__APPLE__`.

**Tech Stack:** C (Apple clang 21, Xcode 27.2), CMake 3.27 with Ninja,
Python 3 standard library, Bash test harnesses, the existing Linux
containers (`lp64_env.py`) for the reference builds and the Windows
comparison.

**Spec:** [macos_plan.md](macos_plan.md): "Design" (Build, Addresses and
timing, Maths, Landing), verification items 1 to 5, 8 and 9, milestone
MAC-001, and its decisions. Read it first, then `AGENTS.md` ("Repo-Local
Guidance", the 64-bit paragraphs), and
[linux_64bit.md](linux_64bit.md).

## What was measured for this plan (2026-11-27, `main` at 990f991)

On the owner's Mac (macOS 27.2.1, Apple M4 Max, Apple clang 21.0.0). A copy
of the tree (`git archive HEAD`) was edited and built in a scratch folder
outside the repository and then discarded; nothing below is on `main`. The
ROM was read through `GE_ROM`.

- **Seven edits and two functions build the headless game.** The plan's six
  compile causes, with their exact fixes as tried:
  - `include/PR/os.h:1013`: add `|| defined(__APPLE__)` to the condition
    that takes `<strings.h>`; `:1033`: skip the `sprintf` declaration when
    `sprintf` is a macro;
  - `include/bstring.h:28`: the same `<strings.h>` branch. This is a seventh
    site, hidden until `os.h` compiled (`src/game/zlib.c` includes it);
  - `src/game/image.c:2690`: the local `extern int sprintf`, the same way;
  - `port/include/asmsym.h:29-31`: Mach-O section names (`__DATA,__data`
    for the two data kinds, `__DATA,__const` for read-only);
  - `src/game/lv.c:225` and `src/game/spectrum.c:51`: `#pragma weak A = B`
    aliases do not exist on Mach-O; both files already have a `#define`
    form for the browser build, which macOS can share;
  - `port/src/romdata.c:87`: `__error()` in place of `__errno_location()`;
  - `port/src/port_lp64.c:29-33`: the executable's range from dyld's load
    commands, skipping `__PAGEZERO`;
  - `port/src/pcsched.c:1074,1083`: a sleep to an absolute
    `CLOCK_MONOTONIC` time built on `nanosleep`.
- **The link needs two things.** Apple's linker refuses `-Wl,-Map=`
  (`CMakeLists.txt:708`); it takes `-Wl,-map,FILE`. And 705 weak references
  are undefined: 702 file symbols of `assets/obseg/obseg.h` (a 64-bit build
  has no compiled-in assets) and `portHealthWide`, `portMpHudWide` and
  `portInputPadDigMoveP1` (video-only functions the headless build tests
  for). ELF resolves an undefined weak reference to null; Mach-O needs
  `-Wl,-undefined,dynamic_lookup`. With it the link succeeds, and `nm -m`
  shows all 705 as `weak external ... (dynamically looked up)` and no
  strong one. The linker prints one warning, `reducing alignment of section
  __DATA,__common from 0x8000 to 0x4000`: two common symbols of 0x8000
  bytes (`sp_main`, `decompressdata.s_huftWorkspace`); no source asks for
  that alignment.
- **It boots and plays.** `lp64_boot.py --expect-run --frames 600`: `OK: ran
  600 frames`. `audit_buffers.py --mission-smoke ... --frames 1800
  --fixed-tick --jobs 4`: 20 of 20 `PASS`. The log shows `memp arena: 64 MB
  at 0x10a348000`, above 4 GB. `lp64_play.py run --set
  demos,stages,stops,menus,tours,attract`: `OK: 117 of 117 cases`.
  `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt` on its
  folders: `OK: 292 pool and stage pairs within budget`. Nothing in these
  runs showed a dependence on the 4 KB page the tree had always run on
  (macOS arm64 pages are 16 KB). `port/src/main.c:821-830` already installs
  the crash handler for SIGBUS as well as SIGSEGV.
- **With no ROM it stops by name.** In an empty folder with `GE_ROM` unset
  and `GE_NOINPUT=1`, the executable logs `ge: romdata: NO ROM - tried
  ...` and exits with status 3.
- **The compile gate passes as it is.** `lp64_diagnostics.py gate` on the
  macOS tree: `360 files compiled, 0 failed`. Its `-ffp-contract=off` rule
  already names `arm64` (`lp64_diagnostics.py:66`).
- **One static check passes without reading anything.**
  `lp64_diagnostics.py narrowing --check` printed `OK: 0 narrowings in 0
  files`: its pattern (`lp64_static_checks.py:41`) reads GCC's wording,
  "conversion from ... may change value", and clang says "implicit
  conversion loses integer precision ... [-Wshorten-64-to-32]". Asked
  directly, clang reports 68 implicit narrowings of 64-bit integers in the
  headless C files, 33 of them from `uintptr_t`, `intptr_t` or `romptr_t`
  (in `bg.c`, `prop.c`, `front.c`, `lightfixture.c` and 14 others). GCC's
  check cannot see these: it loses the typedef's name after arithmetic,
  which its help text says. `unprototyped` stops with `cc: error: unknown
  argument: '-aux-info'`.
- **Clang prints 9,706 warnings in 19 classes** on a full headless
  comparison build, where GCC's log is clean: `microsoft-anon-tag` 8,148,
  `pointer-sign` 621, `unknown-warning-option` 362 (clang does not know
  `-Wno-discarded-qualifiers`), `empty-body` 217, `macro-redefined` 82,
  `comment` 81, `pragma-pack` 46, `implicit-enum-enum-cast` 29,
  `implicit-const-int-float-conversion` 29, `pointer-bool-conversion` 23,
  `unused-value` 21, `switch` 13, `format-security` 12, `parentheses` 9,
  `tautological-constant-out-of-range-compare` 5, `shift-op-parentheses` 2,
  `non-literal-null-conversion` 2, `logical-not-parentheses` 2,
  `parentheses-equality` 1.
- **The sanitizer build runs, and clang's checks are stricter.**
  `-DGE_SANITIZE=ON` configures, builds and passes the boot check.
  AddressSanitizer reported nothing in any run. The mission smoke fails 20
  of 20 on UndefinedBehaviorSanitizer reports GCC's build does not make.
  With reports made non-fatal, all 20 missions ran to their end and logged
  43 distinct source lines of six kinds:
  - 37 lines, array index past `struct coord2d[4]` (`stan.c`, `chrprop.c`,
    `propobj.c`): `rect4f.points` used as a longer polygon;
  - 1 line, array index past `struct hand[2]` (`gunfire.c`, strict tier);
  - `stan.c:782`, a record wider than the object it is read through
    (`prefix = &stan_prefix`, a decompilation artifact);
  - `stan.c:3472-3473`, member access through a null pointer (an
    `offsetof` written by hand in a debug trace);
  - `model.c:5959` (strict tier), `+= 1` on a null `uselessPointer`;
  - `bondview2.c:5505` (strict tier), `(s8)(frac * 255)` with 246.5, a
    float-to-integer cast out of range. GCC's `-fsanitize=undefined` does
    not include that check.
- **Harnesses.** Of the 46 `scripts/port/test_*.sh`, 7 pass on macOS as
  they are. The main cause is one: Apple clang accepts `-m32` in
  `harness_widths`'s probe (`harness_widths.sh:27`), so every harness tries
  a 32-bit half that cannot compile. With `MIDAS_HARNESS_WIDTHS=64` and the
  compile fixes, 21 pass and 25 fail, by first error: `ld: unknown options:
  --gc-sections` (9), clang's error for an implicit function declaration,
  which `-w` does not silence (7), `-msse2` passed outside `harness_flags`
  (3), `-Wno-builtin-declaration-mismatch` unknown under `-Werror` (3),
  `test_pointer_bits`'s canary expecting GCC's wording (1),
  `test_web_touch` needing `emcc` (1), `test_xwayland_pointer` linking X11
  code (1). What lies behind each first error was not measured.
- **Maths.** `lp64_libm_sweep.c` on macOS: the in-tree `tanf`, `atanf` and
  `cosf` give their pinned hashes (`e3ccc704`, `f160154d`, `4c162bf1`).
  The system's `sinf`, `expf` and `powf` give `3990ade5`, `027eb54f` and
  `e758d294`, against glibc's `cc151db4`, `8be3a4cd` and `648805d1` on all
  three Linux hosts (FORK-089, LP64-006 Task 4). The game calls `sinf` 154
  times in its sources, `powf` 20 and `expf` 7.
- **Equivalence.** A macOS comparison build (`-DGE_COMPARE=ON`) ran
  `lp64_equiv.py run --set demos,solo,mp,menus,reload --repeat 2`: `OK: 38
  of 38 cases ran`, `OK: 38 of 38 cases agree with themselves`. Against the
  aarch64 Linux comparison build at 990f991, run the same way in its
  container, `compare --streams sim`: 33 streams identical, 5 different
  (`stage-facility` from tick 635, `stage-silo` 648, `stage-train` 3145,
  `stage-aztec` 2414, `temple-4p` 2929).
- **The cause of those five is the three maths functions.** With one
  `sinf`, `expf` and `powf` given to both builds (each the double function
  rounded to float, which gave equal sweep hashes on both hosts: `81d295da`,
  `30a1a4f5`, `1c4c5c98`), the five cases were rerun on both and all five
  were identical over their 3,897 ticks. That function is an experiment's
  stand-in, not a design: it changes the Linux builds' streams too.

### What could not be measured

- Whether Arm's published `sinf`, `expf` and `powf` (decision 3) reproduce
  glibc's bits. Nothing was downloaded for this plan.
- The play sets and the sanitizer smoke beyond the first report kinds: more
  kinds may appear once these are settled.
- The second layer of each harness failure.
- A GitHub macOS runner: its label, its Xcode and whether Ninja is on it.

## What MAC-001 owns, and what it leaves

It owns verification items 2, 4, 5 and 8 of the macOS plan, the headless
halves of 1 and 3, and 9. It leaves to MAC-002 everything under
`port/fast3d/`, the 16 `STRICT_FILES`, SDL, the window and the video
halves. Three of its findings are in strict-tier files (`gunfire.c`,
`model.c:5959`, `bondview2.c:5505`); it records them and hands them over.

## Global Constraints

- **No other build changes.** Before every commit that touches a file a
  32-bit build compiles, `python3 scripts/port/compare_win32.py commits HEAD
  WORKTREE` exits 0 (`identical in loaded code and data`). Every edit to a
  shared source or header is inside `#if defined(__APPLE__)`, or extends an
  existing condition with `|| defined(__APPLE__)`, so the other compilers
  see the tokens they see today.
- **No strict-tier file is edited:** nothing under `port/fast3d/`, and none
  of `port/src/port_arm_mesh.c`, `port_native_arms.c`, `port_grip_mesh.c`,
  `port_wardrobe.c`, `ads_hands.c`, `port_knife_ads.c`, `port_graphics.c`,
  `port_shiny.c`, `port_weapon_images.c`, `port_hud_weapons.c`,
  `joy_port.c`, `src/game/gunfire.c`, `model.c`, `bondview2.c`,
  `scripts/port/hand_gallery.py` and its template. A fix that needs one is
  recorded for MAC-002, not made.
- **macOS code lives in `port/src/port_macos.c`** (declared in
  `port/include/port_macos.h`), compiled only when CMake's `APPLE` is true,
  with `-Wall -Wextra -Werror`. Shared files call it from `__APPLE__`
  branches.
- **Upstream-owned files get the smallest guard**: no reformatting, no
  renaming, no moved code (`include/PR/os.h`, `include/bstring.h`,
  `src/game/*.c`).
- **Shipped maths is untouched.** Task 8 changes only the comparison build
  (`port/src/port_compare_math*.c`, compiled only with `-DGE_COMPARE=ON`).
- **Every commit updates `FORK_CHANGES.md`** (FORK-093): what changed, the
  counts before and after, and what ran.
- macOS build trees and run output live under `build/mac/` (ignored by
  Git, `.gitignore:212`), ROM-derived output never leaves it, and the
  folder is deleted before each push. No ROM-derived data in Git. Python
  uses the standard library only. Shell scripts start with
  `#!/usr/bin/env bash` and `set -euo pipefail`. New source files stay
  under 500 lines.
- Delivery is direct to `main`, one push per task after its review is
  clean. Never `git commit --no-verify`. Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

### Task check

Every task ends by running this and recording the results in FORK-093.
`ROM` is the ROM's path. Steps 1 and 2 exist from Task 2.

```bash
# 1. The macOS headless build, its link check, the gate and the boot.
cmake -S . -B build/mac/headless -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build/mac/headless
python3 scripts/port/lp64_diagnostics.py gate build/mac/headless
python3 scripts/port/lp64_boot.py --rom "$ROM" --expect-run --frames 600 build/mac/headless/IronSights

# 2. The 20 missions on it.
python3 scripts/port/audit_buffers.py --mission-smoke build/mac/headless/IronSights --rom "$ROM" --frames 1800 --fixed-tick --jobs 4 --output build/mac/smoke/missions.json

# 3. The Linux 64-bit trees still pass their gate (x86_64 container; the
#    lines of AGENTS.md, "Repo-Local Guidance"), and aarch64 still boots.
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c '
  cmake -S . -B build/lp64/gate -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate --target ge_buildinfo &&
  cmake -S . -B build/lp64/gate-video -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate-video --target ge_buildinfo &&
  python3 scripts/port/lp64_diagnostics.py gate build/lp64/gate build/lp64/gate-video &&
  python3 scripts/port/lp64_diagnostics.py narrowing --check build/lp64/gate build/lp64/gate-video &&
  python3 scripts/port/lp64_diagnostics.py unprototyped --check build/lp64/gate build/lp64/gate-video'
python3 scripts/port/lp64_env.py run --arch aarch64 -- sh -c '
  cmake -S . -B build/lp64/boot -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON && cmake --build build/lp64/boot -j"$(nproc)" &&
  python3 scripts/port/lp64_boot.py --rom /rom/baserom.u.z64 --expect-run --frames 600 build/lp64/boot/IronSights'

# 4. The shipped Windows program is unchanged (before committing).
python3 scripts/port/compare_win32.py commits HEAD WORKTREE
```

Expected: step 1 builds with exit 0, prints `OK: 705 weak references, 0
strong` (the count may move with the tree; the strong count may not),
`0 failed`, and `OK: ran 600 frames`; step 2 prints 20 `PASS`; step 3
prints `0 failed` for both trees, both checks `OK`, and `OK: ran 600
frames`; step 4 exits 0. For a change to a harness or to code a harness
compiles, also run every harness line of CI's tests step in the container
it needs, as LP64-003's task check says. The browser build is checked by CI
after the push.

## Review Focus

1. A macOS guard leaks into another build (a shared macro now tests
   something else). Expected: the Windows comparison differs, or the Linux
   gate fails. (Task check, steps 3 and 4.)
2. A strong symbol is missing and the link hides it, because undefined
   symbols are now looked up at load. Expected: the link check fails naming
   it. (Task 2, Step 2.)
3. A check passes on clang because it cannot read clang's output.
   Expected: the tool refuses a compiler whose wording it does not know,
   and a test with clang's wording finds the planted narrowing. (Task 5.)
4. A harness passes on macOS only because its body was skipped. Expected:
   each harness prints the width it ran, a skip prints its reason, and the
   list of skips is exactly the plan's. (Task 4.)
5. The sleep returns early or late under a signal and the game's pace
   drifts. Expected: the host test's interrupted-sleep case, and the demos,
   which end at their measured ticks only at the right pace. (Tasks 1
   and 7.)

---

### Task 1: The harness helper and the two host functions

**Files:**
- Modify: `scripts/port/harness_widths.sh`,
  `scripts/port/test_harness_widths.sh`
- Create: `port/include/port_macos.h`, `port/src/port_macos.c`,
  `scripts/port/test_macos_host.c`, `scripts/port/test_macos_host.sh`
- Modify: `FORK_CHANGES.md`

**Interfaces:**
- `harness_widths`: on a compiler whose `-dumpmachine` contains `-apple-`,
  32 is never offered, whatever `-m32 -fsyntax-only` says.
- `int portMacImageRange(uintptr_t *lowest, uintptr_t *end);` returns 1
  and the span of the main executable's `LC_SEGMENT_64` commands with the
  slide applied, `__PAGEZERO` left out; 0 when it has none.
- `int portMacSleepUntil(const struct timespec *target);` sleeps until
  `CLOCK_MONOTONIC` reaches `target`. Returns 0 when it has (at once for a
  time already past), `EINTR` when a signal ended the sleep early: the
  values `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)` gives, so
  the callers' retry loops stay as they are.
- `test_macos_host.sh` sources `harness_widths.sh`, compiles the driver
  with `port_macos.c` and no game header, and prints `test_macos_host: PASS
  (64-bit)`. Off macOS it prints `test_macos_host: SKIP (not macOS)` and
  exits 0. It goes on `HARNESS_WORKFLOW_EXEMPT`
  (`test_harness_widths.sh`) with the reason "runs only on macOS; named in
  the macOS job from Task 9", and Task 9 takes it off.

- [ ] **Step 1: Write the failing helper test, then fix the helper**

In `test_harness_widths.sh`: with a fake compiler that accepts `-m32` and
answers `-dumpmachine` with `arm64-apple-darwin27.2.0`, `harness_widths`
prints `64` only. Run it: it fails with `32` then `64`. Fix
`harness_widths`. Expected on macOS: `bash
scripts/port/test_harness_widths.sh` passes; in the x86_64 container it
passes unchanged.

- [ ] **Step 2: Write the failing host tests**

`test_macos_host.c`, each a function that prints its name and aborts on a
failed check:
- `image range holds main`: `portMacImageRange` returns 1, `lowest <=
  (uintptr_t) main < end`, and the address of a file-scope variable is
  inside it;
- `image range is above 4 GB`: `lowest >= (uintptr_t) 1 << 32`;
- `sleep reaches a future time`: a target 20 ms ahead returns 0, and the
  clock then reads at least the target and less than the target plus
  250 ms;
- `sleep returns at once for a past time`: a target 1 s in the past
  returns 0 within 50 ms;
- `an interrupted sleep says EINTR`: with a `SIGALRM` handler installed
  without `SA_RESTART` and `setitimer` at 5 ms, a target 200 ms ahead
  returns `EINTR` before the target; calling again until 0 ends at or
  after the target.

Run: `bash scripts/port/test_macos_host.sh`. Expected: it fails to link
(`portMacImageRange` undefined).

- [ ] **Step 3: Implement `port_macos.c`**

The range walks the load commands of `_dyld_get_image_header(0)` with
`_dyld_get_image_vmaddr_slide(0)`. The sleep reads the clock, returns 0
for a time not ahead, else calls `nanosleep` once for the difference and
returns `EINTR` if it was interrupted.

- [ ] **Step 4: Run the tests, commit**

Run: `bash scripts/port/test_macos_host.sh`. Expected: `test_macos_host:
PASS (64-bit)`. In the x86_64 container: the `SKIP` line, exit 0, and
`bash scripts/port/test_harness_widths.sh` passes.

```bash
git commit -m "Add the macOS host functions and keep harnesses off 32 bits there"
```

---

### Task 2: Apple clang builds, links and boots the headless game

**Files:**
- Modify: `CMakeLists.txt`, `include/PR/os.h:1013,1033`,
  `include/bstring.h:28`, `src/game/image.c:2690`,
  `port/include/asmsym.h:29-31`, `src/game/lv.c:225`,
  `src/game/spectrum.c:51`, `port/src/romdata.c:84-88`,
  `port/src/port_lp64.c:29-33,119-156`, `port/src/pcsched.c:1074,1083`
- Create: `scripts/port/macos_link_check.py`,
  `scripts/port/test_macos_link_check.py`
- Modify: `scripts/port/test_pointer_bits.sh`,
  `.github/workflows/windows-release.yml` (one line for the new Python
  test, beside the other `test_*.py` lines), `FORK_CHANGES.md`

**Interfaces:**
- Consumes: `portMacImageRange`, `portMacSleepUntil` (Task 1).
- `CMakeLists.txt`, when `APPLE`: a `FATAL_ERROR` naming the reason unless
  `GE_POINTER_BITS` is 64 ("macOS builds are 64-bit only: configure with
  -DGE_POINTER_BITS=64") and `CMAKE_SYSTEM_PROCESSOR` is `arm64` ("macOS
  builds are Apple Silicon only"); `port/src/port_macos.c` added to the
  target with `-Wall -Wextra -Werror`; `-Wl,-map,` in place of `-Wl,-Map=`;
  `-Wl,-undefined,dynamic_lookup`; and a post-build run of the link check
  on the executable. No other platform's lines change.
- `macos_link_check.py EXECUTABLE`: runs `nm -m`, and for the lines ending
  `(dynamically looked up)` prints `OK: N weak references, 0 strong` and
  exits 0 when every one is `weak external`; else prints each strong name
  and exits 1. `strong_lookups(nm_text: str) -> list[str]` holds the
  parsing, so the test runs on any host.
- The Mach-O section names of `asmsym.h` are `.section __DATA,__data` for
  `ASM_BSS` and `ASM_DATA` and `.section __DATA,__const` for `ASM_RODATA`,
  with `ASM_SIZE` empty, as measured. A zero-fill section for `ASM_BSS` is
  the implementer's choice if it links and the boot check passes.

- [ ] **Step 1: Write the failing configure tests**

In `test_pointer_bits.sh`, run only when `uname` is `Darwin`: a configure
without `-DGE_POINTER_BITS=64` fails with "macOS builds are 64-bit only";
with it, the generated link line has `-undefined,dynamic_lookup` and
`-map,` and no `-Map=`. Run it on macOS: the first case fails (the
configure succeeds today).

- [ ] **Step 2: Write the failing link-check tests, then implement it**

`test_macos_link_check.py`: `strong_lookups` returns `[]` for
`"(undefined) weak external _CarmourguardZ (dynamically looked up)"`,
`["_missing"]` for `"(undefined) external _missing (dynamically looked
up)"`, and ignores `"(undefined) external _printf (from libSystem)"`.

- [ ] **Step 3: Make the tree compile**

Apply the guarded edits listed under "What was measured". In `port_lp64.c`
the Linux reader stays inside `#if defined(__linux__)` unedited and an
`#elif defined(__APPLE__)` branch fills the same `Lp64ImageRange` from
`portMacImageRange`; the `#error` stays for any other platform. In
`pcsched.c` each of the two `clock_nanosleep(CLOCK_MONOTONIC,
TIMER_ABSTIME, &s_PaceTarget, NULL)` calls gets an `__APPLE__` twin
calling `portMacSleepUntil(&s_PaceTarget)`.

Run: `cmake --build build/mac/headless`. Expected: every file compiles.

- [ ] **Step 4: Make it link, and check what the link hid**

Expected: exit 0, `OK: 705 weak references, 0 strong`, and one linker
warning (the `__common` alignment). Record the weak count and the warning
in FORK-093.

- [ ] **Step 5: Run the task check, commit**

Expected as the task check says, with the boot log holding `ge: lp64:
address space OK`.

```bash
git commit -m "Build, link and boot the 64-bit headless game with Apple clang"
```

---

### Task 3: Clang's warnings, each class named

**Files:**
- Modify: `CMakeLists.txt` (the 64-bit warning options, `:51-85`),
  `FORK_CHANGES.md`

**Interfaces:**
- The 64-bit options split by compiler: `-Wno-discarded-qualifiers` only
  for GCC; for clang (`CMAKE_C_COMPILER_ID` matching `Clang`), one named
  `-Wno-` for each class the macOS build prints, with its count in the
  comment beside the flags, as the GCC classes have. The three `-Werror=`
  pointer checks are the same for both. Never `-w`.

- [ ] **Step 1: Count on the plain tree**

Build `build/mac/headless` from clean with the log kept; count `[-W...]`
by class. Expected: the 19 classes of "What was measured" with counts near
its figures; record the actual ones.

- [ ] **Step 2: Read the classes that can mark a defect**

Before a class is switched off, read every site of `format-security`,
`pointer-bool-conversion`, `tautological-constant-out-of-range-compare`,
`implicit-const-int-float-conversion`, `non-literal-null-conversion`,
`logical-not-parentheses`, `parentheses-equality`, `shift-op-parentheses`,
`switch`, `unused-value`, `implicit-enum-enum-cast` and `macro-redefined`
(229 sites as measured). For each class write one line in FORK-093: its
count, and either "idiom: ..." or each site that is a defect with what it
does. A defect is recorded, not fixed here, unless its fix is a macOS-only
line.

- [ ] **Step 3: Name the classes, rebuild**

Expected: the macOS build log has no compiler warning; the x86_64
container's gate trees compile with the flags they had
(`lp64_diagnostics.py gate` passes, and its entries show
`-Wno-discarded-qualifiers` still present there).

- [ ] **Step 4: Run the task check, commit**

```bash
git commit -m "Name each warning class Apple clang prints at 64 bits"
```

---

### Task 4: The harnesses run on macOS

**Files:**
- Modify: `scripts/port/harness_widths.sh`,
  `scripts/port/test_harness_widths.sh`, and the 25 harnesses below
- Modify: `FORK_CHANGES.md`

**Interfaces:**
- `harness_widths.sh` gains, each tested in `test_harness_widths.sh` with
  a fake compiler:
  - `harness_gc_flags`: prints `-ffunction-sections -fdata-sections
    -Wl,--gc-sections`, or on an `-apple-` compiler `-Wl,-dead_strip`;
  - `harness_quiet_flags`: prints `-w`, and on a clang compiler also
    `-Wno-error=implicit-function-declaration -Wno-error=int-conversion`
    (clang makes those errors that `-w` does not reach);
  - `harness_known_flag FLAG`: prints `FLAG` when the compiler accepts it
    under `-Werror`, else nothing;
  - `harness_skip_unless_tool NAME TOOL`: prints `NAME: SKIP (TOOL not
    installed)` and returns 1 when `TOOL` is missing.
- The harnesses change only where they name a flag: the nine with
  `--gc-sections` (`test_buffers`, `test_hit_sound`, `test_hud`,
  `test_inputrec_headless`, `test_lp64_compare`, `test_mission_cheats`,
  `test_mission_times`, `test_stan_callbacks`, `test_states`); the seven
  that rely on `-w` (`test_ads`, `test_dyn`, `test_lp64_typed_reads`,
  `test_save_widths`, `test_start_input`, `test_upstream_merge`,
  `test_wardrobe`); the three with a bare `-msse2` (`test_headless_configure`,
  `test_mema_order`, `test_monitor_script`); the three with
  `-Wno-builtin-declaration-mismatch` (`test_lp64_address`,
  `test_lp64_notice`, `test_romconv`); and `test_pointer_bits`, whose
  canary accepts either compiler's wording for an integer made from a
  pointer.
- Skips on macOS, each printed with its reason and nothing else skipped:
  `test_web_touch` (`emcc` not installed), `test_xwayland_pointer` (X11
  code, Linux only), and `test_lp64_address`'s one `-no-pie` case (Apple's
  linker cannot place an arm64 image below 4 GB).

- [ ] **Step 1: Write the failing helper tests, then the helpers**

- [ ] **Step 2: Convert the 25 harnesses by cause, one cause a commit**

After each cause, run all 46 on macOS and record how many print `PASS
(64-bit)`. A harness that now fails on something else is read before it is
changed: a difference in the game's code is a finding for FORK-093, not a
test to loosen.

- [ ] **Step 3: Every harness passes or is named**

Expected on macOS: 44 of the 46, and `test_macos_host`, print `PASS
(64-bit)`; `test_web_touch` and `test_xwayland_pointer` print their `SKIP`
line. In the x86_64, aarch64 and i686 containers every harness line of
CI's tests step passes as before, at the widths it ran before. A harness
that cannot be made to pass is listed in FORK-093 with its failure and
goes to "Decisions for the owner" of the next plan; the milestone does not
close on a silent skip.

- [ ] **Step 4: Run the task check, commit**

```bash
git commit -m "Run the test harnesses on macOS"
```

---

### Task 5: The static checks on clang

**Files:**
- Modify: `scripts/port/lp64_static_checks.py`,
  `scripts/port/lp64_diagnostics.py`,
  `scripts/port/test_lp64_diagnostics.py`
- Create: `scripts/port/lp64_narrowing_clang.txt`
- Modify: `FORK_CHANGES.md`

**Interfaces:**
- `narrowing`: also reads clang's line, `warning: implicit conversion
  loses integer precision: 'SRC' (aka 'X') to 'DST'`, compiling with
  `-Wshorten-64-to-32` when the entry's compiler is clang. When a tree's
  compiler is neither GCC nor clang by `--version`, it exits 2 saying so:
  it never prints `OK` for output it cannot read.
- `narrowing --check` on a clang tree compares with
  `lp64_narrowing_clang.txt` (`path<TAB>count<TAB>reason`, exact match),
  not with the GCC allowlist: clang keeps a typedef's name through
  arithmetic and so sees places GCC's check cannot.
- `unprototyped` on a clang tree exits 2 with "needs GCC's -aux-info: run
  it in the x86_64 or aarch64 container". `port_macos.c` is covered
  instead by its own `-Wall -Wextra -Werror`.

- [ ] **Step 1: Write the failing tests, then implement**

With the fake compiler the existing tests use: clang's wording for a
`uintptr_t` to `u32` conversion is listed; a compiler whose `--version`
names neither is refused with exit 2; `unprototyped` on a clang entry
exits 2 with the message.

- [ ] **Step 2: Record what clang sees**

Run `narrowing` on `build/mac/headless`. Expected: 33 places in 18 files
from `uintptr_t`, `intptr_t` or `romptr_t`; record the actual figures.
Read each: write `lp64_narrowing_clang.txt` with the reason for each file
("a difference of two addresses in one object", or what it is), and list
in FORK-093 any that carries a host address into 32 bits. Such a place is
a defect of the 64-bit build on every host and is fixed here only if the
Windows comparison stays identical; otherwise it is recorded for the
owner.

- [ ] **Step 3: Run the task check, commit**

`narrowing --check build/mac/headless` prints `OK`.

```bash
git commit -m "Make the narrowing check read clang, and refuse what it cannot read"
```

---

### Task 6: The sanitizers under clang

Written for decision 2's recommendation.

**Files:**
- Modify: `CMakeLists.txt` (the `GE_SANITIZE` block, `:466-492`),
  `src/game/stan.c:3472-3473`, `FORK_CHANGES.md`

**Interfaces:**
- With `GE_SANITIZE` on a clang compiler, the options add
  `-fno-sanitize=array-bounds,object-size,float-cast-overflow`, and
  `src/game/model.c` alone gets `-fno-sanitize=pointer-overflow`. The
  comment beside them gives each kind's sites and count as measured, and
  says that GCC's build never made these reports. GCC's options do not
  change.
- `stan.c:3472-3473` uses `offsetof(StandTilePoint, link)` and
  `offsetof(StandTile, points)`.

- [ ] **Step 1: Reproduce**

Build `build/mac/san` (`-DGE_POINTER_BITS=64 -DGE_SANITIZE=ON`) and run
the mission smoke. Expected: 20 `FAIL`, each log's first report at
`stan.c:782`.

- [ ] **Step 2: Fix the one line that is ours, name the rest**

The `offsetof` edit must leave the Windows comparison identical. Then add
the options.

- [ ] **Step 3: Run until no report**

Boot check, mission smoke and `lp64_play.py run --set
demos,stages,stops,menus,tours,attract --frames-scale 3` on the sanitizer
build. Expected: `OK: ran 600 frames`, 20 `PASS`, every case `PASS`. A new
report kind is handled by the same rule: read its sites, fix a line that
is the fork's and whose fix leaves Windows identical, otherwise name the
kind with its sites; a report from AddressSanitizer is never switched off.
Record in FORK-093 the three strict-tier sites for MAC-002.

- [ ] **Step 4: Run the task check, commit**

```bash
git commit -m "Run the sanitizer build under Apple clang"
```

---

### Task 7: The play sets and the pool budgets

**Files:**
- Modify: `FORK_CHANGES.md` only, unless a run finds a fault

- [ ] **Step 1: Run the sets on the plain build**

```bash
python3 scripts/port/lp64_play.py run --rom "$ROM" --set demos,stages,stops,menus,tours,attract --output build/mac/play/plain build/mac/headless/IronSights
python3 scripts/port/lp64_play.py peaks --budget scripts/port/lp64_budgets.txt build/mac/play/plain
python3 scripts/port/lp64_play.py traces --expect scripts/port/lp64_demo_traces.txt build/lp64/play/REF build/mac/play/plain
```

Expected: every case `PASS`; `OK: N pool and stage pairs within budget`;
the demo traces as `lp64_demo_traces.txt` expects of a 64-bit build
(`REF` is the i686 reference run, made as the 64-bit plan describes). A
pool past half its size is a finding for the owner: this plan resizes no
pool.

- [ ] **Step 2: Record, commit**

```bash
git commit -m "Record the macOS headless play sets"
```

---

### Task 8: One `sinf`, `expf` and `powf` for the comparison build

Written for decision 3's recommendation.

**Files:**
- Create: `port/src/port_compare_math_arm.c` (the three functions and
  their tables)
- Modify: `CMakeLists.txt` (compiled with `port_compare_math.c`, same
  conditions and `-ffp-contract=off`), `port/src/port_compare_math.c` (its
  header comment), `scripts/port/test_compare_math.c`,
  `scripts/port/test_lp64_compare.sh`, `tools/THIRD_PARTY_NOTICES.txt`,
  `FORK_CHANGES.md`

**Interfaces:**
- In a comparison build without `GE_COMPARE_LIBM`, `sinf`, `expf` and
  `powf` are the file's, computed in double with `+ - * /`, comparisons and
  table lookups, rounded once to float, under the same compile-time
  refusals as `port_compare_math.c` (no x87, `FLT_EVAL_METHOD` 0, no
  fast-math).
- Their sweep hashes are glibc's: `sinf cc151db4`, `expf 8be3a4cd`, `powf
  648805d1`, pinned in `test_lp64_compare.sh` beside the other three.

- [ ] **Step 1: Measure before writing anything**

Take `sinf`, `expf` and `powf` from Arm's optimized-routines (MIT) at a
recorded commit, build them alone with the sweep on macOS and in the
aarch64 container. Expected: all three hashes equal glibc's on both. If
any differs, stop: the recommendation's premise is false, and the
question goes back to the owner with the hashes.

- [ ] **Step 2: Pin the hashes (failing), then add the file**

`test_lp64_compare.sh` expects the three hashes; it fails on macOS today
(`3990ade5`, `027eb54f`, `e758d294`). Add the file, its notice entry
(name, source, commit, licence, "comparison builds only, never shipped")
and the CMake lines. `test_compare_math.c` gains, for each function, the
check the other three have: within two ulp of the C library's double
function over 100,000 inputs.

- [ ] **Step 3: The Linux comparison builds did not move**

Rebuild `build/lp64/cmp-a64` and `cmp-x64`, run `lp64_equiv.py run
--set demos,solo,mp,menus,reload --repeat 2` on each, and `compare
--streams sim --expect scripts/port/lp64_equiv_expected.txt` on the two
folders. Expected: `OK`, with `x64:a64` still without a line. (The i686
lines are exercised by their own runs, which this task does not repeat:
all three Linux hosts' libm functions had the hashes now pinned.)

- [ ] **Step 4: Run the task check, commit**

```bash
git commit -m "Give the comparison build one sinf, expf and powf"
```

---

### Task 9: The simulation matches the aarch64 Linux build

**Files:**
- Modify: `scripts/port/lp64_equiv_expected.txt` (its header: the `a64:mac`
  pair), `FORK_CHANGES.md`

- [ ] **Step 1: Run both, compare**

```bash
cmake -S . -B build/mac/cmp -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DGE_COMPARE=ON && cmake --build build/mac/cmp
python3 scripts/port/lp64_equiv.py run --rom "$ROM" --set demos,solo,mp,menus,reload --repeat 2 --output build/mac/equiv/RUN/mac build/mac/cmp/IronSights
python3 scripts/port/lp64_env.py run --arch aarch64 -- python3 scripts/port/lp64_equiv.py run --rom /rom/baserom.u.z64 --set demos,solo,mp,menus,reload --repeat 2 --output build/lp64/equiv/RUN/a64 build/lp64/cmp-a64/IronSights
python3 scripts/port/lp64_equiv.py compare --streams sim --expect scripts/port/lp64_equiv_expected.txt build/lp64/equiv/RUN/a64 build/mac/equiv/RUN/mac
python3 scripts/port/lp64_equiv.py sensitivity --rom "$ROM" --output build/mac/equiv/RUN/sens build/mac/cmp/IronSights
```

Expected: `OK: 38 of 38 cases ran` and `agree with themselves` on both;
the comparison identical in all 38, so the expected file gains only a
comment that `a64:mac` has no line, as `x64:a64` has; `sensitivity`
passes. A stream that still differs is traced to its first tick and
cause, by a run with the cause removed, before it gets a line; a line
carries no hash pin (the macOS plan's decision).

- [ ] **Step 2: Run the task check, commit**

```bash
git commit -m "Compare the macOS simulation with the aarch64 Linux build"
```

---

### Task 10: A macOS job in CI

Written for decision 4's recommendation.

**Files:**
- Modify: `.github/workflows/windows-release.yml`,
  `scripts/port/test_harness_widths.sh` (`test_macos_host` leaves
  `HARNESS_WORKFLOW_EXEMPT`), `FORK_CHANGES.md`

**Interfaces:**
- A job `macos-arm64-tests` on a GitHub Apple Silicon runner, its label
  and Xcode recorded in FORK-093. It configures and builds
  `build/mac/headless` (the link check runs in the build), runs
  `lp64_diagnostics.py gate` and `narrowing --check` on it, runs every
  harness (each prints `PASS (64-bit)` or its named `SKIP`), and runs the
  executable with no ROM and `GE_NOINPUT=1`, expecting exit status 3 and
  the log line `ge: romdata: NO ROM`. The `publish` job does not list it under
  `needs`: a red macOS job is visible and holds back no release.

- [ ] **Step 1: Check the job's commands in a fresh clone on the Mac**

Run each of the job's lines in a new clone with no ROM and no `build/`
folder. Expected: each passes as the job expects.

- [ ] **Step 2: Add the job, push, read the run**

Expected: the job passes; every other job is as before. A red job is this
task's to fix.

```bash
git commit -m "Build and test the macOS headless game in CI"
```

---

### Task 11: The guide and the close

**Files:**
- Create: `scripts/port/macos.md`
- Modify: `AGENTS.md` (one paragraph: the macOS trees, the task check's
  step 1, the link check, the clang forms of the static checks),
  `scripts/port/macos_plan.md` (status table, MAC-001 row),
  `FORK_CHANGES.md`

- [ ] **Step 1: Write the guide's first part**

How a developer builds and checks the headless game on macOS, from the
task check; what it lacks so far (no window); where its output lives. It
is finished in MAC-006.

- [ ] **Step 2: Close**

Run the whole task check, the harnesses, Tasks 6, 7 and 9's runs once
more at the closing commit. Fill the status row with what ran and its
figures, and the hand-over to MAC-002: the three strict-tier sanitizer
sites, any defect Tasks 3 and 5 recorded, the weak-reference count, and
what the video tree's 14 failing files need. Delete `build/mac/`.

```bash
git commit -m "Close MAC-001: the headless macOS build"
```

---

## What this plan does not do

- It opens no window and compiles nothing under `port/fast3d/`.
- It does not fix what clang found in strict-tier or upstream-owned code
  beyond one `offsetof`; it records it.
- It does not make the Linux static checks stricter: the 33 narrowings
  clang sees stay invisible to GCC's check.
- It does not package, sign or report crashes.
- It does not run the i686 comparison again.

## Decisions for the owner

Open. Each has the recommendation the plan is written for and the
alternative.

1. **What "done" means.** The macOS plan says MAC-001 is done when a
   headless tree builds with Apple clang and passes verification 2, 4, 5
   and 8, the headless halves of 1 and 3, and 9. *Recommended:* as
   written, with three readings the measurements force: "verification 5"
   means no report under decision 2's check set; "verification 4" means
   all 38 streams identical to aarch64 Linux's after decision 3, with no
   line in the expected file; "verification 8" means 44 harnesses pass
   and two are skipped by name (`test_web_touch`, `test_xwayland_pointer`).
2. **Which sanitizer checks run under clang.** Clang's undefined-behaviour
   checks report 43 source lines that GCC's build never did; none is a
   memory error AddressSanitizer saw. *Recommended:* switch off by name
   the three kinds that fire on decompiled idioms (`array-bounds`,
   `object-size`, `float-cast-overflow`), `pointer-overflow` for `model.c`
   alone, fix the one line that is the fork's, and record the rest. The
   alternative is to fix the sites: 40 lines in four upstream-owned files
   and three in strict-tier files, each needing the Windows comparison,
   the strict ones a laptop landing. That is a cleaner tree and a much
   larger milestone.
3. **How the comparison build gets one `sinf`, `expf` and `powf`.**
   macOS's differ from glibc's, and five of 38 simulation streams differ
   because of them (measured both ways). *Recommended:* take the three
   from Arm's optimized-routines under its MIT licence, the routines
   glibc's own are derived from, pinned to glibc's hashes, with a notice
   entry; they are compiled only in comparison builds and never shipped.
   If Task 8's first step finds they do not reproduce glibc's bits, the
   question returns to you. The alternative is to write our own, as
   `tanf`, `atanf` and `cosf` were: that changes the Linux comparison
   streams wherever these functions feed the game, so the twelve pinned
   Jungle lines would have to be traced again, including the i686 video
   runs under emulation. A third way, leaving the system's functions and
   listing the five differences, contradicts the macOS plan's decision and
   leaves those five cases unjudged after their first difference.
4. **A macOS job in CI now.** *Recommended:* add it in this milestone
   (Task 10): the repository is public, so the runner costs nothing, and
   without it any Windows or Linux change can break the macOS build
   unseen until MAC-006. It never holds back a release. The alternative is
   to wait for MAC-006 as the macOS plan's outline has it.
5. **Clang's 33 narrowings.** *Recommended:* read and list them now, with
   an exact-match allowlist for clang trees (Task 5), and fix here only
   those that carry a host address into 32 bits and leave Windows
   identical. The alternative is to leave the check GCC-only and say so.
6. **Undefined weak references.** *Recommended:* dynamic lookup with the
   link check, as measured. The alternative, naming all 705 symbols to
   the linker, is exact without a check but needs a generated list.
7. **No 32-bit half on macOS.** Apple Silicon cannot run 32-bit code, so a
   harness's 32-bit half is never tried there. *Recommended:* accept; the
   containers and CI keep running both.
