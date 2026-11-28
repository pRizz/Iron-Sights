# In-game update plan

Iron Sights will find newer builds on GitHub and install them itself. It will
keep the player's ROM, settings, saves, profiles and packs, and restart into
the new build. The owner asked for this on 2026-11-27 and accepted the plan
the same day with the five decisions below. Acceptance is not evidence that
any milestone works; the [milestone status](#milestone-status) records what
has been built and verified.

## Why

- Every push to `main` publishes a release marked latest (see
  [releases.md](releases.md)). Today a player updates by downloading the
  matching package, unpacking it and copying the program files over the old
  ones, keeping `Data` and `midas.ini`. Few players do that each time, so most
  play old builds and report bugs that are already fixed.
- Player data is already outside the program files. In the Windows zips, the
  Linux tarballs and the Steam Deck ZIPs, `Data/` and `midas.ini` sit beside
  the executable (`portInitRootDir`, `port/src/main.c`). The AppImage keeps
  them in `${XDG_DATA_HOME:-$HOME/.local/share}/iron-sights`. Replacing the
  program files is the whole update, so the player never enters their ROM or
  moves their settings again.

## What already exists

| Need | Existing piece |
|---|---|
| HTTPS download with progress and cancel | `port/fast3d/enhanced_fonts_http.cpp` (WinHTTP; libcurl, loaded when first needed, on Linux), used by Enhanced Fonts (FORK-052) and HD textures (FORK-053) |
| SHA-256 | `port/fast3d/hd_download_engine.cpp`, `save_backup.cpp` |
| Inflate (zip and gzip) | `port/fast3d/third_party/puff/puff.c` |
| Stage now, apply at the next start, restart with the same arguments | FORK-034's save import: `portSaveTransferStartup` and `restartAfterShutdown` (`port/fast3d/save_transfer.cpp`), a marker applied before settings and saves load, journal recovery (`save_restore.cpp`) |
| Build identity | `build_number` (commit count, only grows), `ci`, `dirty` and `release_url` in `port_buildinfo.h` and each package's `build-info.json` (`scripts/port/build_info.py`) |
| A per-flavor version file on every release | `build-info.json` (Windows x86), `windows-x64-build-info.json`, `linux-build-info.json`, `linux-x86_64-build-info.json` |
| A per-flavor checksum file | `SHA256SUMS.txt`, `SHA256SUMS-windows-x64.txt`, `SHA256SUMS-linux.txt`, `SHA256SUMS-linux-x86_64.txt` |
| Explicit package contents | `package_windows.py`'s and `package_linux.py`'s allowlists |

No new release asset is needed to find an update.

## Owner's decisions (2026-11-27)

1. **Every CI release is an update. Checks are throttled, not promoted.** The
   game checks at most twice a day: a check at launch runs only when 12 hours
   have passed since the last one that finished. **Check for updates** in the
   menu always checks. No release is marked as more important than another.
2. **Automatic mode holds the launch on purpose.** When a check is due, the
   game shows a small indicator while it checks and downloads, installs the
   update and restarts into the new build. The player always plays the latest
   build without relaunching by hand. Downloads from GitHub are expected to be
   quick. The indicator can be skipped (see [Design](#design)).
3. **No manifest signing yet.** An update is trusted because it comes over
   HTTPS from the repository's immutable release (release immutability is on,
   [releases.md](releases.md)) and matches that release's `SHA256SUMS` file.
   An ed25519 signature with a key baked into the game is the planned
   hardening, recorded under [Later](#later-not-planned-here).

4. **Automatic is the default, with no consent screen.** The player already
   downloaded the game from GitHub, so checking GitHub again needs no
   separate question. The Enhanced Fonts rule ("nothing over the network
   until the player agrees") covers third-party content and does not apply
   here. A `midas.ini` without the key means Automatic. The player turns
   checks off, or down to Notify, in System > Updates. `START-HERE.txt` and the
   row's help text say that the game checks GitHub at most twice a day and
   where to turn it off.
5. **Limits for the held launch.** 5 s for the check (one small JSON file),
   and the download gives up after 15 s without progress. There is no
   overall cap, because a slow but moving download still finishes and the
   player can skip it. A failed or skipped check counts as a finished check
   for the throttle, so a bad connection holds the launch at most twice a
   day.

## Goals

- One setting, **Updates: Off / Notify / Automatic**, on every desktop
  package: Windows x86 and x64 zips, Linux i686 and x86_64 tarballs and
  AppImages, and Steam Deck ZIPs.
- Updating never touches player data under `Data/` (ROM if kept there,
  saves, profiles, logs, font and texture packs), `midas.ini`, or a ROM named
  by `[data] rom=` or on the command line. The two files a package itself
  puts in `Data/`, `README.txt` and `shaders_base.txt`, are program files and
  are replaced like the rest.
- The new build starts with the same arguments, so a dragged ROM path or a
  Steam launch option still applies.
- A failed download, a bad checksum, an unwritable folder or no network
  leaves the installed build untouched and boots it.
- A new build that cannot start puts the previous one back by itself.
- Every input works on every new screen and row: keyboard, mouse, gamepad and
  browser touch (the web build only gets the menu row's "not available" text).

## Non-goals

- The web build updates when the page reloads. A "new version, reload" banner
  is a later item.
- macOS has no shipped build yet (MAC-004 to MAC-006). Its updater waits for a
  signed, notarized `.app`.
- No delta or partial updates. The whole package is downloaded.
- No release notes inside the game. A **What's new** row opens the release
  page in the browser, as About's links already do.
- No choice of an older build, and no channels.

## Anti-goals

- Never update a local or modified build: `PORT_BUILD_CI` must be 1 and
  `PORT_BUILD_DIRTY` 0. The owner's `build_win/IronSights.exe`, audit copies,
  comparison builds (`GE_COMPARE`) and every developer tree are never
  replaced. This is checked in code, not left to the setting.
- Never update during play. Checks and installs happen at launch or from the
  menu row only.
- Never write outside the install's own program files (those its
  `SHA256SUMS.txt` lists) and `Data/Updates/`.
- Never follow a URL from a response. The game builds every address itself
  from the repository name compiled into it and a tag that must match
  `build-[0-9]+\.[0-9]+-[0-9a-f]{12}`.

## Design

### Finding the install's package

Two files beside the executable, written by the packagers
(`scripts/port/package_identity.py`, UPD-001):

- `package.txt`: `schema=1` and `flavor=windows-x64-zip` (or
  `windows-x86-zip`, `linux-i686-tar`, `linux-x86_64-tar`,
  `linux-i686-appimage`, `linux-x86_64-appimage`, `steamdeck-i686`,
  `steamdeck-x86_64`). `package_identity.FLAVORS` maps each flavor to its
  release package, checksum file and build-info file; UPD-002's C table must
  match it.
- `SHA256SUMS.txt`: every other file the package installs, by path relative
  to the executable's folder, with its SHA-256. Every package already had
  this file before UPD-001; it is now checked to list every file and only
  those, and it lists `package.txt`. It is the list of program files the
  updater may replace. (The plan first proposed a separate
  `package-files.txt`; UPD-001 used this existing list instead of adding a
  second one.)

Without `package.txt` (an older package, an unpacked developer tree) the
updater is off and says so ("This copy cannot update itself. Download the
latest build from GitHub."). An AppImage is the one file in `$APPIMAGE`; its
`package.txt` is inside the image.

### Checking

1. Fetch `https://github.com/pRizz/Iron-Sights/releases/latest/download/<flavor's build-info file>`.
   This is a redirect to the asset, not the GitHub API, so the API's limit of
   60 requests an hour does not apply. Both transports already follow it:
   WinHTTP by default, and `enhanced_fonts_http.cpp` sets libcurl's
   `CURLOPT_FOLLOWLOCATION`. (The plan first expected to add that switch;
   UPD-002 found it there, so nothing under `port/fast3d/` changes.)
2. Read `build_number`, `commit`, `ci` and `release_url`. Offer the build only
   when `ci` is true and `build_number` is larger than this build's. Take the
   tag from `release_url` and download everything else from
   `releases/download/<tag>/...`, so a release published during the update
   cannot mix two builds.
3. Record the check's time, result and the newest build seen in
   `Data/Updates/state.txt` (not `midas.ini`, which holds settings only).

### Downloading and installing

1. Download the package and its `SHA256SUMS` file to `Data/Updates/<tag>/`.
   Check the package's hash against the sums file.
2. Unpack it into `Data/Updates/<tag>/staged/`: zips through a small central
   directory reader and `puff`, tarballs through gzip and ustar readers and
   `puff`. Strip the package's top folder if it has one. Refuse absolute paths, `..`,
   links and any file not in the staged `SHA256SUMS.txt`. Check every file's
   hash against that list.
3. Refuse the install when the program folder is not writable (an install
   under `Program Files`, a read-only mount). Notify mode then shows the
   download page instead.
4. Write a journal, then swap file by file: rename each current program file
   to `<name>.old` and move the staged file into place. Windows lets a running
   executable and its loaded DLLs be renamed but not overwritten, so the
   rename comes first on every platform. Delete files that the old
   `SHA256SUMS.txt` listed and the new one does not. If any step fails,
   undo the renames from the journal and boot the old build.
   - AppImage: download beside `$APPIMAGE` and rename over it. Its launcher
     (`scripts/port/linux/AppRun`) copies `Data/shaders_base.txt` into the
     data folder only when it is missing, so an updated AppImage would keep
     the old list; UPD-002 makes AppRun refresh the copy when it differs.
   - A package may not write player data: an entry named `midas.ini`, or one
     under `Data/` other than `Data/README.txt` and `Data/shaders_base.txt`
     (in any case), fails the install before anything moves
     (`portUpdatePackagePath`). This replaced the first draft's copy of the
     save folder before each swap: the swap cannot reach the saves at all.
   - A program file whose bytes differ from the installed `SHA256SUMS.txt`
     (one the player edited, such as `gamecontrollerdefault.txt`) is replaced
     like the others, and the edited copy is kept as `NAME.edited`.
5. Restart with the original command line on Windows (`CreateProcessW` with
   `GetCommandLineW`, then exit) and `execv` on Linux (`$APPIMAGE`, not
   `/proc/self/exe`, for an AppImage). On Linux `execv` keeps the process
   ID, so Steam still tracks the game. FORK-034's restart helper does the
   same, but it is a static function in `port/fast3d/save_transfer.cpp`
   (strict tier), so `port_update_startup.c` carries its own small copy.
6. The updater writes each line it logs to `Data/Updates/log.txt` as well
   as the run's log, since the restarted game starts its own
   `ge_direct.log`. The file moves to `log.old.txt` past 64 KB.

### Confirming the new build

Every start reads `Data/Updates/journal.txt` early, before settings and
saves load (`portUpdateRecover`, called from `main.c` before the arenas are
pinned). A journal without its closing `installed` line is an interrupted
swap, and it is undone. A finished one counts the start in
`Data/Updates/attempts.txt`. When the new build first draws the legal screen
(`portUpdateLegalScreen`), it deletes the `.old` files, the journal and the
count. On a third start without that, it restores the `.old` files from the
journal, restarts into the old build, and records the bad build in
`state.txt` so Automatic skips it until a newer one appears. A crash before that startup code (a missing DLL, say) cannot be
caught this way. The player can still copy the `.old` files back by hand, and
`START-HERE.txt` says how.

Suspend points belong to one executable and do not load in the new build.
Campaign saves, profiles and settings do. The icon cache rebuilds itself
when its generator version changes (about 6 s).

### Screens

- **Indicator (Automatic, check due).** It is drawn on the legal screen
  (`port/src/port_startup.c`), which every input already reaches and which
  draws text in the game's fonts. Lines: "Checking for updates...", then
  "Downloading build 452 - 12.4 of 24.1 MB", then "Installing...
  restarting". Below them: "<control> to skip", naming the current input's
  control (Esc, a click, B, a tap). Skip boots the installed build and counts
  as a finished check. A launch with no check due shows nothing.
- **Notification (Notify).** The check runs in the background at launch.
  When it finds a build, a dim line appears beside the file-select build
  stamp: "Build 452 is available - System > Updates". It never appears in a
  mission.
- **Menu.** Gameplay > Iron Sights > System > **Updates**, in both the
  Options folder (`src/game/front.c`) and the watch
  (`port/src/port_watchpages.c`):
  - **Updates**: Off / Notify / Automatic (`[updates] mode=`).
  - **Check for updates**: checks now, ignoring the throttle.
  - A status line: "Up to date (build 446)", "Build 452 available",
    "Last check failed: No connection", or "This copy cannot update itself".
  - **Install build 452**: shown when one is found. It uses the indicator's
    screen, then restarts.
  - **What's new**: opens the new build's release page.
  All of these are ordinary rows and actions, which every supported input
  can already confirm, cancel and adjust.

### Settings

`[updates] mode=off|notify|automatic`, `automatic` by default (decision 4)
from UPD-003. In UPD-002 the updater reads the key from `midas.ini` itself
(`portUpdateModeFromIni`) and defaults to `off` (`PORT_UPDATE_DEFAULT_MODE`):
without the indicator, an automatic update would hold the launch with
nothing on screen. `notify` does not check until UPD-003 gives it its line.
UPD-003 makes the key an option row, with its help line in
`port/src/port_option_help.c`, and flips the default.
`tools/midas_ini_help.txt` documents it from UPD-002. `settings_placement.py`
must pass.

### Where the code goes

As built in UPD-002 (`port/include/port_update.h` documents the interface),
all under `port/src/`:

- `port_update_core.c`: settings, the flavor table, build-info parsing, tag
  and path checks, checksum lists. No files and no network.
- `port_update_sha256.c`: SHA-256 in C (the renderer's are C++ in
  `port/fast3d/`).
- `port_update_archive.c`: the zip, gzip and ustar/pax readers over `puff`.
- `port_update_files.c` and `port_update_files.h`: files, folders, renames,
  the install lock and the journal.
- `port_update.c`: the check, download and staging. `port_update_swap.c`:
  the swap, its undo, recovery and confirmation.
- `port_update_startup.c`: this executable's paths and build, the restart,
  the update log and the two hooks. `port_update_http.cpp`: the bridge onto
  the existing stream transport. It runs the transport on a worker thread so
  that the 5 s and 15 s limits hold whatever the transport's own timeouts
  are.
- CMake builds them only for Windows and Linux desktop builds with video,
  never a comparison build, and leaves them out elsewhere. `GE_UPDATER` is
  defined only for `main.c` and `port_startup.c`, which hold the hooks. The
  ROM-free harness compiles all but the startup and HTTP files.
- UPD-003 adds the rows in `front.c` and `port_watchpages.c`, and the
  indicator and file-select lines in `port_startup.c`.
- The game defines its own `isdigit` (`src/str.c`), and its include folders
  shadow `<limits.h>` and `<string.h>`; the latter declares no `strncmp`,
  `strcpy` or `strcat`. The updater uses none of these, so the Windows link
  and `lp64_diagnostics.py unprototyped` stay clean.

## Constraints

- **Strict tier.** Any edit under `port/fast3d/` needs a fresh, reviewed hand
  gallery. The updater makes none; it only includes the transport's header.
- **Load before play.** The check and download run at launch, before play,
  which is where AGENTS.md wants first-use work. The Notify check runs on a
  background thread and must never stall a frame. Only its result is shown.
- **No per-frame work** beyond drawing one line of text. No GL queries.
- **Settings placement** and **input idioms** as above.
- **Privacy.** The request carries no player data: a fixed `User-Agent:
  IronSights` and no query string.
- **Packaging.** `package.txt` joins every package, and
  `package_identity.check` holds each package to it: `test_distribution.py`
  on the Windows zips and the Steam Deck ZIP, and `test_sentry_linux.py` on
  the built tarball, Steam Deck ZIP and extracted AppImage in CI.

## Verification

- `scripts/port/test_update.sh`, at every width `harness_widths.sh` offers and
  named in CI: throttle arithmetic, version comparison, tag and path
  checking, archive reading (zip, tar.gz, refused entries), the swap and its
  undo on an injected failure, the rename-first rule, and the confirmation
  count and restore. It uses a temporary folder and needs no ROM or network.
- An end-to-end run on a real executable: `scripts/port/update_e2e.py`
  serves a fake release over plain HTTP on 127.0.0.1 to a test build
  (`-DGE_UPDATE_TEST=ON`, which only `port_update_startup.c` sees and no
  release configures). Only such a build reads `GE_UPDATE_TEST_DOWNLOADS`
  (an `http://127.0.0.1:` address), `GE_UPDATE_TEST_BUILD` and
  `GE_UPDATE_TEST_CI`. The run installs the test exe as build 100. It must
  update itself to build 200 while it runs, restart, reach the legal screen
  and confirm; the run then checks the files, the player data and the update
  log. It covers the Windows zip flavor. The Linux flavors are covered by
  the harness until UPD-004's release cycle.
- A real release cycle on each flavor: install release N with a ROM in
  `Data`, saves and changed settings, push N+1, and check that Automatic
  installs it on launch and Notify offers it, with the ROM, saves, settings
  and packs unchanged and the old program files gone.
- No benchmark is needed (nothing new per frame). Run
  `late_load_audit.py` once to show the stage loads are unchanged.

## Milestones

**UPD-001 - Package identity.** `package.txt` in every package and a
`SHA256SUMS.txt` checked to list every file, with `test_distribution.py`
and `test_sentry_linux.py` checks. Changes no game code. CI
releases carry the files from then on.

**UPD-002 - Updater core.** The updater files above, the swap, restart and
confirmation, the update log, `test_update.sh`, `update_fixture.py`,
`update_e2e.py`, and AppRun's refresh of the AppImage's own files. Reachable
only through `[updates] mode=automatic` and `--update-now`, so it can be
tested before there is any UI.

**UPD-003 - Settings and screens.** The menu rows in both menus, the
indicator, the file-select line,
help text and settings placement, all checked with every input.

**UPD-004 - Release and hand-over.** The real release cycle on every flavor,
`START-HERE.txt` and `DATA-README.txt` updated ("This build checks GitHub
for new builds at most twice a day and updates itself; turn it off in
Options > Gameplay > Iron Sights > System > Updates"), and [releases.md](releases.md) updated.

## Later (not planned here)

- An ed25519-signed manifest, with the public key in the game (decision 3's
  hardening).
- A "new version, reload" banner in the web build.
- macOS, after MAC-006, through Sparkle or a bundle swap.
- A **Roll back to previous build** row.
- Release notes in the game.

## Milestone status

| Milestone | Status | Notes |
|---|---|---|
| UPD-001 | Done (2026-11-28) | On `main` as 0f82e0d; every CI job passed, including `test_distribution.py` (36 tests) and both Linux jobs' `test_sentry_linux.py`, which ran `package_identity.check` on the built tarballs, Steam Deck ZIPs and extracted AppImages; release `build-453.1-0f82e0da71ea` is the first with `package.txt`. `package_identity.py` writes `package.txt` and checks each package (FORK-095). Locally: `test_distribution.py`'s five new tests pass on Windows and in the x86_64 release image, and the zips packaged from `build_win` and `build_win64` (21 files each) pass `package_identity.check`. |
| UPD-002 | Done (2026-11-28) | On `main` as 87c8d4a, with fixes a63b6fe and db85471 after CI caught a file missing from CMake's updater list, a buffer GCC 13 flagged, and two undeclared wide-string calls in the MinGW tree. Every CI job passed on db85471 (`test_update.sh` at both widths, the three gate trees, the narrowing and prototype checks, the macOS job, the packaged Windows and Linux builds), and release `build-457.1-db85471e61ea` is the first with the updater, off by default. The updater (FORK-095) with no screens yet: it runs only with `[updates] mode=automatic` or `--update-now`, and the default is `off` until UPD-003. No strict-tier file changed (libcurl already followed redirects). Locally: `test_update.sh` passes at 64 bits (x86_64 release image) and 32 bits (`midas-lp64-i686-video`); `update_e2e.py` passes all eleven checks on a 32-bit Windows `GE_UPDATE_TEST` build; `build_win`, `build_win64` and the test tree build and link; `lp64_diagnostics.py gate` compiles a 64-bit Linux video tree, 402 files with 0 failures; narrowing finds none; `unprototyped --check` passes; `lp64_casts.py` and `lp64_overlays.py check --no-defects` pass; `test_linux_launcher.py` passes. A MinGW x64 gate tree in this container lacked SDL headers for 11 renderer and input files (none of them the updater's), so CI's tree is the check there. A 64-bit Linux video link and the Linux packages are first built by CI. |
| UPD-003 | Not started | Ready to start. |
| UPD-004 | Not started | Needs UPD-003. |
