# Hand, arm and weapon review gallery

Open [`review/hand-gallery/index.html`](../../review/hand-gallery/index.html)
locally in a browser. No web server or network connection is needed. GitHub
shows the HTML source; download/clone the directory to use the interactive view.

The 108-view matrix covers PP7, AK47/KF7 and Klobb, regular, fully settled ADS
and regular with Hip-Fire Arms (FORK-075) on, six wardrobes (blue, boiler,
jungle, snow, tuxedo, Connery), and original versus fork models. The hip-arms
stock cells, and the PP7 (which has native hands), show what their regular cells
do: only the KF7 and Klobb fork cells gain arms. The original 72 regular/ADS
cases run first in their old order, then the 36 hip-arms cases. Keep that
order: the shiny guns' level reflection (FORK-058) drifts by a few colour steps
with game time, so a reordered timeline once split a PP7 case's two samples
across a change and failed the repeat check. The same drift is why a few
hip-arms stock cells differ from their regular cells by up to 6 steps on the
gun metal. Filters, a larger image inspector, stock/fork keyboard switching,
and a pixel inspection toggle help compare small seams. All 38 native generated
icons are included: weapons (with the watch laser), the tank's strip image, the
unarmed card's rifle butt and, grouped separately,
the six wardrobe-specific chopping arms. These images are references, not automatic judgments of anatomy.

**Stock is a model control, not an upstream executable capture.** It disables
our added rifle hands and native arm replacement, using original weapon models
under the same fork camera and ADS pose. Upstream has no equivalent ADS mode.
For wardrobes not represented on a stock weapon, identical stock cells are
expected. The fork's ordinary settings and gameplay remain unchanged.

## Regenerate and review

Requirements: Python 3.10+, Git, your own `baserom.u.z64` in the repository root,
and a configured video build as described in the README. Windows uses the local
`goldeneye` Docker build image and `build_win`; Linux uses `build_pc_video`.
The capture needs a working local OpenGL display. No Python packages are needed.

```powershell
python scripts/port/hand_gallery.py generate
python scripts/port/hand_gallery.py verify
python scripts/port/test_hand_gallery.py
```

The script builds current sources before rendering. A different local build
command can be configured as a JSON argv array in Git config `midas.galleryBuild`;
it must rebuild the same configured executable. Do not point it at a stale build.
When the main game is running, build current sources into a separate CMake binary
directory and use `generate --executable build_win/hd-prototype/IronSights.exe`.
This skips the default build and leaves the running player executable untouched.
The supplied executable must already contain the current rendering sources.
The script copies the rebuilt
executable and DLLs into an isolated, ignored directory, creates its own settings
and icon cache, and leaves campaign saves, profiles and normal settings alone.
It removes its executable copy when capture finishes or fails. Diagnostic logs
and raw captures remain under `build_win/hand-gallery-*` for troubleshooting.

Every view has a fixed camera, neutral background, seed, FOV, wardrobe, gun,
lighting and settled pose. Mission-start hints are disabled in this isolated
fixture, alongside the other HUD overlays. Watch clock hands are frozen for review only. Two
successive samples must have identical pixels; the entire matrix is then
captured by a second game process and compared again. Native icon generation
performs its existing repeated-sample check. The script publishes output only
after these checks pass. It does not silently accept unstable renders.

The icon run and both render passes are separate game processes running side
by side, each in its own folder. The passes run unpaced (`GE_NOSLEEP`,
`maxfps=0`); `GE_FIXEDTICK` counts every frame as exactly one field without
waiting on the clock, so a capture's game time follows the frames drawn and
each case's settle time passes as fast as the GPU renders it. A full
regeneration takes about 40 seconds on the development machine, down from about
6 minutes when the steps ran one after another at 60 frames a second.
All three processes are silent: they run with SDL's dummy audio driver
(`SDL_AUDIODRIVER=dummy`), which never opens the sound device, and with music
and effects at 0 in the fixture's settings, so the unpaced level no longer
plays its music and effects at many times speed while the gallery runs.

`manifest.json` records the recipe, source-input fingerprint, ROM hash, GPU/GL
identity and file/pixel hashes. No timestamps or machine-specific absolute paths
appear in output. PNG encoding is canonical. Repeated captures on the recorded
renderer are checked exactly; different GPU drivers may produce different
pixels, and their identity is recorded for reviewers. A source fingerprint ties
images to render inputs without a self-referential commit ID.

## Local pre-commit hook

```powershell
python scripts/port/hand_gallery.py install-hook
```

This installs the repository's `.githooks/pre-commit` through this clone's local
Git configuration and records the Python interpreter. Git does not activate
hooks automatically on clone. The installer refuses to replace another hooks
directory or an existing custom pre-commit hook.
The hook also runs `bun scripts/bright-builds-check.ts all` before gallery
verification, so install Bun globally before enabling it. See the
[Bright Builds workflow](bright_builds_rules.md) for the legacy source baseline.

On every commit the hook checks the **staged** gallery against staged source
inputs and validates all image hashes. An unchanged, fresh gallery needs no
ROM, build or render. When render inputs change, it regenerates a stale working
gallery and stops the commit for image review and explicit staging. If a fresh
working gallery already exists, it simply asks you to stage it. It never stages
files itself. Partially staged render inputs are rejected before capture so the
images cannot accidentally represent unstaged code. After review:

```powershell
git add review/hand-gallery
python scripts/port/hand_gallery.py verify --index
git commit
```

### Strict and deferred inputs

The fingerprint has two tiers (design: [the native 64-bit plan](native_64bit_plan.md),
"Gallery rule: two tiers"). The manifest (schema 2) records
`inputs_sha256` over every rendering input and `strict_inputs_sha256` over the
strict tier only: the files in `STRICT_FILES` in `hand_gallery.py` (the fork's
hand, arm and wardrobe code, `src/game/gunfire.c`, `model.c` and `bondview2.c`,
the weapon icon files, `port/src/joy_port.c`, the generator and its template)
and everything under `port/fast3d/`. Every other rendering input is deferred.

- A strict mismatch is stale: the hook, `verify` and CI all fail.
- A mismatch only in the full fingerprint is pending. `verify` fails on it
  unless `--allow-pending` is given, in which case it prints
  `Gallery pending` and exits 0. Image, coverage and HTML checks still run.
- Pending is opt-in per clone: `git config midas.galleryDefer true`, or
  `python scripts/port/hand_gallery.py install-hook --defer`. With it set, the
  hook passes a pending gallery and says so. Without it (a clone that can
  render, such as the Windows laptop) the hook regenerates on any staleness,
  which clears pending. If generation fails there, the error points at this
  setting.
- A deferring clone may commit a strict-tier change on a branch (owner
  decision, 2026-11-16). When the staged gallery is stale for strict inputs,
  `midas.galleryDefer` is set and the checked-out branch is neither `main` nor
  a detached HEAD, the hook verifies the staged gallery with every check except
  strict freshness (schema, determinism flags, coverage, image hashes, HTML),
  then passes and prints
  `Hand gallery is STALE on branch <name>: strict inputs changed. Regenerate on a machine that can render before this reaches main.`
  If any of those checks fails, the hook behaves as before. On `main`, on a
  detached HEAD and in a clone without the setting nothing changed. The
  `verify` command has no such exception, so CI and `main` still fail on a
  strict mismatch; the branch lands only after `generate` on a machine that
  can render.
- CI runs `verify --allow-pending`: it accepts pending and logs it, and still
  fails on a strict mismatch.
- Pending is cleared by running `generate` on a machine that can render, at
  the end of each milestone at the latest, then reviewing and staging
  `review/hand-gallery`.
- A new file that draws hands, arms or wardrobe must be added to
  `STRICT_FILES` in the same change.

Rendering inputs conservatively include game/port source, headers, shader and
build configuration, the generator/template and release build workflow.
Documentation-only changes do not rerender. CI runs ROM-free workflow tests and
verifies the checked-in gallery's coverage, integrity and source fingerprint;
it cannot regenerate without a user's ROM and GPU.

## Version-control scope

The user's explicit review-gallery exception permits only the generated HTML,
manifest and final PNGs under `review/hand-gallery`. Keep ROMs, extracted models
and textures, raw BMPs, caches and executables out of Git. The images do not
grant a license to the underlying game artwork. Other audit captures stay local.
When changing hands or arms, also follow [the arm contracts](arm_rendering.md)
and update `FORK_CHANGES.md`.
