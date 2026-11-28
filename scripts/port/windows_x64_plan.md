# Native Windows x64 build plan

Iron Sights ships for Windows only as a 32-bit program
(`Iron-Sights-windows-x86.zip`). This plan makes the 64-bit source, which
already ships for x86_64 Linux, build and run as a native 64-bit Windows
program, and ends with a 64-bit download beside the 32-bit one. It builds on
the [64-bit port](native_64bit_plan.md) (done on 2026-11-26), which listed
Windows x64 as a non-goal that it must not block. It is independent of the
[macOS plan](macos_plan.md); neither waits for the other.

**Drafted and accepted on 2026-11-27.** The owner accepted the plan with
all five recommendations ("Accept the plan with your recommendations");
[Owner's decisions](#owners-decisions) lists them with the alternatives not
taken. The owner then asked for the milestones to be carried out ("keep
working on these plans and implementations"); the
[milestone status](#milestone-status) table says where they stand.

This plan was drafted under `AGENTS.md` (the fork inventory, the two-tier
gallery rule, render performance, load before play, direct-to-main
delivery), `standards-overrides.md`, the 64-bit plan's decisions and its
"After the 64-bit plan" section, and FORK-014, FORK-018 and FORK-089 of
`FORK_CHANGES.md`.

## Why

- Windows is the platform the fork has always shipped first, and it gets
  only the 32-bit build. On 64-bit Linux, the x86_64 package was faster
  than the i686 one in every benchmark cell on the same renderer (LP64-007
  Task 10, FORK-089); whether Windows gains the same is not measured.
- A 32-bit Windows process has 2 GB of address space. The game already scans
  for free ranges because a 32-bit GL driver or an overlay can take all of
  the fixed hints (`port/src/pcsched.c:91-99`). A 64-bit process does not
  have that ceiling.
- The engine work is done. What is left is Windows' own layer, its tooling
  and its release, and the probe below measured that this is small.

## Goals

1. A 64-bit Windows executable, built by MinGW-w64 GCC from this
   repository, that plays the whole game: every solo stage, multiplayer, the
   menus, the watch and the fork's own features, with sound, keyboard, mouse,
   gamepad and raphnet adapters.
2. It is the same game as the 64-bit Linux build: its simulation matches the
   x86_64 Linux comparison build tick for tick, or each difference is traced
   and recorded.
3. It has what the 32-bit Windows build has, except suspend points (see
   [decision 2](#owners-decisions)): crash reporting with symbols, the
   profiler, HDR output, the Enhanced font and HD texture downloads, the
   weapon icon cache, save backup, restore and transfer.
4. Players get `Iron-Sights-windows-x64.zip`, published and attested by CI
   beside the 32-bit zip.
5. The 32-bit Windows, Linux and browser builds and the 64-bit Linux build
   are unchanged in behaviour.

## Non-goals

- Windows on ARM (arm64). It runs x64 programs under emulation; a native
  arm64 build is a later plan if there is a reason.
- An MSVC or clang-cl build of the game. Sentry's SDK stays the one CI
  already builds with MSVC.
- Suspend points at 64 bits. The 64-bit plan decided they are unavailable in
  every 64-bit build; a relocating suspend format would be its own project.
- Retiring the 32-bit Windows build. It stays the reference, the build that
  has suspend points, and the gallery's capture build.
- A Microsoft Store, MSIX or installer package. The zip stays the format.
- Code signing with an Authenticode certificate (the 32-bit zip has none;
  FORK-018).

## Anti-goals

These would make the project a failure even if a 64-bit exe runs:

- Any change in the 32-bit Windows program's loaded code or data that is not
  named function by function (`scripts/port/compare_win32.py`).
- A 64-bit exe that keeps a game address below 4 GB, silently or not.
- A crash handler that misreports or truncates a 64-bit address.
- ROM-derived data or generated game data in Git.
- A change to a shipping build's maths, pools or timing made to suit Windows
  x64 without the owner's say.

## Constraints

- **One CMake refusal.** `CMakeLists.txt:19-21` stops a `WIN32` configure at
  `GE_POINTER_BITS=64`, and `scripts/port/test_pointer_bits.sh:177-182`
  asserts that stop (its case 5, with the i686 toolchain file). Lifting it is a deliberate change to the 64-bit plan's
  scope and updates that test.
- **The host-pointer rule.** At 64 bits, an address is the host's when any of
  its upper 32 bits is set (`port/include/port_hostptr.h:40-41`), so every
  game address must be above 4 GB. The 32-bit Windows rule is "above
  0x10000000", met with a fixed image base, `--disable-dynamicbase` and
  fixed `VirtualAlloc` hints (`CMakeLists.txt:136-138`, `pcsched.c:459-470`).
- **LLP64.** Windows x64 keeps `long` at 32 bits; Linux x86_64 makes it 64.
  Every 64-bit gate so far ran on Linux GCC (`lp64_diagnostics.py`, the ABI
  probe in `scripts/port/test_abi_probe.sh:11-15`), so none has checked the
  Windows ABI: 4-byte `long`, 2-byte `wchar_t`, `-mms-bitfields` (MinGW's
  default), and `long long unsigned int` as `uintptr_t`'s spelling, which
  `scripts/port/lp64_static_checks.py:8-9,33-35` does not expect.
- **Strict tier.** This clone can render: `midas.galleryDefer` is unset and
  the owner's laptop regenerates `review/hand-gallery`, so a strict-tier
  change is reviewed and landed here, as usual. The gallery's capture build
  stays the 32-bit exe. The probe below needed no edit under `port/fast3d/`.
- **The ROM** is in `build_win/Data`, ignored by Git. CI has no ROM, so every
  check that plays the game runs on the laptop.
- **Recorded decisions stand:** widen pointers; write our own; runtime ROM
  loading only at 64 bits (`GE_ROMDATA_FALLBACK=OFF`); no suspend points at
  64 bits; a shipping build's maths does not change without the owner's say;
  the two-tier gallery rule.

## What was measured

On 2026-11-27, `main` at 990f914, on the owner's laptop (Windows 11, NVIDIA
GeForce RTX 3070 Laptop GPU), in a throwaway copy of the `goldeneye` image
with Ubuntu's `gcc-mingw-w64-x86-64` (GCC 13-win32) added, building from
`git archive HEAD` in the container's `/tmp`. The probe's toolchain file,
script and patch were scratch files under `build/win64-probe/` (ignored by
Git, deleted after the probe); nothing in the repository was changed.

1. **The refusal is the first stop.** An x86_64 MinGW toolchain file with
   `-DGE_POINTER_BITS=64` stops at configure on `CMakeLists.txt:20`
   ("Windows x64 is outside the 64-bit plan").
2. **With the refusal commented out, only three files fail to compile.**
   Headless: 3 of 356 compile steps fail; video: 3 of 389. The same three in
   both, all in the port layer and none in `src/` or `port/fast3d/`:
   - `port/src/main.c`, the crash handler: x86-32 `CONTEXT` members (`Eip`,
     `Ebp`, `Esp`, `Eax` and the rest, `:150-431`) and an
     `int-to-pointer-cast` error at `:201` (the EBP walk through
     `unsigned long *`);
   - `port/src/port_profile.c`, the sampling profiler, compiled into every
     Windows build: `CONTEXT` members (`:136-145`) and three pointer and
     32-bit integer casts that the 64-bit `-Werror=` flags reject
     (`:142`, `:147`, `:246`);
   - `port/src/port_lp64.c:29-33`, the deliberate `#error` off Linux: the
     executable's range comes from `dl_iterate_phdr`.

   The 64-bit `-Werror=pointer-to-int-cast`, `int-to-pointer-cast` and
   `int-conversion` flags apply to every file. Under LLP64 they also reject a
   pointer cast to `long`, so no other file in either tree does that.
3. **The link needs the Windows flags split by width.** With the three files
   patched (register names swapped to `Rip`/`Rbp`/…, the two casts demoted
   to warnings, a PE image range from `GetModuleHandleW(NULL)` and
   `SizeOfImage`), the link fails on the `-m32` in `CMakeLists.txt:136`
   (`cannot open linker script file ldscripts/i386pe.x`). Without `-m32`,
   `--image-base=0x10000000` and `--disable-dynamicbase`, the video tree
   links: a 37.8 MB PE32+ x86-64 GUI executable, image base 0x140000000,
   `HIGH_ENTROPY_VA` and `DYNAMIC_BASE` set, importing only system DLLs,
   `SDL2.dll` and `WINHTTP.dll`. The 64-bit `SDL2.dll` comes from the
   `x86_64-w64-mingw32` folder of the SDL2 2.30.11 MinGW package the image
   already holds.
4. **The headless tree does not link on Windows.** It fails with undefined
   references to port functions (`portMouseBlocksAutoAim`,
   `portInputSlotDevice`, `portMusicVolumeRaw` and others) and to
   `clock_gettime` through `scripts/port/sdl_shim/SDL.h:33`. Headless is a
   Linux configuration today; whether the 32-bit Windows headless tree links
   was not measured.
5. **The manifest stops the exe before `main`.** The first launch exited with
   `0xC000007B` (STATUS_INVALID_IMAGE_FORMAT) and no output:
   `port/win/ProjectMidas.manifest:3,5` names `processorArchitecture="x86"`,
   so Windows loads the 32-bit Common Controls into the 64-bit process. With
   `amd64` there it starts.
6. **It boots and draws.** From an isolated folder (`build_win/win64-probe-run`,
   the ROM copied into its `Data`; deleted after the probe), `GE_FRAMES=600 SDL_AUDIODRIVER=dummy`:
   exit 0, `GL: renderer: NVIDIA GeForce RTX 3070 Laptop GPU/PCIe/SSE2`,
   `ge: video: window + GL context up`, the render thread on at 120 Hz,
   `ge: frame limit reached: frames=600 gfxtasks=257`, and the pools' peak
   lines with no exhaustion. Only the front end ran; no stage was loaded.
7. **Windows puts the heap and the stack above 4 GB, but one arena stays
   low.** The address line read `high arena 000001BB0FA00000, ROM image
   000001BB14610000, converter scratch 000001BB0E3680C0, executable image
   00007FF608920000, malloc block 000001BB0DF97510, stack 0000008F91DFF2EC`:
   every checked address is above 4 GB, under the default high-entropy ASLR.
   But the log also read `ge: memp arena: 64 MB at 0000000020000000`:
   `pcsched.c:459-470` tests `_WIN32` before `IS_64_BIT`, so the memp arena
   takes its 32-bit hint and is never checked. At 64 bits that arena's
   pointers read as N64 addresses under the host-pointer rule. The probe ran
   600 front-end frames on it without a fault, which proves nothing about a
   stage. `portAllocHighRaw` has the same order (`:56-57`): its Windows
   branch has no above-4 GB test, and here it passed only because Windows
   placed the mapping high.
8. **What the probe left as it was** (read in the code; not run):
   - crash reporting: Sentry's runtime is built `-A Win32` and staged as
     x86 only (`scripts/port/sentry_tools.py:63-67,82-91`;
     `windows-release.yml:181-194`), so a 64-bit exe logs "reporting
     unavailable" (`port/crash/reporting_win.c:63-70`). The probe's log
     read `crash reporting preference=ON active=0`;
   - the crash handler's module lookup parses PE32 only (`main.c:89-130`:
     magic `0x10B`, exports at `pe+0x78`), and its watchpoints and
     `GE_WATCH_ADDR` go through `DWORD` and `strtoul` (`:257-260`, `:497`);
   - the profiler's samples are 32-bit words (`port_profile.c:84-90`), and
     `scripts/port/bench/bench_profile.py:66-89,197-203` names them with
     `i686-w64-mingw32-nm` and no rebasing, which assumes a fixed base;
   - packaging and CI are x86 only: `package_windows.py:9-14` (one zip name,
     a DLL list, no PE machine check), `windows-release.yml:226-230,276,
     473,509,536,561`, `ci_dependencies.py:29-30,113,185-186`, the root
     `Dockerfile:22-23` (i686 MinGW only), `release_inputs.py:20-21`;
   - tooling that names `build_win`, the i686 compiler or the i686 SDL:
     `bench/stock.py:36,117-118`, `hand_gallery.py:440-461`,
     `audit_mission_cheats.ps1:32` (always copies `build_win/SDL2.dll`),
     `late_load_audit.py`, `soak.py`; `compare_win32.py:58-60` builds i686
     only, though it already parses PE32+;
   - LLP64 sites that compile but truncate: about 85 lines use `long`;
     about 11 hold addresses, all in Windows-only diagnostics (the crash
     handler, the profiler, `%08lx` names in `gfx_pc.cpp:2816,5049` and
     `gfx_opengl.cpp:1134`). The game headers already choose by
     `__SIZEOF_LONG__` or pointer size (`include/PR/gbi.h:1477-1491`, whose
     comment names 64-bit Windows; `gu.h:41-52`; `abi.h:223`).

## Approaches

### Toolchain

| Approach | Cost | Verdict |
|---|---|---|
| MinGW-w64 x86_64 GCC 13, cross-built in the container | One toolchain file and one package in the image; the same compiler family, flags and `-fms-extensions` dialect as both shipped builds. | **Recommended.** |
| MSVC or clang-cl | The game's C relies on GCC extensions, top-level assembly (`port/include/asmsym.h`) and GCC's warning names; a second compiler dialect for one platform. | Rejected. |
| MinGW-w64 on Windows itself (MSYS2) | A second build environment to keep in step with CI's. | Rejected; the container already builds the 32-bit exe on this laptop. |

### Who it is for

| Approach | Cost | Verdict |
|---|---|---|
| Self-build only | No CI job or package; players keep the 32-bit zip. | The early milestones. |
| A second download beside the 32-bit zip | One CI job, one package, a README section; two Windows zips to explain. | **Recommended.** |
| Replace the 32-bit zip | Players lose suspend points (FORK-014). | Rejected while 64-bit builds have no suspend points. |

### Addresses

| Approach | Cost | Verdict |
|---|---|---|
| Rely on high-entropy ASLR and check every region | The 64-bit branches already do this on Linux; Windows needs them ordered before `_WIN32`. The probe saw every checked address high. | **Recommended**, with the checks as the safety net. |
| Reserve everything below 4 GB at startup | Guarantees high placement for later allocations but not for the main thread's stack or the CRT heap, which exist before `main`. | Fallback, if a machine places a region low. |
| A fixed image base and fixed hints above 4 GB | Repeatable addresses; gives up ASLR. | Rejected unless a later suspend format needs it. |

## Design

### Build

A `cmake/mingw-x86_64.cmake` beside `mingw-i686.cmake`, with
`CMAKE_SYSTEM_PROCESSOR` `x86_64` so `GE_BUILD_ARCHITECTURE` names it
(`CMakeLists.txt:738-746`). The refusal at `:19-21` is lifted for `WIN32`
and stays for the web build. The `if(WIN32)` link options (`:131-139`) split
by width: the 32-bit line stays as it is; at 64 bits the same static
runtime, stack and libraries, without `-m32`, the fixed image base or
`--disable-dynamicbase`. The SDL root's default and help text
(`:618,653,765-769`) choose the folder by width. The manifest becomes
`processorArchitecture="*"`, which Windows resolves to the process's own
architecture, so one file serves both widths; `compare_win32.py` shows
whether that changes the 32-bit exe beyond its resources.

### Addresses

`port_lp64.c` gains a Windows branch that takes the executable's range from
its PE headers (`GetModuleHandleW(NULL)`, `SizeOfImage`). In `pcsched.c`,
the `IS_64_BIT` branches come before the `_WIN32` ones in
`portAllocHighRaw` and `portGetMempArena`, and the 64-bit Windows mapping
uses `VirtualAlloc` with the same "above 4 GB or stop" test the Linux
branch has. The 32-bit branches are untouched. The address-space line then
lists the memp arena too.

### Crash handler and profiler

The handler in `main.c` and the sampler in `port_profile.c` get x64 paths:
`CONTEXT`'s `Rip`/`Rsp`/`Rbp`, `StackWalk64` with
`IMAGE_FILE_MACHINE_AMD64` (or `RtlVirtualUnwind`, since x64 code has
unwind tables and need not keep frame pointers), a PE32+ module parser,
`uintptr_t` for every address, `strtoull` for `GE_WATCH_ADDR`, and
`DWORD64` debug registers. The 32-bit paths keep their code, under
`#if IS_64_BIT` / `#else`, so the 32-bit exe's functions are unchanged.
The profiler's sample words widen at 64 bits, and `bench_profile.py`
learns the PE image base from each sample file so it can rebase under
ASLR.

### Crash reporting

`sentry_tools.py` builds sentry-native for `-A x64` as well as `-A Win32`
and stages each by architecture; CI's `sentry-sdk` job caches both.
`reporting_win.c` tags the event's architecture, as `reporting_linux.c:164`
does. Symbols stay DWARF in the PE with a build id (`sentry.cmake:31`),
uploaded the same way.

### Gates under the Windows ABI

A 64-bit MinGW gate tree joins CI's tests step: `lp64_diagnostics.py gate`,
`narrowing` and `unprototyped` run on it, `lp64_static_checks.py` learns
MinGW's spelling of the pointer-sized types and treats `long` as 32-bit
there, and `test_abi_probe.sh` compiles the layout probe with the x86_64
MinGW compiler too, so every pinned record is checked under
`-mms-bitfields`. A record that differs there is a finding, not an
allowlist line.

### Package and release

`package_windows.py` takes the architecture: `Iron-Sights-windows-x86.zip`
as today, `Iron-Sights-windows-x64.zip` with the 64-bit `SDL2.dll`, Sentry
files and notices, and both check every PE file's machine type and refuse a
comparison build. A `windows-x64` job in `windows-release.yml` builds and
packages it; the release attaches and attests it beside the 32-bit zip, and
the README and release notes say which to choose (the x64 zip has no suspend
points). The `Dockerfile` and `ci_dependencies.py` add the x86_64 MinGW
packages; `release_inputs.py` records their versions.

### Tooling

`hand_gallery.py`, `bench/stock.py`, `late_load_audit.py`, `soak.py` and the
PowerShell audits take the executable's folder and copy the `SDL2.dll`
beside it; `audit_mission_cheats.ps1` stops hard-coding `build_win`. The
64-bit tree lives in `build_win64` beside `build_win`, so the main 32-bit
exe is never replaced by a 64-bit one. The suspend-point audits
(`audit_states.ps1`, `audit_state_effects.ps1`) stay 32-bit and refuse a
64-bit exe by name.

## Verification

Each item names what it proves. Game runs are on the laptop with the ROM;
their output is ROM-derived and stays out of Git.

1. **It compiles under the 64-bit rules and the Windows ABI.** The MinGW x64
   gate tree passes `lp64_diagnostics.py gate`, `narrowing --check` and
   `unprototyped --check`; the layout probe passes under MinGW x64.
2. **It boots.** The 64-bit exe runs 600 frames, exits 0, and logs
   `ge: lp64: address space OK` with the memp arena among the regions.
3. **It plays.** `audit_buffers.py --mission-smoke` passes 20 of 20 on the
   64-bit exe in a window, and the play sets
   (`demos,stages,stops,menus,tours,attract,sound`) pass, run natively on
   Windows if `lp64_play.py` can, else the builder records how.
4. **It is the same game.** A Windows x64 comparison build (`GE_COMPARE`)
   runs `lp64_equiv.py`'s 38 cases, and `compare --streams sim` against the
   x86_64 Linux comparison build passes or each difference is listed in
   `lp64_equiv_expected.txt` with its first tick and cause. MinGW's maths
   library is not glibc's: `scripts/port/lp64_libm_sweep.c` runs under it,
   and a function that differs joins the comparison build's in-tree set;
   shipped maths is untouched.
5. **It draws the same.** The picture digest of a Windows x64 video
   comparison build equals the x86_64 Linux one's, or each difference is
   explained; screenshots are reviewed by eye.
6. **Crashes are reported.** A deliberate crash in the packaged x64 exe
   reaches Sentry tagged `x86_64`, with function names, from both the
   handler and the out-of-process backend.
7. **The 32-bit exe did not change.** `compare_win32.py` from the commit
   before each milestone prints `identical in loaded code and data`, or
   every changed function is named; the 32-bit audits and CI pass.
8. **The other builds did not change.** CI passes, and the x86_64 Linux play
   sets and comparison pass again after each milestone that touches shared
   code.
9. **It costs what it should.** `bench.py run --smoke` with the 32-bit and
   64-bit exes on the same machine, and the late-load audit on the 64-bit
   exe for every stage; the figures go in the inventory.
10. **The package is the game.** The extracted x64 zip boots, runs the
    mission smoke, and its every PE file is x86-64.
11. **A person played it.** The owner plays a mission to its end with a
    gamepad and with mouse-look, in a window and fullscreen, with HDR on and
    off; the session's log goes through `lp64_play.py peaks --budget
    scripts/port/lp64_budgets.txt`.

### What no item above exercises

- A save, `midas.ini` and profiles shared by the 32-bit and 64-bit exes in
  one folder, and a player moving from one zip to the other.
- An overlay or capture tool that injects a DLL (Steam, Discord, OBS,
  RivaTuner), now loading its 64-bit copy.
- A user name or folder with non-ASCII characters in the data root's path.
- A machine with ASLR settings changed by policy (mandatory or bottom-up
  randomisation forced off), where regions may land low and the game must
  stop with a message rather than misread an address.

## Working machine

- **The owner's laptop** (Windows 11, RTX 3070 Laptop GPU) runs the 64-bit
  exe natively, regenerates the gallery with the 32-bit exe, and holds the
  ROM.
- **The `goldeneye` container** on the same laptop cross-builds both widths;
  its image gains the x86_64 MinGW packages.
- **The Linux containers** supply the x86_64 Linux comparison build the
  Windows one is compared with.
- **CI** has no ROM and no GPU; it builds, gates, packages, smoke-tests
  without a ROM, and publishes.

## Milestones

Each milestone gets its own detailed plan before it starts, accepted by the
owner, as the 64-bit plan's did.

1. **WIN64-001 Build and boot.** The toolchain file, the lifted refusal and
   `test_pointer_bits.sh`'s new expectation, the link options by width, the
   manifest, the image range, the allocation order, x64 paths in the crash
   handler and profiler sufficient to compile and to print a correct stack,
   the SDL root by width, the image's packages, and the MinGW gate tree in
   CI. Done when verification 1, 2 and 7 hold on `main`.
2. **WIN64-002 Plays and matches.** The play sets and mission smoke on
   Windows, the comparison build, the maths sweep, the digest. Done when
   verification 3, 4, 5, 8 and 9 hold.
3. **WIN64-003 Crash reporting and tooling.** Sentry x64, the profiler's
   wide samples and `bench_profile.py`'s rebasing, the tooling by folder.
   Done when verification 6 holds and every tool named under "Tooling" runs
   against `build_win64` or refuses it by name.
4. **WIN64-004 Package, release and hand-over.** `package_windows.py` by
   architecture, the CI job, the release, the README and release notes, the
   owner's play session. Done when one push to `main` publishes the attested
   x64 zip beside the x86 one, verification 10 and 11 hold, and the status
   table says Done.

Every milestone leaves the 32-bit, web and 64-bit Linux builds passing
their existing checks, updates `FORK_CHANGES.md` with each change, and
updates the status table before handing off.

## Owner's decisions

The owner took every recommendation on 2026-11-27. The plan above is
written to them; do not reopen them without the owner.

1. **Who it is for:** a second Windows download beside the 32-bit zip,
   which stays the default and the one with suspend points. Not taken:
   self-build only, or replacing the 32-bit zip.
2. **Suspend points:** the x64 zip ships without them, as the Linux x64
   packages do, and says so in the watch row and the README. Not taken: a
   relocating suspend format first, which would be a separate plan.
3. **Four milestones as above.** Not taken: stopping after WIN64-002 (a
   self-built, verified exe) and deciding on a release then.
4. **The 64-bit plan's Windows x64 non-goal is lifted.** That plan's other
   decisions stand; this one adds Windows x64 beside them. The refusal in
   `CMakeLists.txt:19-21` and case 5 of `test_pointer_bits.sh`, which
   asserts it, stay until WIN64-001 changes them together.
5. **Tree name:** `build_win64` beside `build_win`, so the main 32-bit exe
   stays where the gallery, the audits and the owner's shortcuts expect it.

## Left to the builder

- `StackWalk64` or `RtlVirtualUnwind` in the crash handler and profiler.
- Whether `lp64_play.py`, `lp64_equiv.py` and `audit_buffers.py` run
  natively on Windows as they are, or need a Windows runner path.
- Whether the 64-bit headless tree is made to link on Windows, or stays a
  Linux configuration (measurement 4).
- How CI smoke-tests the x64 zip without a ROM, beside the 32-bit zip's
  existing smoke test.
- The exact `long` sites to widen; the diagnostics under `-Werror` and the
  ABI probe find them, not reading.

## Milestone status

Update this table before handing off. Record evidence in the notes; do not
mark a milestone done without it.

| ID | Status | Notes |
|---|---|---|
| WIN64-001 | Done (2026-11-27) | [Its plan](windows_x64_win64_001.md); FORK-094 has the change and its evidence. On `main` as 4fde028; CI passed on it, including the MinGW gate tree (`build/lp64/gate-mingw: 392 files compiled, 0 failed`). The 64-bit exe boots 600 frames with every checked region above 4 GB, the memp arena included; `compare_win32.py` names only `chraiDefaultWeaponFireHandler` (the `chrprop.c` fix) and the manifest. |
| WIN64-002 | Done (2026-11-27) | [Its plan](windows_x64_win64_002.md), accepted 2026-11-27 under the owner's standing instruction. On the laptop: play sets 119 of 119 natively with the pools within budget, mission smoke 20 of 20, the 38 comparison cases 100 of 100 streams identical to the Linux x64 build, bench +6 to +14% average fps over the 32-bit exe, late loads as the Linux 64-bit work explained (FORK-094). On `main` as 1f3ea6f; CI passed on it. |
| WIN64-003 | Done (2026-11-27) | [Its plan](windows_x64_win64_003.md). The x64 Sentry runtime (built with the laptop's MSVC) passes the nine-case Windows smoke test on the 64-bit exe, events tagged `x86_64`; the profiler samples and unwinds at 64 bits and `bench_profile.py` rebases; every Tooling item runs on `build_win64` or refuses it by name (FORK-094). On `main` as 7ca2347; CI passed, and its `sentry-sdk` job built and uploaded `sentry-msvc-x64`. An event in the real Sentry project with uploaded symbols moves to WIN64-004's CI job. |
| WIN64-004 | Waiting on the owner | [Its plan](windows_x64_win64_004.md). Locally the extracted x64 zip boots, passes the mission smoke 20 of 20 and the Sentry smoke, and every PE file in it is x86-64 (verification 10). The push of 9352f21 published release `build-442.1-9352f2162b26` with the attested x64 zip (every CI job passed). A test crash from the released x64 exe reached Sentry (HTTP 200, event `cf3a5f62a4494567ce6497b036c02b94`, environment `windows-x64-check`). Sentry symbolicated it (issue IRON-SIGHTS-7, `testCrashFault (reporting_win.c:250)` down to `WinMainCRTStartup`). Left: the owner's play session (verification 11). |
