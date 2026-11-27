# WIN64-001: Build and boot

The first milestone of the [Windows x64 plan](windows_x64_plan.md). It turns
the probe that plan measured into the tree: a MinGW-w64 x86_64 toolchain
builds a 64-bit Windows executable from `main`, the executable boots with
every game address checked above 4 GB, the crash handler prints a correct
x64 stack, the 64-bit compile gates run on a MinGW tree as well as on Linux,
and the 32-bit executable stays byte-for-byte what it was.

**Status:** Done on 2026-11-27: on `main` as 4fde028, where CI passed,
including the MinGW gate tree (392 files compiled, 0 failed). Planned on
2026-11-27 from the measurements recorded in the
Windows x64 plan (`main` at 990f914) and a reading of the files named below
at eb01b50. Accepted by the owner the same day under their standing
instruction to take sensible recommendations ("I accept your sensible
recommendations"); the decisions below are this plan's recommendations.

**Done when** verification 1, 2 and 7 of the Windows x64 plan hold on
`main`:

1. The MinGW x64 gate tree passes `lp64_diagnostics.py gate`, `narrowing
   --check` and `unprototyped --check`, and the layout probe compiles under
   MinGW x64.
2. The 64-bit exe runs 600 frames, exits 0, and logs `ge: lp64: address
   space OK` with the memp arena among the regions.
7. `compare_win32.py commits` from the commit before the milestone prints
   `identical in loaded code and data`, or names every changed function;
   the 32-bit audits and CI pass.

## Decisions

Taken with the plan; each names the alternative not taken.

1. **Toolchain and width must agree.** `GE_POINTER_BITS=64` on Windows needs
   `cmake/mingw-x86_64.cmake` and `GE_POINTER_BITS=32` needs
   `mingw-i686.cmake`; CMake refuses a mismatch by name, from
   `CMAKE_SIZEOF_VOID_P`. Not taken: inferring the width from the
   toolchain, which would make a stale cache silently change width.
2. **One manifest, `processorArchitecture="*"`.** Windows resolves `*` to
   the process's own architecture for both the identity and the Common
   Controls dependency. Not taken: a second manifest and `.rc` file.
   `compare_win32.py` shows the 32-bit exe's resource bytes change and
   nothing else.
3. **Register access by width in one header.** `port/include/port_win_context.h`
   names the program counter, stack and frame registers, the stack walk's
   machine, the address type and its print format. `main.c` reads them
   through it, so both widths share one crash handler, first-chance
   reporter, stall watchdog and watchpoints. At 32 bits the macros expand
   to the code that is there today. Not taken: a second, x64-only copy of
   the handler, which would fork the watchdog's modal-loop logic.
4. **The profiler stays 32-bit in this milestone.** At 64 bits
   `port_profile.c` compiles a stand-in that logs once that `GE_PROFILE` is
   not available yet and names WIN64-003, which widens the samples and
   teaches `bench_profile.py` to rebase them. It tests `_WIN64`, not
   `IS_64_BIT`, because `bench/stock.py` compiles this file into stock
   trees that lack `platform_info.h`.
5. **The image range from the PE headers.** `port_lp64.c` takes the
   executable's range from `GetModuleHandleW(NULL)` and the optional
   header's `SizeOfImage` on Windows, beside the Linux `dl_iterate_phdr`
   branch; other platforms keep the `#error`.
6. **64-bit Windows allocations go through the 64-bit rule.**
   `portAllocHighRaw` gains a 64-bit Windows branch (`VirtualAlloc` with no
   hint, released and refused when below 4 GB), and `portGetMempArena`
   takes its fixed 0x20000000 hint only at 32 bits, so a 64-bit Windows
   memp arena goes through `portAllocHighOrExit` like Linux's and is
   checked. The 32-bit branches are untouched.
7. **The gate tree is a MinGW video tree.** `build/lp64/gate-mingw`,
   configured with the x86_64 toolchain, `GE_POINTER_BITS=64` and
   `GE_VIDEO=ON`, builds only `ge_buildinfo`; the three
   `lp64_diagnostics.py` checks run on it in CI's tests step beside the
   Linux trees. The headless tree does not link on Windows and is not a
   gate (the plan's measurement 4 and "Left to the builder"). The tests
   job gains the x86_64 MinGW packages and the SDL2 MinGW headers it needs.
8. **The layout probe compiles under MinGW x64.** `test_abi_probe.sh` adds
   a MinGW half when `x86_64-w64-mingw32-gcc` is installed: it compiles and
   links the probe with the game's flags, so every `_Static_assert` holds
   under the Windows ABI and `-mms-bitfields`; the probe's three run-time
   checks need Windows to run and are named as not run. Not taken: Wine in
   CI.
9. **The `goldeneye` image gains the x86_64 MinGW packages** in a layer of
   its own, so the i686 layers above it are reused.

## Tasks

### Task 1: Toolchain, CMake and manifest

- Create: `cmake/mingw-x86_64.cmake`.
- Modify: `CMakeLists.txt` (the refusal at `:19-21` becomes the width
  check of decision 1; the `if(WIN32)` link options split by width; the
  SDL2 MinGW default folder chosen by width at its three uses),
  `port/win/ProjectMidas.manifest`, `scripts/port/test_pointer_bits.sh`
  (case 5: a mismatch is refused both ways; 64 with the x86_64 toolchain
  configures), `Dockerfile`, `README.md` (the Windows build section names
  the 64-bit configuration and `build_win64`).

### Task 2: Addresses

- Modify: `port/src/port_lp64.c`, `port/src/pcsched.c`.

### Task 3: Crash handler and profiler

- Create: `port/include/port_win_context.h`.
- Modify: `port/src/main.c`, `port/src/port_profile.c`.

### Task 4: Gates under the Windows ABI

- Modify: `scripts/port/test_abi_probe.sh`,
  `scripts/port/ci_dependencies.py` and its test (the tests profile adds
  `gcc-mingw-w64-x86-64-win32`, `g++-mingw-w64-x86-64-win32` and
  `binutils-mingw-w64-x86-64`), `.github/workflows/windows-release.yml`
  (the SDL2 package and the gate tree in the tests job),
  `scripts/port/lp64_static_checks.py` only if the MinGW tree's compiler
  spells a checked type otherwise.

### Task 5: Verification and the record

- Build the 32-bit exe from the commit before the milestone and after it
  with `compare_win32.py commits`; build the 64-bit exe into `build_win64`
  and boot it 600 frames from an isolated runtime folder; run the gates in
  the container; regenerate the gallery if a rendering input changed.
- Modify: `FORK_CHANGES.md` (FORK-094), the Windows x64 plan's status
  table, this file's status.

## What the build found

Recorded in FORK-094 with the evidence; the decisions above were extended
by these, each within the milestone's done condition:

- MinGW-w64's x64 start-up wraps `main` in an SEH scope whose filter calls
  an installed `SIGSEGV`, `SIGILL` or `SIGFPE` handler before Windows
  reaches the unhandled-exception filter, so at 64 bits `main.c` installs
  none of the three; with them, the crash test exited 139 with no dump.
- MinGW x64's RBP does not point at the saved RBP, so the frame walks use
  the unwind tables at 64 bits (decision 3 named `StackWalk64` and the
  frame-pointer walk).
- CMake's Windows-GNU module puts include directories in response files,
  which the gate refuses; `CMakeLists.txt` turns that off on Windows after
  the last `enable_language`.
- The overlay review treated `_WIN32` as undefined at 64 bits; it no
  longer does, and the 15 sites it now sees have verdicts.
- The probe's MinGW half found `struct image_entry` and `Gsetcolor` 12
  bytes under Microsoft's bitfield rules (at both Windows widths), and
  with it a byte-stride read of `g_Textures` in `chrprop.c` that on
  Windows read the wrong texture; it reads the field now.

## Left to the builder

- The stack walk: at 64 bits the handler's own frame walks use the unwind
  tables (`RtlLookupFunctionEntry`, `RtlVirtualUnwind`), since the
  frame-pointer walk printed garbage under MinGW x64 ("What the build
  found"); the SEH dump's `StackWalk64` with `IMAGE_FILE_MACHINE_AMD64`
  gives the same frames.
- How the MinGW gate tree reaches the SDL2 headers in CI: the tests job
  restores the Windows job's cached SDL2 MinGW package, or downloads it.
