# Browser portability spike

The first WEB-001 implementation slice provided an executable Wasm ABI/address
probe and a compile-only survey of the existing headless game. The historical
measurements below explain the initial failures. The root Wasm core now links
and passes a private-ROM Dam smoke; see the [runtime runbook](web_runtime.md) for
the current build. Follow the [accepted plan](web_target_plan.md) and
[milestone tracker](web_target_progress.md) for acceptance status.

## Run the checks

Requirements are Python 3.10+, Git and Docker with Linux containers. The SDK pin
is [web_toolchain.json](web_toolchain.json): Emscripten 6.0.13 with the immutable
official image digest. The runner pulls that exact image when absent, then runs
with networking disabled, the source mount read-only and a writable output mount
at `build/web-spike`. No ROM is read, mounted into a virtual filesystem or
packaged. The source mount may contain an ignored local ROM; the tooling only
selects source files through CMake and does not access that ROM.

```sh
python scripts/port/test_web_spike.py
python scripts/port/web_spike.py probe
python scripts/port/web_spike.py audit --jobs 4
```

Use `all` to run the probe and audit together. Exit status 0 means the selected
checks completed without failures, 1 means the source survey found compile
failures, and 2 means setup or a probe failed. A source survey failure must not
be represented as a passing game build. Later root-platform fixes can change
these survey results; compare current logs rather than assuming 11 failures.

Commands can be rerun. Logs and summaries are overwritten for the selected mode;
other modes may retain earlier results. Use the matching `summary-probe.json`,
`summary-audit.json` or `summary-all.json`. The `audit.json` source records link
to individual logs under `audit/`. Objects, JavaScript, Wasm and logs remain in
the ignored build directory. They are diagnostic artifacts, not a deployment.

The probe has its own CMake project under `scripts/port/web_probe`; no native
target or render inputs are changed by running it. The browser game now uses
the existing root CMake project and `ge` target through its Emscripten branch.

## ABI and address experiment

The probe includes actual engine headers and checks pointer/long widths,
display-list/audio command strides, vertex/matrix/viewport sizes and ROM table
layouts at compile time. Runtime checks cover little-endian words, unsigned
characters, one single-precision rounding boundary, pointer-word round trips and
a correctly typed function-pointer round trip. Checks terminate on failure even
in Release builds; the repository's logging-only `assert.h` is not used.

Two builds allocate synthetic 64 MiB game and conversion arenas plus a 12 MiB
ROM stand-in. The compact build uses the normal low data base. The high build
uses `GLOBAL_BASE=268435456`, preserving the native classifier's numeric
separation at the cost of a large unused prefix. Neither design is selected for
production by this experiment.

The 2026-10-24 Node/Wasm run with Emscripten 6.0.13 produced:

| Measurement                                           | Compact   | High data base |
| ----------------------------------------------------- | --------- | -------------- |
| Initial linear memory bytes                           | 33554432  | 301989888      |
| Final linear memory bytes                             | 181272576 | 434896896      |
| Requested allocation bytes                            | 146800640 | 146800640      |
| Static address                                        | 4608      | 268439040      |
| Game arena address                                    | 16783480  | 285217912      |
| Conversion arena address                              | 83892352  | 352326784      |
| ROM stand-in address                                  | 151001224 | 419435656      |
| Existing host classifier accepts tested host pointers | No        | Yes            |
| Segment word `0x03000120` lies inside the game arena  | Yes       | No             |
| Tested ABI contracts                                  | Passed    | Passed         |

These are Wasm linear-memory capacities for synthetic allocations, not physical
resident memory, a browser benchmark or peak game memory. The compact layout
demonstrates an ambiguity: the same numeric word can name valid host memory and
an installed N64 segment reference. Registered ranges alone cannot resolve that
overlap. The high layout is a correctness baseline for the tested values, not
proof that every legacy pointer/address operation is valid under Wasm.

## Compile survey

The runner configures the real root CMake project with `GE_HEADLESS=ON`,
`GE_VIDEO=OFF`, `GE_ROMDATA_FALLBACK=OFF` and compile-command export enabled. It
generates the build-info header and records the ordinary root build result
first, with fast final linking for iteration. The initial failure, before the
Emscripten branch existed, was the inherited x86 SSE compiler configuration.

It then compiles each selected translation unit to a Wasm object with that
source's defines and includes, removing only `-m32`, `-msse2`, `-mfpmath=sse`,
`-fno-toplevel-reorder`, `-malign-data=abi` and assembler include-path flags.
The survey demotes three inherited C diagnostic classes from errors to warnings:
implicit function declarations, integer conversions and incompatible pointer
types. These survey flags are recorded in the output. The current root target
separately retains legacy C allowances described in the runtime runbook, with
linker signature warnings fatal. Existing `-w` remains in the selected flags.
Successful compilation alone does not establish type or runtime correctness.

The initial survey compiled 246 of 257 translation units and failed 11:

- Missing declarations followed by conflicting definitions: `src/game/bg.c`,
  `src/game/bondview2.c`, `src/game/options.c`, `port/src/port_profiles.c`.
- Anonymous-field name ambiguity under Clang: `src/game/lv.c` and
  `src/game/spectrum.c`.
- Native assembler section/symbol syntax: `port/src/port_weapon_images.c`,
  `port/src/ramrom_data.c`, `port/src/segments.c`, `port/src/ucode_dl_stubs.c`.
- Native crash-handler dependency on `execinfo.h`: `port/src/main.c`.

This excludes the SDL renderer/audio/input source set and does not attempt the
native executable overlay step, a final link, ROM loading or gameplay. The
compile database derives from CMake rather than a separately maintained game
source list. Read the current logs when fixing a failure; this baseline is not
an exception list that makes failures acceptable in a production build.

## Superseded initial implementation step

Introduce the root CMake Emscripten platform branch with explicit browser
startup/segment adapters and suspend-feature gating, then address the reported
Clang declaration/anonymous-field errors. Preserve native behavior and run the
applicable native tests and gallery workflow for shared inputs. Link the full
headless core, measure its real static/arena requirements and choose an address
strategy before advancing to WEB-002 rendering and browser scheduling. This
step is now implemented; continue from the milestone tracker's current gate.
