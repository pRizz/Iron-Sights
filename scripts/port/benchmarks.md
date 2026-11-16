# Benchmarks

The benchmark suite times the game on fixed work, several times over, and
compares builds: Iron Sights before and after a change, and stock Project
Midas 1.0.2b. It covers the Windows build and, with its own runner, the
[browser build](#the-browser-build). It needs the local US ROM
(`baserom.u.z64` in the repository root, or `GE_ROM`) and the project's build
container. Nothing ROM-derived is committed: routes are generated controller
input, and frame logs and result files stay under the ignored `build_win` and
`build`.

## Quick start

```sh
python scripts/port/bench/stock.py
```

```sh
python scripts/port/bench/bench.py run --build stock=build_win/bench-stock/build/ProjectMidas.exe --build fork=build_win/IronSights.exe --preset default --preset matched
```

The first command builds the stock comparison executable once (about five
minutes). The second prints a Markdown report and writes the full results to
`build_win/bench-results/<time>.json`. The first `--build` is the one the others
are compared against; `--base <label>` picks another.

Close other programs first and leave the machine alone while it runs. A game
window opens for each run and takes the foreground. Runs are silent.

To compare a change, build the commit from before it and run both together:

```sh
python scripts/port/bench/stock.py --ref HEAD~1 --name before
```

```sh
python scripts/port/bench/bench.py run --build before=build_win/bench-before/build/IronSights.exe --build after=build_win/IronSights.exe
```

Always measure the builds you compare in one session. Numbers from another
day, or from a busier machine, do not compare.

`bench.py report <results.json> --out <file.md>` renders a results file again.

## What a run measures

Each run plays one scenario in **fixed tick, unpaced** mode: every frame
advances the game by exactly one 60 Hz tick, and the game draws as fast as it
can, with no vsync and no frame cap. Game time follows the frames drawn, so a
run of 3,600 ticks draws the same 3,600 frames on any machine and in any build.
The measured window starts 300 ticks into the mission, after the load.

Per frame, the game logs the time from one present to the next and what it was
spent on (`GE_BENCH`, `port/src/port_bench.c`). The report gives, per build and
scenario, the median over the repeats of:

| Column | Meaning |
|---|---|
| Average fps | Frames in the window over the time they took |
| 1% low fps | Frame rate over the slowest 1% of frames |
| Median frame ms, 99th pct ms | Frame-time percentiles |
| Render ms | Mean time in the renderer per frame, without the buffer swap |
| Game ms | Mean of the longest stretch of game code per frame (tick plus display-list build); where the renderer shares the game's thread, its time is taken off |
| Draws | Mean draw batches per frame; a workload check, not a score |
| Load ms | The stage's load, taken as its longest frame before the window |

A change is printed in bold only when it is larger than the slowest-to-fastest
range of the runs it compares; otherwise treat it as noise. A single run has no
range, so nothing is bold. The results file
also holds frame counts over the 120, 60 and 30 fps budgets, the swap, audio and
pacer times, time in timed load events, and `work_ms`: render plus game, the
processor time a frame costs.

Fixed tick makes every frame do a whole 60 Hz game tick. In normal uncapped
play most frames are sub-ticks that skip the 60 Hz logic, so the in-game fps
counter reads higher than these numbers. The suite measures cost per frame of
full work; it is a comparison tool, not a prediction of the fps counter.

## Scenarios, routes and presets

`scripts/port/bench/scenarios.json` defines them.

- **Scenarios** are a mission, or a split-screen arena, with a route, a seed, a
  warm-up and a length in ticks. The missions were picked from a survey of all
  of them as the heaviest to draw (Cradle, Streets, Depot, Jungle) plus Dam.
  In the four-player scenario player 1 follows the route and the other three
  stand at their spawns, so all four views are drawn.
- **Routes** are generated, not recorded: a route description (walk forward,
  turn the camera by a fixed list of angles between straight legs, fire in
  bursts) becomes controller and mouse-look input in the game's own
  `GE_INPUTREC` format, replayed by game tick with `GE_INPUTPLAY`. The same
  description gives the same input for any build on any machine. Bond walks
  into walls for part of a route; it is a repeatable load, not a play-through.
  For a route you played yourself, record it with `GE_INPUTREC` (same
  `GE_STAGE`, `GE_SEED` and cheats, `maxfps=60`), put the file under
  `scripts/port/bench/` and name it in the scenario as `route_file` in place
  of `route`. Results from different routes do not compare.
- **Presets** are `midas.ini` lines added to the harness settings (window, no
  vsync, no frame cap). `default` adds nothing, so each build runs as it ships.
  `matched` sets the settings both builds share to stock's defaults and turns
  the fork's graphics features off, to compare the engines alone.

The game's own scripted drivers (`GE_DRIVE`, `GE_MPDRIVE`) are not used: they
count controller polls, which outrun game ticks in an unpaced run, so they give
different input every time.

## The stock build and other reference builds

`stock.py` exports a commit into `build_win/bench-<name>`, copies this
checkout's generated assets into it, and builds it in the project container
with the same compiler and flags. With no arguments the commit is the inherited
baseline named in `FORK_CHANGES.md` and the name is `stock`. It adds only what a
measurement needs, and only where the commit lacks it: the same `port_bench.c`,
the one call to it in `hitchlog.c`, and an early return that stops
`GE_FIXEDTICK` waiting on the clock. Stock's replay, seed, stage and fixed-tick
switches are its own. `--ref <commit> --name <name>` builds any other commit
the same way, which is how a "before" executable is made without keeping old
executables around. `--name <name> --clean` deletes a folder.

## The browser build

The browser runs the same scenarios, routes, presets and metrics. Three steps,
after building the runtime (`python scripts/port/web_build.py video`) and the
site (`bun run build` in `web/`) and serving it
(`node tools/serve-site.mjs 4191 /Iron-Sights/` in `web/`; from Git Bash set
`MSYS_NO_PATHCONV=1` so the prefix is not rewritten):

```sh
python scripts/port/bench/bench_web.py plan build/web-bench/current --preset default --preset matched
```

```sh
cd web && MIDAS_BROWSER=chrome MIDAS_ANGLE=d3d11 node tools/bench-audit.mjs ../baserom.u.z64 ../build/web-bench/current http://127.0.0.1:4191/Iron-Sights/
```

```sh
python scripts/port/bench/bench_web.py collect --build web=build/web-bench/current
```

`plan` writes the run list (environment, `midas.ini`, route and tick window
for each run). The Playwright tool plays it in one browser profile: a first
launch prepares the weapon icons, then each run reloads the page with its own
settings and saves the game's frame log beside the plan. `collect` turns one or
more such folders into the usual results file and report; give it a "before"
and an "after" folder to compare two runtimes.

Read browser results differently:

- **Compare Render ms and Game ms, not fps.** The browser schedules frames
  itself, and an unpaced run hands it the event loop twice a frame, each a wait
  of about 4 ms. Average fps and frame times mostly measure that. Render ms and
  Game ms are processor time inside the game and do not include the waits (the
  swap's wait is counted as swap, the pump's as pacer sleep).
- **The renderer shares the game's thread**, so a frame costs render plus game
  (`work_ms` in the results), and that sum against the display's frame time is
  the headroom: at 60 Hz a frame has 16.7 ms.
- **Timers are coarse.** Without cross-origin isolation Chrome rounds the
  game's clock to 0.1 ms, which is fine for the millisecond-scale browser
  numbers and too coarse for anything smaller.
- **Sound is not switched off** in the browser runs; a headless browser plays
  none. `MIDAS_HEADED=1` shows the window (and plays the sound).
- The runner has no guard against other programs. Keep the machine quiet.

## Smoke check for a render change

`AGENTS.md` asks for this before committing anything drawn or computed every
frame. It takes about ten minutes:

```sh
python scripts/port/bench/stock.py --ref HEAD --name before
```

```sh
python scripts/port/bench/bench.py run --smoke --build before=build_win/bench-before/build/IronSights.exe --build after=build_win/IronSights.exe
```

`--smoke` is three scenarios (Cradle, Dam, four-player Temple), both presets
and three repeats. `--ref HEAD` builds the last commit, which is "before" while
the change is still uncommitted. When the change sits behind a switch, one
executable can be measured against itself instead:
`--build off=build_win/IronSights.exe@GE_GLSHADOW=0 --build on=build_win/IronSights.exe`
gives a build its own environment after the `@`. Then profile one scenario of
each build as below and read the differences table, and record the before and
after figures in the change's inventory entry.

## Profiling

The benchmarks say how long a frame takes; the profilers say where the time
goes. Both use the same scenarios and tick window.

**Windows.** The game samples itself. `GE_PROFILE=<file>` starts a sampler
thread ([port_profile.c](../../port/src/port_profile.c)) that, a thousand times
a second (`GE_PROFILE_HZ`), suspends each thread that has used processor time,
reads its program counter and walks its frame-pointer chain. Take a profiled
run of each build and keep the folders:

```sh
python scripts/port/bench/bench.py run --build stock=build_win/bench-stock/build/ProjectMidas.exe --build fork=build_win/IronSights.exe --scenario cradle-fight --preset matched --repeats 1 --keep --env GE_PROFILE=profile.txt
```

```sh
python scripts/port/bench/bench_profile.py --run stock=build_win/bench-run-<time>/matched-cradle-fight-stock-1 --run fork=build_win/bench-run-<time>/matched-cradle-fight-fork-1 --exe stock=build_win/bench-stock/build/ProjectMidas.exe --exe fork=build_win/IronSights.exe
```

The report gives processor milliseconds per frame by function, self and
inclusive, for each busy thread, and for two runs the functions whose
inclusive time differs most (the first `--run` is the base). `--folded <prefix>`
also writes folded stacks for a flame graph tool. Function names come from the
project container's `nm`. Delete the kept run folders afterwards.

- Counts are processor time: a thread that has not run since the last sample
  is not sampled.
- The GL driver and system DLLs have no frame pointers, so a walk that starts
  inside one may stop there. Such samples are still charged to the module
  (`[nvoglv32.dll]`), and usually to the game function that called it.
- Time the game thread spends in the pump is set aside and reported as one
  figure. Unpaced, that thread spins there waiting for the renderer, polling
  input tens of thousands of times a second; it is not the cost of a frame.
- Sampling costs little: a profiled run's frame rates match an unprofiled one's.

**Browser.** `MIDAS_BENCH_PROFILE=1` makes the browser runner record a Chromium
CPU profile of each run, from the moment the player has control to the end, as
`<run>.cpuprofile` beside the frame log, with the number of frames it covers:

```sh
python scripts/port/bench/bench_profile_web.py --profile temple=build/web-bench/current/matched-temple-4p-1.cpuprofile --profile dam=build/web-bench/current/matched-dam-walk-1.cpuprofile
```

Functions of the game keep their C names; WebGL calls show as the browser's
own functions (`getParameter`, `bufferData`), which is exactly what shows a
costly call across the JavaScript boundary. The files also open in Chrome's
DevTools Performance panel.

## Method notes

- **Use a visible window.** A hidden window ran about 30% slower on the
  development machine and varied more. `--hidden` exists for smoke tests only.
- **Keep the machine to itself.** Before each run the runner waits (up to 15
  minutes) for other game processes to exit: another session's audit, a gallery
  capture or the player's own game takes the GPU. A run one of them overlapped
  is repeated, up to three times; if it still overlapped, it is kept and the
  report says how many such runs there were. The report also gives processor
  use across the whole machine during the runs, which shows up a build or
  another heavy job the process check cannot see.
- **Repeat and interleave.** Run-to-run variation of 5–15% is normal on a
  laptop. The runner alternates builds within each repeat and reports medians;
  the default is five repeats.
- **Check the workload.** Repeats of one build and scenario draw the same
  frames, so their mean draw counts agree; the report says so when they differ
  by more than half a percent, and part of their range is then a difference in
  work. Builds differ from each other in draws by design.
- **Sound is silent, not skipped.** Runs use SDL's dummy audio driver; the game
  still synthesises its audio, and the log reports that time.
- **Clean up.** Each run's executable copy is deleted when it exits and its
  folder when it has been measured. `--keep` keeps the folders
  (`build_win/bench-run-*`) for a closer look; delete them afterwards.

## Checks

`python scripts/port/test_bench_profile.py` tests the profilers' sample
parsing, naming and tables. `python scripts/port/test_bench.py` tests the metrics, run planning (Windows and
browser), route generation, host checks, report and stock patching without a
ROM.
