# WIN64-003: Crash reporting and tooling

The third milestone of the [Windows x64 plan](windows_x64_plan.md). The
64-bit Windows exe gets Sentry crash reporting with its own x64 runtime, a
working sampling profiler whose samples `bench_profile.py` can name under
ASLR, and every tool the plan names under "Tooling" either runs against
`build_win64` or refuses it by name.

**Status:** Done on 2026-11-27: checked on the owner's laptop (FORK-094 has
the evidence) and on `main` as 7ca2347, where CI passed and built the x64
Sentry runtime. The real-Sentry part of verification 6 moves to WIN64-004. Planned on 2026-11-27 at
544cc42 from a reading of the files named below. Accepted by the owner the
same day: they asked to start it ("start win64-003") under their standing
instruction to take sensible recommendations; the decisions below are this
plan's recommendations.

**Done when** verification 6 of the Windows x64 plan holds, as far as this
milestone can take it, and every tool under "Tooling" runs against
`build_win64` or refuses it by name:

6. A deliberate crash in the 64-bit exe, with the x64 Sentry runtime beside
   it, reaches a Sentry receiver tagged `x86_64` with its image's
   architecture `x86_64`, from both the in-process handler and the
   out-of-process backend. This milestone proves it against the smoke
   test's local receiver (`test_sentry_windows.py --build`); the packaged
   zip, symbol upload and an event in the real Sentry project, which needs
   CI's upload token and the owner's eyes, come with WIN64-004's CI job.

## What was read

At 544cc42:

- `port/crash/reporting_win.c` loads `sentry.dll` from the exe's folder by
  name and resolves its functions; nothing in it depends on the width. The
  64-bit exe logs `reporting unavailable` only because the runtime CI
  builds and stages is x86 (`sentry_tools.py build` uses `-A Win32`,
  `stage` refuses any other architecture). It sets no `architecture` tag;
  `reporting_linux.c` sets one from `portBuildArchitecture()`.
- `test_sentry_windows.py` asserts the crashing image's `arch` is `x86`.
- `port_profile.c` walks frame pointers, which MinGW x64 does not keep
  usable (WIN64-001's finding), and stores 32-bit words; at 64 bits it
  compiles a stand-in. `bench_profile.py` reads symbols with
  `i686-w64-mingw32-nm` and matches raw addresses, which works only for the
  32-bit exe's fixed base; its pump filter names `_portPumpEvents`, the
  32-bit spelling.
- Tooling: `late_load_audit.py`, `soak.py` and `bench.py` already copy the
  exe's own DLLs (WIN64-002 ran the first and last on `build_win64`).
  `audit_mission_cheats.ps1` always copies `build_win/SDL2.dll`.
  `audit_states.ps1` and `audit_state_effects.ps1` test suspend points,
  which 64-bit builds do not have. `hand_gallery.py generate --executable`
  takes any exe, but the gallery's capture build is the 32-bit exe.
  `bench/stock.py` builds a ref with the i686 toolchain only.

## Decisions

Taken with the plan; each names the alternative not taken.

1. **One Sentry runtime per architecture.** `sentry_tools.py build` and
   `stage` take `--arch x86` (the default, so CI's existing job is
   unchanged) or `--arch x64` (`-A x64`); the runtime's
   `sentry-build.json` records it and `stage` refuses a mismatch. CI's
   `sentry-sdk` job builds and caches both, the x64 one as its own artifact
   for WIN64-004's job. Not taken: one universal folder, which would let a
   package carry the wrong DLL.
2. **Events carry the architecture.** `reporting_win.c` sets the
   `architecture` tag from `portBuildArchitecture()`, as Linux does, at
   both widths (the 32-bit exe's events gain `i686`).
3. **The smoke test takes the exe's width.** `test_sentry_windows.py`
   expects the image `arch` and `architecture` tag that the exe's PE machine
   implies (x86 and `i686`, or x86_64 and `x86_64`).
4. **The 64-bit sampler unwinds without the loader's locks.** At 64 bits
   `port_profile.c` samples a suspended thread's `Rip`, then unwinds with
   `RtlVirtualUnwind`, finding each function's unwind entry by a binary
   search of the module's own exception directory, read from the module
   list the sampler refreshes while no thread is suspended.
   `RtlLookupFunctionEntry` is not used there: it takes the loader's
   function-table lock, which a suspended thread loading a DLL may hold,
   and the sampler starts while DLLs load. Sample words are pointer-sized
   at 64 bits and stay 32-bit words at 32 bits, so the 32-bit sampler's
   code does not change. The file format stays `# ge-profile 1`, with
   wider hex.
5. **`bench_profile.py` rebases.** It reads the exe's preferred image base
   from its PE header and moves each sample address from the module's load
   address (its `M` line) to that base before naming it; it picks
   `x86_64-w64-mingw32-nm` for a PE32+ exe, and its pump filter accepts
   both spellings. A 32-bit exe loads at its preferred base, so its
   numbers are unchanged.
6. **Tooling.** `audit_mission_cheats.ps1` copies `SDL2.dll` from the
   exe's folder. `audit_states.ps1` and `audit_state_effects.ps1` refuse a
   64-bit exe, naming why (no suspend points at 64 bits).
   `hand_gallery.py generate --executable` refuses a 64-bit exe, naming the
   32-bit capture build, through the new `pe_machine.py` (the gallery tool
   is at its recorded length baseline). `bench/stock.py --ref` takes `--arch x64` to
   build a ref with the x86_64 toolchain and SDL, and refuses it for the
   stock 1.0.2b build, which has no 64-bit source.

## Tasks

### Task 1: Sentry x64

- Modify: `scripts/port/sentry_tools.py` and its test,
  `port/crash/reporting_win.c`, `scripts/port/test_sentry_windows.py`,
  `.github/workflows/windows-release.yml` (the `sentry-sdk` job).
- Build the x64 runtime with the laptop's MSVC 2022, configure
  `build_win64` with `MIDAS_SENTRY_INCLUDE_DIR`, stage the runtime beside
  the exe, run `test_sentry_windows.py --build build_win64`.

### Task 2: Profiler

- Modify: `port/src/port_profile.c`, `scripts/port/bench/bench_profile.py`
  and its test.
- One profiled `bench.py` run of each exe and `bench_profile.py` over them.

### Task 3: Tooling

- Modify: the scripts of decision 6 and the guides that name them.

### Task 4: The record

- `compare_win32.py` for the 32-bit exe; `FORK_CHANGES.md` (FORK-094), the
  plan's status table, this file's status.
