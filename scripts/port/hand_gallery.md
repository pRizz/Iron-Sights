# Hand, arm and weapon review gallery

Open [`review/hand-gallery/index.html`](../../review/hand-gallery/index.html)
locally in a browser. No web server or network connection is needed. GitHub
shows the HTML source; download/clone the directory to use the interactive view.

The 72-view matrix covers PP7, AK47/KF7 and Klobb, regular and fully settled ADS,
six wardrobes (blue, boiler, jungle, snow, tuxedo, Connery), and original versus
fork models. Filters, a larger image inspector, stock/fork keyboard switching,
and a pixel inspection toggle help compare small seams. All 35 native generated
weapon icons are included, with the six wardrobe-specific chopping arms grouped
separately. These images are references, not automatic judgments of anatomy.

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
lighting and settled pose. Watch clock hands are frozen for review only. Two
successive samples must have identical pixels; the entire matrix is then
captured by a second game process and compared again. Native icon generation
performs its existing repeated-sample check. The script publishes output only
after these checks pass. It does not silently accept unstable renders.

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
