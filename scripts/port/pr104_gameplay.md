# PR #104 gameplay capture

Use `audit_upstream_pr104.ps1` to exercise the exact upstream PR branch in a
separate Project Midas runtime. It checks for clean commit `05a8d83`, builds a
separate Windows binary directory in the sibling `Project-Midas-ambient` worktree,
mounting this checkout's generated assets read-only for the build,
then records one silent H.264 window video and process telemetry CSV per stage.
The runner uses your local US ROM through `GE_ROM`; it neither copies the ROM
nor launches the Iron Sights executable. `GE_NOINISAVE=1` and an isolated
runtime keep normal settings, saves, profiles and logs untouched. The runtime
pins a windowed 960×540 display, 60 FPS and no vsync. `-Seconds` is a minimum
recording target. Automatic mode uses a generous 120-pump-frame-per-second cap;
manual mode requests a clean window close after the elapsed recording time.
The automatic run can last longer at lower pump rates.
The isolated INI sets `look_updown=1` (Upright) for conventional mouse look;
it does not change an existing game profile or the upstream PR.
Captures stay ignored under `build_win/pr104-gameplay-runs/` and must not be
committed.

Requirements: Docker with the `goldeneye` image, the clean PR #104 worktree,
your local ROM, PowerShell 7, and FFmpeg/FFprobe with `gdigrab` and `libx264`.
The window must be visible on a Windows desktop for capture.

From this repository root, record hands-off Dam and Facility runs:

```powershell
./scripts/port/audit_upstream_pr104.ps1 -Mode Auto -Stages dam,facility -Seconds 90
```

Automatic mode filters real input and uses the inherited `GE_DRIVE=1` walk and
turn pattern. It records a smoke run, not a human visual sign-off.
Manual mode removes the automatic input environment variables entirely; an
empty `GE_NOINPUT` value would still disable controls in the game.
It also leaves `GE_SKIPINTRO` unset so synthetic intro-skip taps cannot cycle
weapons after control begins. Let the opening camera finish or use normal
controls to advance it.

For the contributor's hands-on rendering check, replay the same stages with
normal controls. The runner requests a clean exit after the requested time,
then opens the next stage. Move and turn, aim, and inspect lit guards, weapons
and props for lighting changes, missing geometry or crashes. Use `-Stages dam`
for one stage or `-SkipBuild` to reuse
the isolated build after its first successful build. `-SkipBuild` requires the
runner's matching commit stamp, so an older executable cannot be reused by
accident.

```powershell
./scripts/port/audit_upstream_pr104.ps1 -Mode Manual -Stages dam,facility -Seconds 180 -SkipBuild
```

Each stage directory contains `gameplay.mp4`, `process.csv`, `stdout.log`,
`stderr.log`, FFmpeg's report, any game logs under `Data/Logs`, and
`session.json`. The JSON records the branch commit, stage, mode, exit codes,
stage-launch and frame-limit markers, wall-clock close request, crash-log
presence, and FFprobe video metadata. The CSV samples process CPU time,
one-core CPU percentage, working and private memory, handles, and threads once
per second. These are host-process
measurements; video frame count is capture output, not the game's render FPS.
FFmpeg may report a final window read error as the game exits; a usable run
still requires exit zero and a probeable video. The runner leaves visual review
marked pending in its JSON.

A completed scripted run needs a stage-launch marker, frame-limit marker,
zero game/FFmpeg exit codes, at least one process sample, a probeable video
covering at least the requested time, and no crash log. Manual mode
uses a wall-clock close request instead of the frame cap. Review the
video and game log before describing visual results. The hands-on checkbox in
the upstream PR remains unchecked until the contributor actually plays the
exact branch and records the stages, duration and observations. The separate
32-bit AddressSanitizer reproducer already covers the over-read itself.
