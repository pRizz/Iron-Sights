# WIN64-002: Plays and matches

The second milestone of the [Windows x64 plan](windows_x64_plan.md). The
64-bit Windows executable that [WIN64-001](windows_x64_win64_001.md) built
and booted now plays the game's run sets natively on Windows, matches the
x86_64 Linux comparison build stream for stream, and is measured against
the 32-bit exe.

**Status:** Done on 2026-11-27: verified on the owner's laptop (FORK-094
has the figures) and on `main` as 1f3ea6f, where CI passed. Planned on 2026-11-27 at
4fde028 (WIN64-001 on `main`) from the measurements below. Accepted by the
owner the same day under their standing instruction to take sensible
recommendations ("I accept your sensible recommendations"); the decisions
below are this plan's recommendations.

**Done when** verification 3, 4, 5, 8 and 9 of the Windows x64 plan hold:

3. `audit_buffers.py --mission-smoke --video` passes 20 of 20 on the
   64-bit exe, and the play sets `demos,stages,stops,menus,tours,attract,sound`
   pass.
4. A Windows x64 comparison build runs `lp64_equiv.py`'s 38 cases, and
   `compare --streams sim` against the x86_64 Linux comparison build passes
   or each difference is listed in `lp64_equiv_expected.txt`.
5. The same comparison passes for `gfx` (the picture digest), and the
   screenshots of both are reviewed by eye.
8. CI passes, and the x86_64 Linux checks pass again if shared code
   changed.
9. `bench.py run --smoke` with the 32-bit and 64-bit exes, and the
   late-load audit on the 64-bit exe, have their figures in the inventory.

## What was measured first

At 4fde028 on the owner's laptop (Windows 11, RTX 3070 Laptop GPU):

- **SDL's offscreen driver cannot make a GL context on Windows.** The
  64-bit exe with `SDL_VIDEODRIVER=offscreen` stops with `Could not
  initialize OpenGL / GLES library` (SDL's offscreen driver needs EGL,
  which Windows lacks). With the native `windows` driver and the rest of
  the play harness's video variables it runs 300 frames, draws 295
  pictures and exits 0.
- **The play harness runs natively once it can open a window.** With the
  driver set by host and `SDL2.dll` beside each case's copy, the `menus`
  set passes 7 of 7 from Windows' Python 3.12, about 894 pictures a case.
- **The comparison build's maths are the same under MinGW x64.**
  `lp64_libm_sweep.c` linked with the three comparison maths files prints
  the six pinned hashes (`sinf cc151db4`, `cosf 4c162bf1`, `tanf
  e3ccc704`, `atanf f160154d`, `expf 8be3a4cd`, `powf 648805d1`). Linked
  with MinGW's own C library, four differ from glibc's: `sinf 902caeb5`,
  `cosf e4754375`, `expf 30a1a4f5`, `powf 1c4c5c98` (`tanf` and `atanf`
  equal the in-tree ones). Shipped maths stays as it is (the plan's
  anti-goals); only comparison builds take the in-tree six.
- **A Windows x64 comparison video build configures and links**
  (`build/lp64/cmp-win64-video`: the x86_64 toolchain,
  `-DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DGE_COMPARE=ON
  -DGE_ROMDATA_FALLBACK=OFF` and the 64-bit SDL2 MinGW root).

## Decisions

Taken with the plan; each names the alternative not taken.

1. **The run tools run natively on Windows, with a real window.**
   `lp64_play_run.py` sets `SDL_VIDEODRIVER=windows` on Windows and
   `offscreen` elsewhere, and `run_copies.py`'s one copy of the executable
   carries `SDL2.dll` and links it beside each case's copy. The play sets,
   the mission smoke (`audit_buffers.py`) and the comparison
   (`lp64_equiv.py`) all go through those two files, so all three gain
   Windows at once. Cases open 640x480 windows on the desktop while they
   run, input off (`GE_NOINPUT=1`). Not taken: Wine or a Windows runner of
   its own.
2. **Windows runs are video runs.** The 64-bit headless tree does not link
   on Windows (the plan's measurement 4) and stays a Linux configuration;
   every Windows set runs with `--video`, which counts retraces at 60 a
   second as a headless run does. Not taken: making the headless tree
   link, which no player needs.
3. **The comparison pair is `x64:win64`.** `lp64_equiv.py run --video` on
   `build/lp64/cmp-win64-video` into `build/lp64/equiv/win64-video/win64`
   and on the Linux `build/lp64/cmp-x64-video` into
   `build/lp64/equiv/win64-video/x64`, both at the same commit with
   `--repeat 2`; `compare --streams sim,gfx,snd --expect` judges them, and
   any difference gets its own `x64:win64` line with its cause. Not
   taken: comparing with a headless Linux build, whose `sim` stream is not
   its video twin's.
4. **MinGW's maths are recorded, not adopted or replaced.** The four
   MinGW hashes go in FORK-094. A shipped 64-bit Windows build therefore
   computes `sinf`, `cosf`, `expf` and `powf` otherwise than a shipped
   Linux build, as the 32-bit Windows build always has; the comparison
   build removes the difference so the comparison can see everything else.

## Tasks

### Task 1: The run tools on Windows

- Modify: `scripts/port/lp64_play_run.py`, `scripts/port/run_copies.py`
  and their tests.

### Task 2: Plays

- Run the play sets and the mission smoke on `build_win64/IronSights.exe`
  natively, and `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt`
  over their folders.

### Task 3: Matches

- Build `build/lp64/cmp-win64-video` and the Linux `cmp-x64-video` at one
  commit, run the 38 cases on both with `--video --repeat 2`, compare
  `sim,gfx,snd`, and review a screenshot set of each (`--draw --shots`).

### Task 4: Costs

- `bench.py run --smoke` with `build_win/IronSights.exe` and
  `build_win64/IronSights.exe` together, and `late_load_audit.py` on the
  64-bit exe, if both tools can take the 64-bit exe's folder; otherwise
  the tooling gap moves to WIN64-003 (its "tooling by folder") and is
  recorded here.

### Task 5: The record

- Modify: `FORK_CHANGES.md` (FORK-094), the plan's status table, this
  file's status.

## What the runs found

Recorded in FORK-094 with the evidence; each within the milestone's done
condition:

- **The Windows CRT buffers a redirected stderr.** glibc never buffers
  stderr; the Windows runtime does when it is a file or a pipe, so a
  harness run's `ge:` lines came out late, after stdout's `load_bg_file`
  lines, and once split mid-line by the frame-limit line. The play logs
  passed, but `lp64_play.py peaks --budget` put 27 of 341 pool and stage
  pairs in the wrong level. `portDirectLaunchLog` (`port/src/main.c`) now
  makes a redirected stderr unbuffered on Windows, as its direct-launch
  log already was; the same sets then gave `OK: 320 pool and stage pairs
  within budget`. (A first guess, flushing stdout before the frame-limit
  exit, fixed only the mid-line split and was taken out.)
- **The comparison streams were written in text mode.** The simulation
  trace and the picture digest opened their files with `"w"`, so a Windows
  build wrote CRLF lines and the stream readers refused them. Both open
  `"wb"` now.
- **The rest needed nothing.** The comparison build's maths, the stream
  hashes, the pools and every play case behave on Windows as on Linux.
