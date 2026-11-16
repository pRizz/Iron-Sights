# LP64-001 Scaffolding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put in place everything the 64-bit port needs before any game code
changes: the two-tier gallery rule, a working toolchain on the owner's Mac, and
the gates that measure pointer-width problems.

**Architecture:** Six small tools and one build option, each with ROM-free
tests. Python tools keep their logic in pure functions and their Docker, Git
and compiler calls in a thin `main`. Nothing in this milestone changes how any
shipped build behaves.

**Tech Stack:** Python 3 standard library (no packages), CMake, GCC 13 in
Ubuntu 24.04 containers, Docker Desktop on Apple Silicon, Bash for the
`test_*.sh` harnesses.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md). Read it first, then
`AGENTS.md`, `scripts/port/hand_gallery.md` and `scripts/port/arm_rendering.md`.

## Global Constraints

- The 32-bit Windows, Linux and web builds keep working and keep shipping. No
  task here changes game, engine, port or renderer source.
- No ROM-derived data in Git, including test fixtures. The ROM is the ignored
  file in the checkout root; tools find it through `GE_ROM`.
- Python tools use the standard library only.
- Every commit updates `FORK_CHANGES.md` (extend FORK-089; Task 1 instead
  updates FORK-022, the gallery's entry). Record what was built and what was
  run, not what is hoped.
- Delivery is direct to `main` with a push per task, except Task 1, which goes
  through a branch and the owner's Windows laptop (see Task 1). A task is
  pushed only after its review is clean, because every push to `main`
  publishes a development build.
- Before each commit run `bun scripts/bright-builds-check.ts all`,
  `python3 scripts/port/settings_placement.py` and the gallery hook. Task 2
  installs the hook so this happens on `git commit`.
- New files stay under 500 lines. Shell scripts start with
  `#!/usr/bin/env bash` and `set -euo pipefail`.
- Tests follow Arrange, Act, Assert, one concern each.
- Build output goes under `build/lp64/`, which Git already ignores.
- Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

## Task order and the gallery gate

`scripts/port/hand_gallery.py`, `CMakeLists.txt`,
`.github/workflows/windows-release.yml` and `scripts/port/ci_dependencies.py`
are gallery inputs today. Changing any of them makes the gallery stale, and the
Mac cannot regenerate it.

- **Tasks 2, 3, 4 and 5** touch none of those files. They land on `main` from
  the Mac in any order.
- **Task 1** changes the gallery tool, so it is built on a branch and lands
  through one regeneration on the laptop. Do Task 1 through its Step 6
  **before** Task 2's Step 7 installs the hook on the Mac: with the hook
  installed, the Mac cannot commit a strict-tier change at all, which is the
  rule working as designed. Never use `--no-verify` to get around it.
- **Tasks 5b, 6 and 7** change deferred-tier inputs. They wait until Task 1 is
  on `main` and the Mac's clone has pending enabled. Task 5b goes first.

## Review Focus

Failure modes the spec implies that are most likely to bite, each pinned by a
test in the task that owns the code:

1. A strict-tier file is renamed or deleted and silently drops into the
   deferred tier. Expected: the test suite fails. (Task 1, Step 1.)
2. A clone that cannot render has no pending setting, so the hook tries to
   regenerate and fails obscurely. Expected: the error names
   `midas.galleryDefer`. (Task 1, Step 1.)
3. The ROM's file name has spaces and brackets
   (`GoldenEye 007 (U) [!].z64`). Expected: it reaches Docker as one intact
   argument, read-only. (Task 2, Step 1.)
4. The diagnostics baseline was recorded with one compiler and is checked with
   another. Expected: a refusal that names both, not a false regression.
   (Task 4, Step 1.)
5. Two executables differ only in their timestamp or debug sections. Expected:
   reported identical. A one-byte change in `.text` is reported as different.
   (Task 5, Step 1.)

---

### Task 1: Two-tier gallery rule

**Files:**
- Modify: `scripts/port/hand_gallery.py`
- Modify: `scripts/port/test_hand_gallery.py`
- Modify: `scripts/port/hand_gallery.md`, `AGENTS.md` ("Render review
  gallery"), `FORK_CHANGES.md` (FORK-022 only),
  `.github/workflows/windows-release.yml` (the `hand_gallery.py verify` line)
- Regenerated on the laptop: `review/hand-gallery/`

**Interfaces:**
- Produces, in `hand_gallery.py`:
  - `STRICT_FILES: tuple[str, ...]` and `STRICT_PREFIXES = ('port/fast3d/',)`
  - `strict_path(name: str) -> bool`
  - `source_fingerprint(root=ROOT, staged=False, strict=False) -> str`
  - `freshness(manifest: dict, root=ROOT, staged=False) -> str`, one of
    `'fresh'`, `'pending'`, `'stale'`
  - `defer_enabled(root=ROOT) -> bool`
  - `verify(root=ROOT, staged=False, allow_pending=False) -> dict`
  - Manifest `schema` 2 with a new key `strict_inputs_sha256`
  - CLI: `verify --allow-pending`, `install-hook --defer`
  - Git setting: `midas.galleryDefer` (boolean, local to the clone)

`STRICT_FILES`, exactly:

```python
STRICT_FILES = (
    'port/src/port_arm_mesh.c', 'port/src/port_native_arms.c',
    'port/src/port_grip_mesh.c', 'port/src/port_wardrobe.c',
    'port/src/ads_hands.c', 'port/src/port_knife_ads.c',
    'port/src/port_graphics.c', 'port/src/port_shiny.c',
    'src/game/gunfire.c', 'src/game/model.c', 'src/game/bondview2.c',
    'port/src/port_weapon_images.c', 'port/src/port_hud_weapons.c',
    'port/src/joy_port.c',
    'scripts/port/hand_gallery.py', 'scripts/port/hand_gallery_template.html',
)
```

- [ ] **Step 1: Create the branch and write the failing tests**

```bash
git switch -c lp64/gallery-two-tier
```

Extend the `fixture(root)` helper so the temporary repository also has a
strict file, `port/fast3d/example.cpp`, staged. Make `references(root)` write
`schema=2` and `strict_inputs_sha256=gallery.source_fingerprint(root, strict=True)`.
Add these tests to `class Gallery`:

```python
def test_strict_tier_membership(self):
    for name in gallery.STRICT_FILES:
        self.assertTrue(gallery.input_path(name), name)
        self.assertTrue(gallery.strict_path(name), name)
        self.assertTrue((gallery.ROOT/name).is_file(), name)   # Review Focus 1
    self.assertTrue(gallery.strict_path('port/fast3d/gfx_pc.cpp'))
    for name in ('src/game/front.c', 'port/src/romdata.c', 'CMakeLists.txt',
                 '.github/workflows/windows-release.yml', 'notes.md'):
        self.assertFalse(gallery.strict_path(name), name)

def test_deferred_change_is_pending(self):
    # Arrange: fresh references, then change the deferred file and stage it.
    # Act / Assert:
    self.assertEqual(gallery.freshness(manifest, root, True), 'pending')
    with self.assertRaisesRegex(ValueError, 'stale'):
        gallery.verify(root, True)
    gallery.verify(root, True, allow_pending=True)

def test_strict_change_is_stale_even_when_pending_allowed(self):
    # Arrange: fresh references, then change port/fast3d/example.cpp, staged.
    self.assertEqual(gallery.freshness(manifest, root, True), 'stale')
    with self.assertRaisesRegex(ValueError, 'strict'):
        gallery.verify(root, True, allow_pending=True)

def test_hook_passes_pending_only_with_the_clone_setting(self):
    # Arrange: deferred change staged.
    # Without the setting: generate is attempted (mock it to raise OSError)
    # and the error text contains 'midas.galleryDefer'.      # Review Focus 2
    # With `git config midas.galleryDefer true`: pre_commit returns, and
    # generate is not called.

def test_schema_1_manifest_must_be_regenerated(self):
    # A manifest with schema 1 and no strict fingerprint raises
    # ValueError matching 'regenerate'.
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `python3 scripts/port/test_hand_gallery.py`
Expected: FAIL, `AttributeError: module 'gallery' has no attribute 'STRICT_FILES'`

- [ ] **Step 3: Implement the two tiers in `hand_gallery.py`**

- `strict_path` is `input_path(name)` and (in `STRICT_FILES` or under a
  `STRICT_PREFIXES` entry).
- `source_fingerprint(..., strict=True)` hashes only `strict_path` entries;
  with `strict=False` it is unchanged, so existing fingerprints keep their
  meaning.
- `freshness` returns `'stale'` when the strict fingerprint differs,
  `'pending'` when only the full one does, else `'fresh'`.
- `verify` requires `schema == 2`. It raises
  `'Gallery is stale for strict rendering inputs; regenerate references'` on
  stale, and `'Gallery is stale for these rendering inputs; regenerate references'`
  on pending unless `allow_pending`. All image, coverage and HTML checks still
  run in every case.
- `defer_enabled` reads `git config --get --bool midas.galleryDefer`.
- `pre_commit` passes `allow_pending=defer_enabled(root)`, and on pending
  prints `Hand gallery is pending: strict inputs fresh, deferred inputs changed since the last render.`
  When generation fails and pending is not enabled, the raised message ends
  with
  `If this machine cannot render, set midas.galleryDefer (see scripts/port/hand_gallery.md).`
- `generate` writes schema 2 with both fingerprints.
- `main`: `verify --allow-pending` exits 0 on pending and prints
  `Gallery pending: strict inputs fresh; deferred inputs changed since the last render.`
  `install-hook --defer` also sets `midas.galleryDefer true`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `python3 scripts/port/test_hand_gallery.py`
Expected: `OK`, including the nine tests that existed before.

- [ ] **Step 5: Update the rule's documents and CI**

- `.github/workflows/windows-release.yml`: change
  `python3 scripts/port/hand_gallery.py verify` to
  `python3 scripts/port/hand_gallery.py verify --allow-pending`.
- `AGENTS.md`, "Render review gallery": state the two tiers, that pending is
  opt-in per clone, that a clone able to render leaves it off, and that a new
  file which draws hands, arms or wardrobe must be added to `STRICT_FILES` in
  the same change.
- `scripts/port/hand_gallery.md`: the strict list, the setting, the CI
  behaviour, and how pending is cleared.
- `FORK_CHANGES.md`: FORK-022 describes the rule as current and points to the
  64-bit plan for why it changed. Do not edit FORK-089 on this branch: Tasks 2
  to 5 extend that entry on `main` while the branch waits, and Task 7 corrects
  its "planned, not yet built" wording after the merge.

- [ ] **Step 6: Commit and push the branch**

`python3 scripts/port/hand_gallery.py verify` fails on this branch until the
laptop regenerates. That is expected; say so in the commit body. The hook is
not installed on the Mac yet, so run its other two checks by hand first:
`bun scripts/bright-builds-check.ts all` and
`python3 scripts/port/settings_placement.py`. Any CI run on the branch is
expected to fail its gallery check for the same reason.

```bash
git add scripts/port/hand_gallery.py scripts/port/test_hand_gallery.py \
  scripts/port/hand_gallery.md AGENTS.md FORK_CHANGES.md \
  .github/workflows/windows-release.yml
git commit -m "Split the review gallery's inputs into strict and deferred tiers"
git push -u origin lp64/gallery-two-tier
```

- [ ] **Step 7: Hand off to the laptop (the owner, or a session there)**

On the Windows laptop, where the clone leaves `midas.galleryDefer` unset:

`main` will have moved while the branch waited (Tasks 2 to 5 land there), and
the gallery must be rendered from the combined source, so rebase first. The
branch touches no file those tasks touch, so the rebase is expected to be
clean; if it is not, stop and resolve it before rendering.

```bash
git fetch origin && git switch lp64/gallery-two-tier
git rebase origin/main
python scripts/port/hand_gallery.py generate
```

Review the images (no image is expected to change; the manifest gains
`schema: 2` and `strict_inputs_sha256`). Then:

```bash
git add review/hand-gallery
git commit -m "Regenerate the review gallery for the two-tier manifest"
git switch main && git pull --ff-only
git merge --ff-only lp64/gallery-two-tier && git push origin main
```

The rebased branch itself is not pushed again; `main` carries it from here.

- [ ] **Step 8: Enable pending on the Mac and confirm**

```bash
git switch main && git pull --ff-only
git config midas.galleryDefer true
python3 scripts/port/hand_gallery.py verify
```

Expected: `Gallery verified: 108 matrix cells, 38 icons, ...`

---

### Task 2: Toolchain on the Mac

**Files:**
- Create: `scripts/port/lp64_env.py`, `scripts/port/test_lp64_env.py`
- Create: `scripts/port/lp64-arm64.Dockerfile`
- Modify: `scripts/port/native_64bit_plan.md` (status table notes),
  `FORK_CHANGES.md`

**Interfaces:**
- Produces, in `lp64_env.py`:
  - `ARCHES = ('i686', 'x86_64', 'aarch64')`
  - `find_rom(root: Path, environ: Mapping[str, str]) -> Path | None`:
    `GE_ROM` if set, else the single `*.z64` in `root`; `ValueError` if more
    than one.
  - `docker_command(arch: str, root: Path, maybe_rom: Path | None, command: list[str]) -> list[str]`
  - CLI: `python3 scripts/port/lp64_env.py build-images` and
    `python3 scripts/port/lp64_env.py run --arch ARCH -- COMMAND...`
- Fixed values: `i686` and `x86_64` use image `goldeneye` with
  `--platform linux/amd64`; `aarch64` uses image `midas-lp64-arm64` with
  `--platform linux/arm64`. The checkout mounts at `/home/dev/project` (the
  working directory). The ROM mounts read-only at `/rom/baserom.u.z64`, and the
  container gets `GE_ROM=/rom/baserom.u.z64`.

- [ ] **Step 1: Write the failing tests**

```python
def test_i686_and_x86_64_share_the_amd64_image(self):
    for arch in ('i686', 'x86_64'):
        argv = env.docker_command(arch, Path('/repo'), None, ['true'])
        self.assertIn('linux/amd64', argv)
        self.assertIn('goldeneye', argv)

def test_aarch64_uses_the_native_image(self):
    argv = env.docker_command('aarch64', Path('/repo'), None, ['true'])
    self.assertIn('linux/arm64', argv)
    self.assertIn('midas-lp64-arm64', argv)

def test_rom_with_spaces_and_brackets_is_one_read_only_argument(self):   # Review Focus 3
    rom = Path('/repo/GoldenEye 007 (U) [!].z64')
    argv = env.docker_command('i686', Path('/repo'), rom, ['true'])
    self.assertIn('/repo/GoldenEye 007 (U) [!].z64:/rom/baserom.u.z64:ro', argv)
    self.assertIn('GE_ROM=/rom/baserom.u.z64', argv)

def test_no_rom_means_no_rom_mount(self):
    argv = env.docker_command('i686', Path('/repo'), None, ['true'])
    self.assertFalse(any('/rom/' in part for part in argv))

def test_command_is_passed_through_unchanged(self):
    argv = env.docker_command('aarch64', Path('/repo'), None, ['sh', '-c', 'echo "a b"'])
    self.assertEqual(argv[-3:], ['sh', '-c', 'echo "a b"'])

def test_unknown_arch_is_rejected(self):
    with self.assertRaises(ValueError):
        env.docker_command('mips', Path('/repo'), None, ['true'])

def test_find_rom_prefers_ge_rom_and_rejects_ambiguity(self):
    # GE_ROM wins; one *.z64 in root is found; two raise ValueError; none -> None.
```

- [ ] **Step 2: Run and confirm failure**

Run: `python3 scripts/port/test_lp64_env.py`
Expected: FAIL, `FileNotFoundError` for `lp64_env.py`.

- [ ] **Step 3: Implement `lp64_env.py` and `lp64-arm64.Dockerfile`**

The Dockerfile is `ubuntu:24.04` with `build-essential cmake ninja-build python3 git`
and a `dev` user created from `HOST_UID`/`HOST_GID` build arguments, as the
root `Dockerfile` does. `build-images` builds the root `Dockerfile` as
`goldeneye` for `linux/amd64` and this one as `midas-lp64-arm64`, passing the
caller's uid and gid. `main` passes arguments to Docker as a list, never
through a shell.

- [ ] **Step 4: Run and confirm the tests pass**

Run: `python3 scripts/port/test_lp64_env.py`
Expected: `OK`

- [ ] **Step 5: Build the images and prove each architecture runs**

```bash
python3 scripts/port/lp64_env.py build-images
python3 scripts/port/lp64_env.py run --arch aarch64 -- gcc -dumpmachine
python3 scripts/port/lp64_env.py run --arch x86_64 -- gcc -dumpmachine
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c 'echo "int main(void){return sizeof(void*)==4?0:1;}" > /tmp/p.c && gcc -m32 /tmp/p.c -o /tmp/p && /tmp/p && echo i686-runs'
```

Expected: `aarch64-linux-gnu`, `x86_64-linux-gnu`, `i686-runs`.
If the last command fails with `Exec format error`, Docker Desktop is not
emulating 32-bit x86. Stop and report; do not work around it silently. The
spec depends on the i686 reference running here.

- [ ] **Step 6: Run the i686 reference headless with the ROM**

The stock headless configuration does not compile on `main`:
`port/src/port_baked_trace.c` (FORK-082) includes `SDL.h` and is not excluded
under `GE_HEADLESS`. Task 5b repairs that. Until it lands, pass a configure
flag and change no source. Try the test shim first, as below. If that does
not compile or link inside the game's include paths, use the image's own
32-bit SDL2 instead: replace the `-DCMAKE_C_FLAGS=...` argument with
`-DCMAKE_C_FLAGS=-I/home/dev/sdl2-linux-i686/include/SDL2` and add
`-DCMAKE_C_STANDARD_LIBRARIES="-L/home/dev/sdl2-linux-i686/lib/i386-linux-gnu -Wl,-rpath,/home/dev/sdl2-linux-i686/lib/i386-linux-gnu -lSDL2"`.
Record which one worked; Task 5b builds on that answer.
`audit_buffers.py` accepts `--frames` from 600 upward.

```bash
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c 'cmake -S . -B build/lp64/ref-i686 -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON -DCMAKE_C_FLAGS=-I/home/dev/project/scripts/port/sdl_shim && cmake --build build/lp64/ref-i686 -j"$(nproc)"'
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ref-i686/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 2 --output build/lp64/ref-i686/smoke.json
```

Expected: the build links `build/lp64/ref-i686/IronSights`, and the smoke run
reports every mission passing. Record the wall-clock time of both in the
status table; it is the emulation cost later milestones plan around.

- [ ] **Step 7: Install the hook, update documents, commit**

```bash
python3 scripts/port/hand_gallery.py install-hook
```

Add the commands of Steps 5 and 6, and the measured times, to LP64-001's row
in `native_64bit_plan.md`. Extend FORK-089.

```bash
git add scripts/port/lp64_env.py scripts/port/test_lp64_env.py \
  scripts/port/lp64-arm64.Dockerfile scripts/port/native_64bit_plan.md FORK_CHANGES.md
git commit -m "Add the 64-bit port's container runner for i686, x86_64 and aarch64"
git push origin main
```

---

### Task 3: Layout probe

**Files:**
- Create: `scripts/port/abi_probe/probe.c`, `scripts/port/test_abi_probe.sh`
- Modify: `FORK_CHANGES.md`

**Interfaces:**
- Produces: `bash scripts/port/test_abi_probe.sh`, which builds and runs the
  probe at every width the host compiler supports and prints one line per
  width, `ABI probe OK (32-bit)` or `ABI probe OK (64-bit)`. LP64-004 adds
  each file layout's assertions to `probe.c`.

- [ ] **Step 1: Write the probe (it is the test)**

Model it on `scripts/port/web_probe/probe.c`, without the Emscripten parts.
It includes `<ultra64.h>`, `<PR/abi.h>`, `"ob.h"` and `"platform_info.h"`.
Assertions, exactly:

```c
/* Fixed at every width: the renderer reads these as N64 formats. */
_Static_assert(sizeof(s32) == 4 && sizeof(u32) == 4, "32-bit integer types");
_Static_assert(sizeof(Vtx) == 16, "Vertex stride");
_Static_assert(sizeof(Mtx) == 64, "Matrix layout");
_Static_assert(sizeof(Vp) == 16, "Viewport layout");
_Static_assert(sizeof(Acmd) == 8, "Audio command stride");
/* Scales with the pointer. */
_Static_assert(IS_64_BIT == (sizeof(void *) == 8), "IS_64_BIT matches the host");
_Static_assert(sizeof(Gfx) == 2 * sizeof(uintptr_t), "Display-list command is two pointer words");
_Static_assert(offsetof(Gfx, words.w1) == sizeof(uintptr_t), "Display-list address word");
#if !IS_64_BIT
_Static_assert(sizeof(Gfx) == 8, "32-bit display-list stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");
#endif
```

`main` prints `ABI probe OK (%d-bit)\n` with `(int)(sizeof(void *) * 8)` and
returns 0.

- [ ] **Step 2: Write `test_abi_probe.sh`**

Follow `scripts/port/test_states.sh` for the compiler flags, defines, include
paths and temporary file handling. Compile `probe.c` twice: once with the
host's native width, once with `-m32 -msse2 -mfpmath=sse`. Skip the `-m32`
pass, printing `ABI probe: -m32 unavailable on this host, skipped`, when
`"${CC:-gcc}" -m32 -x c -fsyntax-only /dev/null` fails. Run each binary.

- [ ] **Step 3: Run it in each container**

```bash
python3 scripts/port/lp64_env.py run --arch x86_64 -- bash scripts/port/test_abi_probe.sh
python3 scripts/port/lp64_env.py run --arch aarch64 -- bash scripts/port/test_abi_probe.sh
```

Expected, x86_64: `ABI probe OK (64-bit)` and `ABI probe OK (32-bit)`.
Expected, aarch64: `ABI probe OK (64-bit)` and the skip line.
If a header does not compile at 64-bit, fix nothing in `include/` or `src/`
here: reduce the probe's includes to what it needs, and note the header in
the commit body for LP64-002.

- [ ] **Step 4: Prove the probe can fail**

Temporarily change `sizeof(Vtx) == 16` to `== 24`, run the x86_64 command,
confirm a compile error naming "Vertex stride", and restore the line.

- [ ] **Step 5: Commit**

```bash
git add scripts/port/abi_probe/probe.c scripts/port/test_abi_probe.sh FORK_CHANGES.md
git commit -m "Add a native layout probe for both pointer widths"
git push origin main
```

---

### Task 4: 64-bit diagnostics ratchet

**Files:**
- Create: `scripts/port/lp64_diagnostics.py`,
  `scripts/port/test_lp64_diagnostics.py`,
  `scripts/port/lp64_diagnostics_baseline.json`
- Modify: `FORK_CHANGES.md`

**Interfaces:**
- Produces, in `lp64_diagnostics.py`:
  - `KINDS = ('int-conversion', 'pointer-to-int-cast', 'int-to-pointer-cast')`
  - `syntax_command(entry: dict) -> list[str] | None`: turns one
    `compile_commands.json` entry for a C file into a 64-bit syntax-only
    command; `None` for non-C entries.
  - `parse(stderr: str, root: Path) -> set[tuple[str, int, int, str]]`:
    `(repo-relative file, line, column, kind)`, where kind is one of `KINDS`
    or `'error'`.
  - `summarize(found: set, compiler: str) -> dict` with keys `compiler`,
    `total`, `by_kind`, `by_file`.
  - `regressions(baseline: dict, current: dict) -> list[str]`
  - CLI: `count`, `check` and `update`, each taking
    `--compile-commands PATH` (default
    `build/lp64/diag/compile_commands.json`).
- `syntax_command` rules: remove `-m32`, `-msse2`, `-mfpmath=sse`, `-w`, `-c`,
  `-o FILE`, `-MD`, `-MT X`, `-MF X` and every `-fsanitize...`; append
  `-fsyntax-only -Wint-conversion -Wpointer-to-int-cast -Wint-to-pointer-cast -fmax-errors=0`.
- `compiler` is the first line of `gcc --version`'s major version, as
  `gcc-13`.

- [ ] **Step 1: Write the failing tests**

```python
SAMPLE = (
    "/w/src/game/front.c:1124:13: warning: passing argument 1 of 'f' makes pointer from integer without a cast [-Wint-conversion]\n"
    "/w/src/game/front.c:1124:13: warning: passing argument 1 of 'f' makes pointer from integer without a cast [-Wint-conversion]\n"
    "/w/src/game/chr.c:1700:29: warning: cast from pointer to integer of different size [-Wpointer-to-int-cast]\n"
    "/w/src/game/chr.c:9:1: warning: unused variable 'x' [-Wunused-variable]\n"
    "/w/src/crash.c:748:11: error: array initializer must be an initializer list\n"
    "In file included from /w/src/game/chr.c:3:\n")

def test_parse_keeps_tracked_kinds_and_errors_once_each(self):
    found = diag.parse(SAMPLE, Path('/w'))
    self.assertEqual(found, {
        ('src/game/front.c', 1124, 13, 'int-conversion'),
        ('src/game/chr.c', 1700, 29, 'pointer-to-int-cast'),
        ('src/crash.c', 748, 11, 'error')})

def test_syntax_command_drops_32_bit_and_output_flags(self):
    entry = dict(file='/w/src/a.c', command='gcc -m32 -msse2 -mfpmath=sse -w -DX -I/w/include -o a.o -c /w/src/a.c')
    argv = diag.syntax_command(entry)
    for dropped in ('-m32', '-msse2', '-mfpmath=sse', '-w', '-c', '-o', 'a.o'):
        self.assertNotIn(dropped, argv)
    for kept in ('-DX', '-I/w/include', '/w/src/a.c', '-fsyntax-only', '-Wint-conversion'):
        self.assertIn(kept, argv)

def test_syntax_command_skips_cxx(self):
    self.assertIsNone(diag.syntax_command(dict(file='/w/port/fast3d/gfx_pc.cpp', command='g++ -c x')))

def test_a_file_rising_is_a_regression_even_if_the_total_falls(self):
    baseline = dict(compiler='gcc-13', total=10, by_file={'a.c': 2, 'b.c': 8}, by_kind={})
    current = dict(compiler='gcc-13', total=9, by_file={'a.c': 3, 'b.c': 6}, by_kind={})
    self.assertEqual(len(diag.regressions(baseline, current)), 1)
    self.assertIn('a.c', diag.regressions(baseline, current)[0])

def test_a_new_file_with_diagnostics_is_a_regression(self):
    baseline = dict(compiler='gcc-13', total=1, by_file={'a.c': 1}, by_kind={})
    current = dict(compiler='gcc-13', total=2, by_file={'a.c': 1, 'new.c': 1}, by_kind={})
    self.assertIn('new.c', diag.regressions(baseline, current)[0])

def test_a_different_compiler_is_refused_not_compared(self):          # Review Focus 4
    baseline = dict(compiler='gcc-13', total=1, by_file={'a.c': 1}, by_kind={})
    current = dict(compiler='gcc-14', total=1, by_file={'a.c': 1}, by_kind={})
    message = diag.regressions(baseline, current)
    self.assertEqual(len(message), 1)
    self.assertIn('gcc-13', message[0]); self.assertIn('gcc-14', message[0])

def test_fewer_diagnostics_is_not_a_regression(self):
    baseline = dict(compiler='gcc-13', total=5, by_file={'a.c': 5}, by_kind={})
    current = dict(compiler='gcc-13', total=1, by_file={'a.c': 1}, by_kind={})
    self.assertEqual(diag.regressions(baseline, current), [])
```

- [ ] **Step 2: Run and confirm failure**

Run: `python3 scripts/port/test_lp64_diagnostics.py`
Expected: FAIL, `FileNotFoundError` for `lp64_diagnostics.py`.

- [ ] **Step 3: Implement `lp64_diagnostics.py`**

`count` runs every command from `syntax_command` (in parallel, each with a
60-second timeout) and prints the summary as JSON. `check` exits 1 printing
each line of `regressions`; when the total has fallen it exits 0 and prints
`Baseline can be lowered: run update.` `update` rewrites the baseline file,
sorted, with a trailing newline. A compile command that times out or cannot
start is an error, not a zero.

- [ ] **Step 4: Run and confirm the tests pass**

Run: `python3 scripts/port/test_lp64_diagnostics.py`
Expected: `OK`

- [ ] **Step 5: Record the baseline**

Until Task 5b repairs the headless configuration, one file cannot find
`SDL.h`, which would enter the baseline as an error. Add the flag Task 2
found to work (the shim form is shown) so the baseline describes the code and
not the missing header. `syntax_command` keeps `-I` flags, so no code changes.

```bash
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c 'cmake -S . -B build/lp64/diag -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS=-I/home/dev/project/scripts/port/sdl_shim && python3 scripts/port/lp64_diagnostics.py update && python3 scripts/port/lp64_diagnostics.py check'
```

Expected: a baseline with a `total` in the low thousands and `check` exiting
0. The planning measurement, made with a different compiler over game and
engine files only, was about 1,600 places in 70 files; a figure far from that
needs an explanation in the commit body. Run the same `check` with
`--arch aarch64` and record in the commit body whether its count matches.

- [ ] **Step 6: Commit**

```bash
git add scripts/port/lp64_diagnostics.py scripts/port/test_lp64_diagnostics.py \
  scripts/port/lp64_diagnostics_baseline.json FORK_CHANGES.md
git commit -m "Count 64-bit pointer diagnostics and refuse any rise"
git push origin main
```

---

### Task 5: Unchanged-32-bit comparison

**Files:**
- Create: `scripts/port/compare_win32.py`, `scripts/port/test_compare_win32.py`
- Modify: `scripts/port/native_64bit_plan.md` (status notes), `FORK_CHANGES.md`

**Interfaces:**
- Produces, in `compare_win32.py`:
  - `pe_sections(data: bytes) -> dict[str, bytes]`: section name to its raw
    bytes. `ValueError` on anything that is not a PE file.
  - `loaded(sections: dict[str, bytes]) -> dict[str, bytes]`: drops every
    section whose name starts with `.debug` or `/` (long-name debug sections)
    and keeps every other section.
  - `differences(a: bytes, b: bytes) -> list[str]`: empty when the loaded
    sections of both files are byte-identical; otherwise one line per
    differing, missing or extra section.
  - `export_source(ref: str, destination: Path, root=ROOT) -> None`: writes
    the tracked files of `ref` into `destination`. The special ref
    `WORKTREE` means the tracked files as they are on disk now, uncommitted
    edits included.
  - CLI: `compare EXE_A EXE_B` (exit 0 identical, 1 different) and
    `commits REF_A REF_B` (builds both, then compares). Either ref may be
    `WORKTREE`.

- [ ] **Step 1: Write the failing tests**

Build synthetic PE files in the test with `struct.pack` (DOS header with
`e_lfanew`, `PE\0\0`, a COFF header carrying a `TimeDateStamp`, an optional
header, and a section table). No real executable is used.

```python
def test_identical_files_have_no_differences(self):
    self.assertEqual(compare.differences(pe(text=b'\x90'*64), pe(text=b'\x90'*64)), [])

def test_timestamp_alone_is_not_a_difference(self):                  # Review Focus 5
    self.assertEqual(compare.differences(pe(stamp=1), pe(stamp=2)), [])

def test_debug_sections_alone_are_not_a_difference(self):            # Review Focus 5
    a = pe(extra={'.debug_info': b'a'*32, '/4': b'x'*8})
    b = pe(extra={'.debug_info': b'b'*48, '/4': b'y'*8})
    self.assertEqual(compare.differences(a, b), [])

def test_one_byte_of_code_is_a_difference(self):                     # Review Focus 5
    result = compare.differences(pe(text=b'\x90'*64), pe(text=b'\x90'*63+b'\xcc'))
    self.assertEqual(len(result), 1)
    self.assertIn('.text', result[0])

def test_a_missing_data_section_is_a_difference(self):
    result = compare.differences(pe(extra={'.rdata': b'r'*16}), pe())
    self.assertIn('.rdata', result[0])

def test_non_pe_input_is_rejected(self):
    with self.assertRaises(ValueError):
        compare.pe_sections(b'\x7fELF' + b'\0'*60)

def test_worktree_export_includes_uncommitted_edits_and_skips_untracked(self):
    # Arrange: a temporary repository with a.c committed as "one", then
    # a.c rewritten to "two" (unstaged) and an untracked b.c.
    compare.export_source('WORKTREE', destination, root)
    self.assertEqual((destination/'a.c').read_text(), 'two')
    self.assertFalse((destination/'b.c').exists())

def test_commit_export_ignores_uncommitted_edits(self):
    compare.export_source('HEAD', destination, root)
    self.assertEqual((destination/'a.c').read_text(), 'one')
```

- [ ] **Step 2: Run and confirm failure**

Run: `python3 scripts/port/test_compare_win32.py`
Expected: FAIL, `FileNotFoundError` for `compare_win32.py`.

- [ ] **Step 3: Implement `compare_win32.py`**

`commits` exports each ref with `git archive` into
`build/lp64/compare/a` and `build/lp64/compare/b`, and builds each in the
`goldeneye` image **mounted at the same container path**
(`/home/dev/project`, with `--platform linux/amd64` as Task 2's runner uses),
one after the other, with the README's Windows
configuration (`-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DSDL2_MINGW_ROOT=$HOME/sdl2-mingw/SDL2-2.30.11/i686-w64-mingw32`).
The same path matters because `__FILE__` and assertion text embed it.
`scripts/port/bench/stock.py` already builds an exported commit this way;
follow its `export_source` and `container` functions.

- [ ] **Step 4: Run and confirm the tests pass**

Run: `python3 scripts/port/test_compare_win32.py`
Expected: `OK`

- [ ] **Step 5: Find out whether the build is reproducible**

```bash
python3 scripts/port/compare_win32.py commits HEAD HEAD
```

Expected: exit 0. If sections differ, the cause is something the build embeds
that changes between two builds of one commit: start with how the About page
gets its build time and commit (`port/src/port_about.c`,
`scripts/port/test_build_time.sh`). Handle it in the tool, by giving both
builds the same stamp input if the build accepts one, or by naming the stamp's
bytes and excluding exactly those. Do not change game or build source in this
task. If it cannot be made to pass without such a change, stop and report: the
spec says this comparison "reports back if not".

- [ ] **Step 6: Prove it detects a real change**

Compare the commits on either side of FORK-088's renderer change:

```bash
python3 scripts/port/compare_win32.py commits 1e3df59 d76b6cf
```

Expected: exit 1, with `.text` among the differing sections.

- [ ] **Step 7: Commit**

Record both results and the build time per comparison in the status table.

```bash
git add scripts/port/compare_win32.py scripts/port/test_compare_win32.py \
  scripts/port/native_64bit_plan.md FORK_CHANGES.md
git commit -m "Compare the 32-bit Windows executable between two commits"
git push origin main
```

---

### Task 5b: Repair the headless build

**Requires Task 1 on `main` and `midas.galleryDefer` set on the Mac.**
Added on 2026-11-15, after Task 2 found the stock headless configuration
broken. The 64-bit plan uses the headless i686 build as its reference, so it
has to build without special flags.

**Files:**
- Modify: `CMakeLists.txt` (the `GE_HEADLESS` block near line 338)
- Create: `scripts/port/test_headless_configure.sh`
- Modify: `FORK_CHANGES.md` (FORK-089, and FORK-082 where it describes Baked
  Lighting's build)

**Interfaces:**
- Produces: `cmake -S . -B DIR -DGE_HEADLESS=ON` followed by a build
  succeeds in the i686 container with no extra flags.
- Fixed behaviour: the change is confined to `GE_HEADLESS` builds. A
  `GE_VIDEO=ON` build's compile and link commands do not change.

- [ ] **Step 1: Write the failing test, `test_headless_configure.sh`**

It configures a headless build into a temporary directory with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, then compiles only
`port/src/port_baked_trace.c` using its recorded command, and asserts the
compile succeeds. Compiling one file keeps the test fast enough for CI.

- [ ] **Step 2: Run and confirm failure**

Run: `python3 scripts/port/lp64_env.py run --arch i686 -- bash scripts/port/test_headless_configure.sh`
Expected: FAIL with `fatal error: SDL.h: No such file or directory`.

- [ ] **Step 3: Repair it in `CMakeLists.txt`**

Use the mechanism Task 2 proved (its report and FORK-089 say which). If the
shim worked: under `GE_HEADLESS`, give `port/src/port_baked_trace.c` the
include directory `scripts/port/sdl_shim`, so the bake runs in its
time-sliced, thread-free path. Do not edit `port_baked_trace.c`.

- [ ] **Step 4: Run and confirm the test passes, then build and run in full**

```bash
python3 scripts/port/lp64_env.py run --arch i686 -- bash scripts/port/test_headless_configure.sh
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c 'cmake -S . -B build/lp64/ref-i686-stock -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON && cmake --build build/lp64/ref-i686-stock -j"$(nproc)"'
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ref-i686-stock/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 2 --output build/lp64/ref-i686-stock/smoke.json
```

Expected: the test passes, the build links, and the smoke run matches the
result Task 2 recorded with the flag.

- [ ] **Step 5: Prove the shipped build is untouched**

```bash
python3 scripts/port/compare_win32.py commits HEAD WORKTREE
```

Expected: exit 0.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt scripts/port/test_headless_configure.sh FORK_CHANGES.md
git commit -m "Make the headless build compile again without SDL"
git push origin main
```

---

### Task 6: Pointer-width build option

**Requires Task 1 on `main` and `midas.galleryDefer` set on the Mac.**

**Files:**
- Modify: `CMakeLists.txt`
- Create: `scripts/port/test_pointer_bits.sh`
- Modify: `FORK_CHANGES.md`

**Interfaces:**
- Produces: the CMake cache variable `GE_POINTER_BITS`, a string, `32` or
  `64`, default `32`. Code never reads it; code reads `IS_64_BIT`.
- Fixed behaviour:
  - Any other value: `FATAL_ERROR "GE_POINTER_BITS must be 32 or 64"`.
  - `64` with Emscripten:
    `FATAL_ERROR "The web build is 32-bit; GE_POINTER_BITS=64 is not supported"`.
  - `64` with `WIN32`:
    `FATAL_ERROR "Windows x64 is outside the 64-bit plan; see scripts/port/native_64bit_plan.md"`.
  - `64` with `GE_ROMDATA_FALLBACK`:
    `FATAL_ERROR "GE_POINTER_BITS=64 requires runtime ROM loading (GE_ROMDATA_FALLBACK=OFF)"`.
  - `32`: every flag exactly as today.
  - `64`: no `-m32`, `-msse2`, `-mfpmath=sse` or `-no-pie`; add
    `-ffp-contract=off`; no `_FILE_OFFSET_BITS`/`_TIME_BITS` definitions; the
    i386 library directory is not used.

- [ ] **Step 1: Capture today's 32-bit commands, before any edit**

```bash
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c 'cmake -S . -B build/lp64/bits-before -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null && sed "s#build/lp64/bits-before#BUILD#g" build/lp64/bits-before/compile_commands.json > build/lp64/bits-before.json'
```

- [ ] **Step 2: Write the failing test, `test_pointer_bits.sh`**

It configures into temporary directories and asserts:

1. Default configure succeeds and its `compile_commands.json` contains `-m32`.
2. `-DGE_POINTER_BITS=64 -DGE_HEADLESS=ON` configures, and its
   `compile_commands.json` contains none of `-m32`, `-msse2`, `-mfpmath=sse`,
   and does contain `-ffp-contract=off`.
3. `-DGE_POINTER_BITS=48` fails, and the output contains
   `GE_POINTER_BITS must be 32 or 64`.
4. `-DGE_POINTER_BITS=64 -DGE_ROMDATA_FALLBACK=ON` fails with the message
   above.
5. `-DGE_POINTER_BITS=64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake` fails
   with the Windows message. Skip this case, saying so, when
   `i686-w64-mingw32-gcc` is not installed.

Only configuring is tested. A 64-bit compile is not expected to succeed until
LP64-002.

- [ ] **Step 3: Run and confirm failure**

Run: `python3 scripts/port/lp64_env.py run --arch x86_64 -- bash scripts/port/test_pointer_bits.sh`
Expected: FAIL at case 2 (the 64-bit configure still contains `-m32`).

- [ ] **Step 4: Add the option to `CMakeLists.txt`**

Keep the edit to the existing flag blocks (near lines 10 to 60, the i686
definitions near 452 and 476, and the Linux SDL link near 559). Do not touch
the asset-object pipeline: it is fallback-only, and fallback is 32-bit only.
Update the file's opening comment to say the build is 32-bit by default and
64-bit by option.

- [ ] **Step 5: Run and confirm the tests pass**

Run the Step 3 command. Expected: all five cases pass or skip as described.

- [ ] **Step 6: Prove the default build is untouched**

```bash
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c 'cmake -S . -B build/lp64/bits-after -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null && sed "s#build/lp64/bits-after#BUILD#g" build/lp64/bits-after/compile_commands.json | diff - build/lp64/bits-before.json && echo commands-identical'
python3 scripts/port/compare_win32.py commits HEAD WORKTREE
```

Expected: `commands-identical`, and the comparison exits 0, which shows the
edit left the 32-bit Windows executable unchanged.

- [ ] **Step 7: Commit**

The hook reports the gallery as pending. That is the deferred tier working.

```bash
git add CMakeLists.txt scripts/port/test_pointer_bits.sh FORK_CHANGES.md
git commit -m "Add GE_POINTER_BITS, defaulting to today's 32-bit build"
git push origin main
```

---

### Task 7: CI and milestone evidence

**Requires Task 1 on `main`.**

**Files:**
- Modify: `.github/workflows/windows-release.yml`,
  `scripts/port/native_64bit_plan.md`, `FORK_CHANGES.md`

**Interfaces:**
- Consumes: every test and tool from Tasks 2 to 6.

- [ ] **Step 1: Add the checks to the ROM-free test step**

In the step named `Test gameplay, cache and release tooling without a ROM`,
after `python3 scripts/port/test_hand_gallery.py`, add:

```yaml
          python3 scripts/port/test_lp64_env.py
          bash scripts/port/test_abi_probe.sh
          python3 scripts/port/test_lp64_diagnostics.py
          python3 scripts/port/test_compare_win32.py
          bash scripts/port/test_pointer_bits.sh
          bash scripts/port/test_headless_configure.sh
          cmake -S . -B build/lp64/diag -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
          python3 scripts/port/lp64_diagnostics.py check
```

- [ ] **Step 2: Run the same lines locally, in the x86_64 container**

Run each added line through
`python3 scripts/port/lp64_env.py run --arch x86_64 -- ...`.
Expected: every one exits 0. The runner uses GCC 13 on Ubuntu 24.04, the same
as the container, so the diagnostics baseline applies to both.

- [ ] **Step 3: Record the milestone's evidence**

In `native_64bit_plan.md`, set LP64-001's row from the facts gathered: which
commits, which commands, their results, the emulation timings, whether the
Windows comparison is reproducible, and the date of the last full gallery
regeneration. Mark it done only if every task above is done; otherwise state
what is open. Extend FORK-089 to match.

- [ ] **Step 4: Commit, push and watch CI**

```bash
git add .github/workflows/windows-release.yml scripts/port/native_64bit_plan.md FORK_CHANGES.md
git commit -m "Run the 64-bit scaffolding checks in CI"
git push origin main
```

Then confirm the pushed commit's `windows-release` run passes its test job.
A failure there is this task's to fix, not a follow-up.

---

## What this plan does not do

- It builds no 64-bit game. `GE_POINTER_BITS=64` configures and does not yet
  compile; LP64-002 gets it compiling.
- It adds no file layouts or converters. The probe is ready to receive their
  assertions in LP64-004.
- It does not make the compile gate an error. Task 4's ratchet only stops the
  count rising.
