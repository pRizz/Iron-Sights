# Windows crash reporting

Windows player releases report crashes to the `iron-sights` project in the
Bright Builds Sentry organization. Reporting defaults to **ON**. Disable it in
**Port Options > Crash Reporting**, or edit the executable's `midas.ini` before
starting:

```ini
[diagnostics]
crash_reporting=0
```

The preference is read before the reporter initializes, independently of video
initialization. Switching OFF stops reporting and removes its pending queue.
`GE_CRASH_REPORTING=0` suppresses reporting for one process without changing the
saved preference. Audit runs with `GE_NOINISAVE`, `GE_NOINPUT` or `GE_HAND_GALLERY` are suppressed.
If the INI cannot be written, switching OFF still stops this process's uploads;
the console explains that the preference could not be saved for future runs.
Local `Data/Logs/crash.log` diagnostics remain available with reporting OFF.

## Data sent

Reports contain the exception code, native stack addresses, registers, loaded
module names and paths, operating system/architecture, build commit/environment,
a generated installation identifier, and game stage/menu/frame numbers. Full
module paths can contain a Windows account name. Sentry receives the connection's
IP address; normal Sentry project data-scrubbing and retention settings apply.

The integration uses the SDK's **native stacktrace-only** reporting mode. It does
not attach minidumps, screenshots, recordings, logs, ROM files, save files, user
settings, or extracted game assets. A stacktrace is technical diagnostic data;
this is not an anonymous-reporting claim.

The dedicated executable-relative queue is `Data/CrashReports`. Offline HTTP retry
is enabled, with startup pruning limits of ten events, 5 MiB and seven days. These
are SDK cache-pruning limits, not a promise that the filesystem never temporarily
exceeds them. Opt-out discards queued reports, including on the next startup;
reports already transmitted cannot be recalled. Queue cleanup never follows
directory junctions or symbolic links. Keep this directory out of bug-report ZIPs.
The pinned SDK retries connection failures; it treats HTTP responses, including
5xx server errors, as final rather than automatically retrying them.

## Build and CI

The game stays on the existing GCC/MinGW **x86** toolchain. The Windows CI job
builds an unmodified, commit-pinned Sentry Native SDK using MSVC **Win32**, the
`native` backend and static Microsoft runtimes. It packages `sentry.dll`,
`sentry-crash.exe`, `sentry-wer.dll`, the SDK license and input/hash metadata.
The game loads the DLL from its own directory with restricted dependency lookup;
missing or incompatible reporting files do not prevent gameplay. The SDK and
headers must always come from the same pinned revision.

`scripts/port/sentry_config.json` contains the SDK/CLI pins, project and **public
DSN**. The DSN permits ingestion; it is not an administrative credential. Never
put a privileged auth token in that file, a game binary, an artifact or a log.

The GitHub Actions repository secret `SENTRY_AUTH_TOKEN` must hold a Sentry
organization CI token (`org:ci`) permitted to upload debug files for this project.
The upload step runs only in the trusted main build, before publication, and fails
if the secret or usable symbols are missing. PR checks do not receive the token.
The pinned CLI download is checked against GitHub's asset SHA256 digest.

GNU ld's `--build-id` emits the PE CodeView identifier needed to match this GCC
executable to its embedded DWARF information. CI uploads the exact executable,
source bundle and SDK debug files. Symbol files are not added to the player ZIP.
Existing release input fingerprints and attestations also cover the reporter.
The Windows smoke job exercises a real access violation in the packaged game
against a loopback receiver without a ROM, and gates release publication.

For a local build, first run the SDK builder on Windows with VS2022 C++ tools:

```powershell
python scripts/port/sentry_tools.py build --work build_win/sentry-sdk --output build_win/sentry-runtime
python scripts/port/sentry_tools.py stage --source build_win/sentry-runtime --destination build_win
```

Pass `-DMIDAS_SENTRY_INCLUDE_DIR=/home/dev/project/build_win/sentry-runtime/include`
to the existing Docker GCC CMake configuration, then rebuild normally. A Windows
build without this optional header path keeps local diagnostics and the opt-out,
but cannot start Sentry. Native Linux and browser reporting are unchanged.

## Validation and deliberate crash

```powershell
python scripts/port/test_sentry_tools.py
python scripts/port/test_sentry_windows.py --build build_win --output build_win/sentry-smoke
```

The Windows suite checks an enabled access violation, INI opt-out and queued-data
removal, runtime opt-out even with a read-only INI, process suppression, missing
DLL fallback, normal shutdown, watchdog error reporting, connection-failure retry, x86 debug identification
and event-only payloads. Each run uses a fresh audit directory, keeps diagnostic logs and removes its
copied executables. To deliberately crash an isolated copy of the game before ROM
loading, run `IronSights.exe --test-crash-reporting`. `--check-crash-reporting`
initializes and closes reporting without loading a ROM or crashing. These flags
never run during ordinary gameplay.

`GE_SENTRY_DSN` redirects an isolated test copy to a loopback receiver or a test
project; `GE_SENTRY_ENVIRONMENT` labels it. Never run dashboard tests against
players' normal save/configuration directory. Verify the test fault's function,
file, line and source snippet in Sentry against the **same executable's** debug
ID. Capturing an envelope alone does not prove server-side symbolication.

The existing local exception logger receives a context copy, preserving the
original CPU state for Sentry. Its filter explicitly forwards to the SDK after
local logging. The watchdog records an explicit error before its deliberate exit;
that event is not presented as an access violation. Abrupt process termination,
power loss and some resource-exhaustion failures can still prevent capture.
