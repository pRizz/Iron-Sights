# Windows distribution and build provenance

The default branch is `main`. Every successful push (including documentation
changes) runs ROM-free tests, builds Windows x86, packages player files and
publishes a uniquely tagged GitHub Release. Manual dispatch on `main` supports
retries (`gh workflow run windows-release.yml -R pRizz/Project-Midas-ADS --ref main`).
If a push does not create a run, inspect Actions settings/policies and use this
manual main dispatch rather than uploading a locally modified executable.
Tags include run number, attempt and commit, so a rerun never replaces
an existing build. Releases are marked latest to support the README's simple
download link; they are development builds rather than manually certified
stable versions. PRs run tests only, without publication
permissions. The previous Linux/Windows three-day EXE-only artifact workflow
has been replaced by windows-release.yml; Linux can still be built locally.
The C test job only installs gcc-multilib. The Windows job installs the Win32
MinGW flavor; the unused POSIX variant is no longer downloaded. Existing runner
CMake/make tools are used when available. Dependency downloads use Ubuntu's
official HTTPS archive/security servers instead of the slow Azure mirror,
with bounded retries and step timeouts.

## Build caches and clean compilation

Every main push still configures, links, stamps, packages and attests a new
executable. No previous EXE, build directory, ROM or generated game data is
restored. Three independent caches avoid repeated work:

- Ubuntu `.deb` downloads for tests and the Windows toolchain. Every run gets
  fresh, signature-verified Ubuntu package indexes. Cached files must match the
  resolved package's exact size and SHA256 from those indexes before installation.
  Stale/corrupt archives are discarded and downloaded again. Cache keys include
  the dependency helper, Ubuntu architecture/profile and resolved package plan;
  fallback keys reuse only archives that still pass this validation. Installed
  package databases and signed indexes are not cached.
- The SDL development archive, keyed by version and SHA256 and checked again
  before extraction, including on cache hits.
- `ccache` C/C++ object files, capped at 256 MiB and keyed by runner image,
  toolchain identity and compiler/frontend content. ccache checks source,
  included headers and compiler flags and hashes compiler content. No unsafe
  sloppiness settings are enabled. Run-specific immutable cache keys with a
  toolchain prefix carry the latest objects forward. Only trusted main builds
  save caches; PR tests can restore dependency downloads but cannot publish them.

Fresh build metadata changes its own translation units even on a documentation
push. Linking and release provenance always run again. Compiler hit/miss counts
appear in the Actions summary; build-inputs.json records verified dependency
hashes, package-download reuse, compiler-cache version/mode and statistics.
Caches are disposable accelerators and may expire or be evicted; a miss builds
normally. Release assets retain their separate immutable publication/retention.

For independent source compilation, select **clean_build** in Run workflow, or:

```sh
gh workflow run windows-release.yml -R pRizz/Project-Midas-ADS --ref main -F clean_build=true
```

This bypasses object-cache restore/save and disables ccache for compilation;
dependency archives are still authenticated and may be reused. Attestations
continue to identify the producing source/workflow. Caching does not make a
build hermetic or promise byte-identical output across compiler/runner changes.

## One-time repository setup

- Enable Actions and allow the checked-in pinned actions to run. For an existing
  fork, explicitly enable the workflow with
  `gh workflow enable windows-release.yml -R pRizz/Project-Midas-ADS`.
- Use the canonical `git@github.com:pRizz/Project-Midas-ADS.git` origin URL,
  rather than the old renamed repository redirect.
- Enable **Settings > General > Releases > Enable release immutability**.
  Releases are assembled as drafts, then published only when every asset has
  been attached. Changing this setting requires repository administration.
- Allow the publish job's scoped `contents: write`, `id-token: write` and
  `attestations: write` permissions. Other jobs have read-only access.

Actions artifacts are retained for 30 days. Release assets preserve the player
ZIP, checksums, build identity, input fingerprint record and signed Sigstore
provenance bundle beyond workflow/log retention. Immutable publication protects
the release assets and tag from replacement; a self-reported build-info file
alone does not provide that assurance.

## What is built and shipped

The build uses a fresh GitHub-hosted Ubuntu 24.04 runner, the checked-out
commit, the SHA256-pinned SDL2 2.30.11 package, and runner-installed
MinGW/CMake/Python. Actions are pinned to full commit hashes. Tracked source
changes after compilation fail the job. No ROM, ROM URL or private asset secret
is used by CI. `build-inputs.json` records actual
compiler/runner versions and input hashes. This is recorded provenance, not
a claim of hermetic or byte-reproducible compilation across runner updates.
The existing CMake Debug configuration retains crash symbols and assertions;
the port's explicit -O2 optimization remains enabled, matching local builds.

`package_windows.py` uses an explicit file allow-list. It includes the EXE,
SDL2 DLL/license, controller defaults, shader list, instructions and metadata;
it never sweeps a local build directory or includes ROM files, personal settings,
profiles or saves. `GE_ROMDATA_FALLBACK` is off and generated weapon images are
not embedded. The three previously compiled menu/logo model files are now
served from the user's ROM at runtime, including their inline textures.
A clean checkout compiles without ROM extraction. Inherited checked-in
scaffolding, tables, fonts and port resources remain part of the source tree;
this is not a claim that every inherited source resource has been removed.

The ZIP and executable are both attested by the publish job in the same run.
The player downloads the ZIP; the separate executable is passed between jobs
only for its own attestation. `VERIFY.txt` describes checking the expected
repository, commit, workflow and branch. Provenance identifies the workflow
that produced the bytes; it is not an Authenticode signature or a security audit.

## Local preparation and About

`scripts/port/build_info.py` stamps UTC time, build count, full commit, branch,
dirty status and available GitHub run/release URLs into a generated header and
matching JSON. CI status requires an exact matching commit and a clean tree.
Local builds say local, and missing Git metadata never invents a commit/run.
`MIDAS_BUILD_TIME_UTC` or `SOURCE_DATE_EPOCH` fixes the timestamp for controlled
builds. These display fields are informational; use the attestation to verify
downloaded bytes.

Both Port Options menus open the About dialog. Windows uses native hyperlink
controls (blue underlined links), keyboard navigation and the default browser.
Only baked HTTPS GitHub links are opened. Without a usable browser the dialog
reports the failure. Linux provides native dialog buttons for the same links.
Scoped intentional waits keep About, missing-ROM dialogs and first-launch
preparation from tripping the Windows hang watchdog; normal monitoring resumes
when they return.

On Windows the first validated-ROM launch prepares icons using an isolated,
temporary copy of the current executable and its SDL DLL. It uses the game's
own OpenGL renderer and a neutral configuration, without Python or downloads.
A progress window allows skipping. The helper is removed after exit. Normal
game settings and campaign data are not used by the helper.

The cache resides in SDL's per-user application-data directory, keyed by the
normalized ROM fingerprint and generator version. It contains transparent
512x160 renders of 29 weapons plus six wardrobe variants, never a ROM copy.
Black/white renders must repeat exactly before native alpha recovery and
premultiplied area downsampling. Cache headers, dimensions, sizes and CRCs are
validated before use, with atomic publication and a per-user generation lock.
Corrupt/stale caches regenerate; unwritable folders, generation failure or
skipping fall back to weapon names. `last-preparation.log` preserves diagnostics.
About reports readiness. Linux can read the same cache format; native automatic
generation is currently Windows-only. Cross-GPU pixel identity is not promised.

Keep the generator version in `port_icon_cache_format.h` current whenever
rendering/material/camera changes require regeneration. ROMs, caches, helper
files and other generated assets remain ignored.
