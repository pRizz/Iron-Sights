# WIN64-004: Package, release and hand-over

The last milestone of the [Windows x64 plan](windows_x64_plan.md). One push to
`main` builds, symbol-uploads, packages, smoke-tests and publishes an attested
`Iron-Sights-windows-x64.zip` beside the x86 zip, and the owner plays it.

**Status:** Published on 2026-11-27: release `build-442.1-9352f2162b26`
carries the attested x64 zip, and verification 10 holds (FORK-094 has the
evidence). Left: the owner's play session and the Sentry check. Planned on 2026-11-27 at ad82813
from a reading of `package_windows.py`, `release_inputs.py`,
`ci_dependencies.py`, `.github/workflows/windows-release.yml` and the release
texts. Accepted by the owner the same day ("Start WIN64-004", after being told
its push publishes the zip) under their standing instruction to take sensible
recommendations; the decisions below are this plan's recommendations.

**Done when** one push to `main` publishes the attested x64 zip beside the x86
one, verification 10 and 11 of the plan hold, and the status table says Done:

10. The extracted x64 zip boots, runs the mission smoke, and its every PE
    file is x86-64.
11. The owner plays a mission to its end with a gamepad and with mouse-look,
    in a window and fullscreen, with HDR on and off, and the session's log
    goes through `lp64_play.py peaks --budget scripts/port/lp64_budgets.txt`.

WIN64-003 left the real-Sentry half of verification 6 here: the CI job uploads
the x64 exe's debug files, and the owner checks that a crash from the
packaged x64 exe reaches the Sentry project, tagged `x86_64` and symbolicated.

## Decisions

Taken with the plan; each names the alternative not taken.

1. **One packager, a table of two targets.** `package_windows.py --arch
   x86|x64` (x86 the default, so the existing job is unchanged), as
   `package_linux.py` does: each target names its zip, its outer checksum
   and build-info files (`SHA256SUMS.txt` and `build-info.json` for x86, as
   today; `SHA256SUMS-windows-x64.txt` and `windows-x64-build-info.json` for
   x64, so both fit in one release) and its PE machine. Every `.exe` and
   `.dll` it packs must carry that machine, and `sentry-build.json` that
   architecture; it refuses otherwise, as it refuses a comparison build.
   Not taken: a second packager.
2. **A `windows-x64` CI job beside `windows`.** The same steps with the
   x86_64 toolchain, `-DGE_POINTER_BITS=64`, the `sentry-msvc-x64` artifact,
   debug-file upload, `stage --arch x64`, `package_windows.py --arch x64`
   and `release_inputs.py --arch x64` (compiler and build folder by
   architecture). `ci_dependencies.py` gains a `windows-x64` profile (the
   x86_64 MinGW packages and ccache, their win32 alternatives, a compiler
   key from the x86_64 compiler). Not taken: one job building both widths,
   which would double its time on the critical path.
3. **The x64 zip is smoke-tested like the x86 one.** `sentry-smoke` runs
   `test_sentry_windows.py` on each zip (the x64 one in its own job on the
   Windows runner), ROM-free, and `publish` needs both.
4. **The release attaches and attests it.** `publish` downloads the x64
   artifact, checks its checksum file and that its build info names the
   same commit, attests the zip, and attaches the zip and its three
   metadata files. The notes say which Windows download to choose: x86 is
   the default and keeps suspend points; x64 is a native 64-bit build
   without them.
5. **The texts.** `START-HERE.txt`, shared by both zips, says which build
   the player has and what x64 lacks; the README's download and build
   sections name the x64 zip. The watch row already says "Suspend points are
   unavailable in 64-bit builds" (`port_state_unavailable.c`).

## Tasks

1. `package_windows.py` and `release_inputs.py` by architecture, with tests.
2. `ci_dependencies.py`'s `windows-x64` profile, with tests.
3. The workflow: `windows-x64`, `sentry-smoke-x64`, `publish`.
4. `START-HERE.txt`, README, release notes.
5. Locally: package `build_win64`, run the smoke test and the mission smoke
   on the extracted zip, check every PE file's machine.
6. Push; confirm the release carries the attested x64 zip.
7. The owner's play session and Sentry check; the record.
