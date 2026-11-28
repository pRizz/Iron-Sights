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
   60 requests an hour does not apply. libcurl needs redirects turned on for
   it (`CURLOPT_FOLLOWLOCATION`, HTTPS only, at most 5); WinHTTP follows them
   already.
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
   - Copy the campaign save folder to `Data/Updates/saves-before-<tag>/`
     first, with `save_backup.cpp`'s allowlist. Keep the newest two copies.
5. Restart through FORK-034's restart helper: the original command line on
   Windows, `execv` on Linux (`$APPIMAGE`, not `/proc/self/exe`, for an
   AppImage). On Linux `execv` keeps the process ID, so Steam still tracks the
   game.

### Confirming the new build

The new build finds `Data/Updates/pending.txt` early in startup, before
settings and saves load, and counts the attempt. When it reaches the legal
screen it deletes the `.old` files and the marker. If it finds the marker on
a third attempt, it restores the `.old` files from the journal, restarts into
the old build, and records the bad build so Automatic skips it until a newer
one appears. A crash before that startup code (a missing DLL, say) cannot be
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

`[updates] mode=off|notify|automatic`, `automatic` by default (decision 4).
Help lines go in `port/src/port_option_help.c` and `tools/midas_ini_help.txt`.
`settings_placement.py` must pass.

### Where the code goes

- `port/src/port_update.c` and `port/include/port_update.h`: state, throttle,
  version check, the swap and the confirmation. These are plain C and can be
  tested without a ROM or a network.
- `port/src/port_update_archive.c`: the zip, gzip and ustar readers over
  `puff`.
- Downloads go through the existing transport. The only edit under
  `port/fast3d/` is turning on libcurl's redirects, which is strict tier (see
  [Constraints](#constraints)).
- Hooks: the startup call beside `portSaveTransferStartup`, the legal-screen
  and file-select lines in `port_startup.c`, and the rows in `front.c` and
  `port_watchpages.c`.

## Constraints

- **Strict tier.** Any edit under `port/fast3d/` needs a fresh, reviewed hand
  gallery, so it lands from a machine that can render (AGENTS.md, Render
  review gallery). Keep that edit to the redirect switch.
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
- An update fixture: a local HTTPS server, honoured only in test builds
  (`GE_UPDATE_TEST`, never defined by a release CMake configuration), serves
  a fake release for an end-to-end run on Windows and Linux.
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

**UPD-002 - Updater core.** `port_update.c`, the archive readers, the
libcurl redirect switch (strict tier), the swap, restart and confirmation,
`test_update.sh` and the fixture. Reachable only through an ini key and
`--update-now`, so it can be tested before there is any UI.

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
| UPD-001 | Built; waiting on CI | `package_identity.py` writes `package.txt` and checks each package (FORK-095). Locally: `test_distribution.py`'s five new tests pass on Windows and in the x86_64 release image, and the zips packaged from `build_win` and `build_win64` (21 files each) pass `package_identity.check`. The Linux packages are first built and checked by CI. |
| UPD-002 | Not started | Needs UPD-001 released. Strict tier for the redirect switch. |
| UPD-003 | Not started | Needs UPD-002. |
| UPD-004 | Not started | Needs UPD-003. |
