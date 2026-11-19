# Native 64-bit port plan

Iron Sights builds only as a 32-bit program today (Windows i686, Linux i686 and
32-bit WebAssembly). This plan makes the same source also build and run with
64-bit pointers, starting with Linux x86_64. It is the first of three projects:
this one, then a native macOS build, then an Apple TV build. macOS and Apple TV
get their own plans once this one has a playable 64-bit build.

The owner accepted this plan on 2026-11-15. Acceptance is not evidence that any
milestone works; the [milestone status](#milestone-status) table records what
has been built and verified.

## Why

- The owner wants the game on their own Apple TV, and buildable by other
  developers for theirs. Apple TV, and macOS on Apple Silicon, run only 64-bit
  ARM code: the tvOS 27.2 SDK has `arm64` and `arm64e` slices and nothing else.
- The owner also wants native macOS and 64-bit Linux builds. All three need the
  same engine change, so it is done once, on the platform where it can be
  compared with the 32-bit build.
- A 64-bit Linux build removes the 32-bit SDL, Mesa and multilib requirements
  that make today's Linux, Steam Deck and sanitizer builds awkward.

## Goals

1. One source tree builds 32-bit and 64-bit. The 32-bit Windows, Linux and web
   builds keep working and keep shipping throughout.
2. A Linux x86_64 build that plays the whole game: every solo stage,
   multiplayer, the menus, the watch, and the fork's own features.
3. Evidence, not impressions, that the 64-bit build behaves like the 32-bit
   build: same simulation, same picture, same sound, on the same machine.
4. Gates that stop new code from reintroducing 32-bit pointer assumptions.
5. Nothing in the result that only works on x86, so macOS on ARM follows
   without redoing this work.

## Non-goals

- macOS, Apple TV, iOS and Windows x64 builds. This plan must not block them,
  and builds none of them.
- Suspend points (save states) on 64-bit builds. See
  [Suspend points](#suspend-points).
- Changing how the 32-bit builds load ROM data. They are the reference the
  64-bit build is compared with.
- 64-bit WebAssembly.
- Adopting upstream's open 64-bit pull requests. See [Approaches](#approaches).

## Anti-goals

These would make the project a failure even if a 64-bit build runs:

- A 64-bit build that differs from the 32-bit build in ways nobody measured.
- Any regression in the shipping 32-bit or web builds.
- ROM-derived data committed to Git, including test fixtures.
- Rewrites of upstream-owned game files beyond what pointer width requires.
  Type and size corrections belong there; new logic belongs in `port/`.

## Constraints

- **ROM at runtime.** Player builds load every asset from the user's ROM
  (`GE_ROMDATA_FALLBACK=OFF`). The compiled-in asset pipeline is a 32-bit-only
  fallback and stays that way.
- **Renderer formats are fixed.** `Vtx` (16 bytes), `Mtx` (64), `Vp` (16) and
  the light structs keep their N64 sizes at both widths, because the renderer
  reads them as such.
- **Upstream merges continue.** The fork merges upstream releases, so edits to
  upstream-owned files must stay small, mechanical and recorded in
  `FORK_CHANGES.md` (see `AGENTS.md`, "Repo-Local Guidance").
- **No ROM in CI.** Checks that need a ROM run locally; CI runs the ROM-free
  ones at both widths.
- **Float behaviour.** The game relies on single-precision arithmetic matching
  the N64. x86_64 uses SSE by default; ARM builds will need
  `-ffp-contract=off`, as the web build already uses.
- **Hands and arms.** The first-person arm contracts
  (`scripts/port/arm_rendering.md`) and the review gallery apply unchanged.

## What was measured

- A syntax-only 64-bit compile of the 161 game and engine C files reported
  about 1,600 distinct places in 70 files that mix pointers with 32-bit
  integers, and 40 hard errors. Most trace back to a few declarations: about
  460 to font functions and variables that carry font pointers as `s32`, about
  190 to animation table entries that start as integers and become pointers.
  The port layer and renderer were not part of that compile.
- A 64-bit ARM program on macOS 27.2 cannot map memory below 4 GB: `mmap`
  there fails, and a program linked with a smaller zero page is killed at
  launch. So no design may depend on low addresses.
- Nearly every ROM asset class is converted in place today, overwriting 4-byte
  offsets with 4-byte pointers: text banks, setup files, collision tiles,
  models, backgrounds, animations, fonts, audio banks and music sequences.
- Every ROM read passes its address through a `u32` (`osPiStartDma`'s
  `devAddr`), and `osVirtualToPhysical` returns `u32` at 133 call sites.
- The renderer steps through display lists by `sizeof(Gfx)` and has no
  hard-coded 8-byte stride. The 8-byte assumptions are in game code
  (`src/game/tex.c`, `bg.c`, `propobj.c`) and `port/src/romfile_model.c`.

## Approaches

**How pointers are represented.** Chosen by the owner: widen.

| Approach | Outcome |
|---|---|
| Widen: 8-byte pointers everywhere, 16-byte display-list commands, ROM data converted to the wider layout at load | **Chosen.** Ordinary C, so debuggers and sanitizers work; the renderer already declares command words pointer-sized; proven by the Perfect Dark port, the same engine family. Costs a converter per asset class, larger memory budgets and a new suspend-point design. |
| Keep 32-bit addresses as offsets into one reserved block, converted at each use | Rejected. Needs a wrapper at every access across upstream-owned game code, and data in the executable sits outside the block. |
| Map all game memory below 4 GB so existing casts keep working | Ruled out by the measurement above. |
| Translate the WebAssembly build to C and compile that for 64-bit | Set aside. It avoids the port but yields no native macOS or Linux build and an unmeasured speed cost. |

**Where the code comes from.** Chosen by the owner: write our own.

| Approach | Outcome |
|---|---|
| Write our own, following the Perfect Dark port's mechanisms | **Chosen.** Full control over structure and verification. |
| Start from upstream's open pull request (murkantor/Project-Midas#95, a rebase of #42) | Not taken. It uses the same widening approach on the same 1.0.2b baseline and has been played on Apple Silicon, but it is unreviewed, macOS only, and conflicts with this fork in 24 of its 200 files. |

Consequence to plan for: if upstream later merges its own 64-bit work, the next
upstream merge will conflict widely with ours. The owner accepted that by
choosing this route. Keeping our edits to upstream files mechanical limits the
damage.

Reading other ports for how they solved a problem is expected. Copying code
from any of them needs a licence check first, and an entry in
`tools/THIRD_PARTY_NOTICES.txt`.

## Design

### Build

- An explicit CMake option selects the pointer width. It defaults to 32 on
  every existing route, so no current build changes unless asked. Code never
  tests the option; it tests `IS_64_BIT` (`include/platform_info.h`).
- `-m32`, the i686 library paths, the `_` symbol prefix and the fixed image
  base become conditional on the 32-bit choice.
- 64-bit Linux links as a position-independent executable, the platform
  default. The 32-bit ELF build keeps `-no-pie`.
- The Linux containers gain x86_64 dependency sets beside the i386 ones, so one
  container builds and runs both widths. That container is the comparison host.
- `GE_ROMDATA_FALLBACK=ON` with 64-bit is a configure error.

### Addresses

- Host pointers are plain 64-bit values. The fixed arena addresses
  (`0x20000000` and friends in `port/src/pcsched.c`) apply to 32-bit only.
- **Telling a host pointer from an N64 segment word.** Today the test is
  "at least `0x10000000`" (`port/include/port_hostptr.h`). On 64-bit the test
  becomes "any of the upper 32 bits set". Segment words and file offsets always
  fit in 32 bits, so the two cannot collide provided no game data lives below
  4 GB. The 64-bit build checks that at startup (executable image, arenas, ROM
  image) and stops with a clear message if it does not hold.
  - This holds by construction on Apple's ARM platforms and for
    position-independent Linux executables.
  - Tools that load programs low (Valgrind, for one) will trip the check. That
    is accepted; the sanitizers do not.
  - The Perfect Dark port instead tags every segment word's low bit. That was
    considered and not chosen, because it changes every site that creates a
    segment word in upstream game code.
- Everything that carries an address in 32 bits widens to `uintptr_t` or
  `intptr_t`: `osPiStartDma`, `romCopy`, `osVirtualToPhysical` and the
  `OS_K0_TO_PHYSICAL` family, `memp` and `mema` bookkeeping, and the globals
  listed in the [hazard inventory](#hazard-inventory). Signedness is chosen per
  field, since it changed behaviour in the Perfect Dark port.
- Audio command words (`Acmd`, `include/PR/abi.h`) are 32-bit and can carry
  buffer addresses. They get the same treatment as display-list words when the
  audio class is converted.

### Game code types

- Fix declarations, not call sites. A field, parameter or global that carries
  a pointer gets a pointer type or `intptr_t`/`uintptr_t`. Casts are the last
  resort.
- Hard-coded sizes become `sizeof`: the `Model` allocation (`0x20`), the
  `rwdata` slot size (`numRecords*4`), the prop record word counts in
  `sizepropdef`, node and record sizes in the model converter.
- Sentinel tests that compare a pointer-or-number field with a small constant
  (`TextureID < MAX_TEXTURES`, `< PORTMAX`, `address & 0x80000000`, menu text
  IDs against function pointers) keep working only if the field is
  pointer-sized and the comparison is unsigned. Each one is listed and given a
  test.
- Unions of a pointer and an integer are reviewed one by one; widening changes
  which members overlap.
- The game's `Vertex` has a pointer inside a 16-byte renderer format. It stays
  16 bytes: the link becomes a 32-bit offset from a documented base, resolved
  where it is used.

### ROM data: file layouts and converters

- Each ROM-loaded format gets a **file layout**: structs with fixed-width
  fields that match the ROM bytes at any pointer width, with static assertions
  on their sizes. The game's runtime structs (`src/bondtypes.h` and friends)
  keep real pointers and grow on 64-bit.
- Each asset class gets a **converter** in `port/src/` that reads the file
  layout and writes the runtime layout into a new buffer, doing the byte swap
  in the same pass. Where the game later turns offsets into pointers itself,
  the converter emits those offsets into pointer-sized slots so the game's own
  step runs unchanged.
- Converters are written to be correct at either width. On 64-bit they are the
  only path. On 32-bit the existing in-place path stays in use, and the
  converter runs only under test (see the converter oracle below).
- Destination sizes are never guessed silently. A converter either measures
  first and allocates exactly, or allocates from a stated bound and fails
  loudly on overflow. The choice per class is the builder's.
- Classes, in the order the game needs them: text banks; fonts; texture
  directory and image bank; animation tables; models; collision tiles; setup
  files; backgrounds (front matter, portals, room data); audio banks; music
  sequences. Briefings, demos, fog tables and AI command bytes carry no
  pointers and need only confirming.

### Display lists

- `Gfx` is 16 bytes on 64-bit, with the N64 command word in the low 32 bits of
  the first word. That is what `include/PR/gbi.h` already declares.
- Every display list the renderer walks must be in host form. ROM lists (model
  nodes, room geometry, the font list) are expanded from 8-byte big-endian
  commands at load. Addresses inside a file are remapped through an old-to-new
  offset table, because the data around them moves when structs grow.
- Model files reference sub-lists and vertices by segment offset into the file.
  The model converter expands every list in the file and rewrites those
  offsets, so no 8-byte list survives.
- Game and port code that walks or patches lists uses the command struct's
  words and `sizeof(Gfx)`, never raw `u32` pairs, `>> 3` or literal 8.
- Runtime list buffers and their per-stage budgets grow to match.

### Memory budgets

- Per-stage budgets (`memallocstringtable` in `src/boss.c`, already doubled on
  PC) and the fixed pools get a 64-bit scale where their contents grow: room
  display lists, the graphics buffer, model and setup allocations, texture
  pools, the permanent bank, the music heap.
- Exhausting any pool on 64-bit fails loudly with the pool's name. Final sizes
  come from a soak run over every stage, with headroom, not from estimates.

### Suspend points

Suspend points copy raw memory back to the same addresses and verify the
executable's hash. 64-bit processes load at a different address on each run,
so that design cannot work there. 64-bit builds report suspend points as
unavailable, as the web build does, and the menus say so. A relocating design
is a separate future project. Campaign saves (`Data/Saves/eeprom.bin`) contain
no pointers and work unchanged; the plan verifies that a save moves between
widths.

### Crash reports and the rest of the port layer

- The x86 frame-pointer walks (`port/src/main.c`, `port_profile.c`) get a
  portable replacement on 64-bit. Sentry is built for x86_64.
- External textures stop being refused on 64-bit once their address slot is
  pointer-sized (`port/src/port_textures.c`).
- The 109 source files this fork has added are covered by the same gates as
  upstream code. Their ROM-free tests run at both widths.

## Verification

Each item names what it proves. "Both builds" means the i686 and x86_64 Linux
builds of one commit, run in one container.

**What counts as measured.** Equivalence is claimed for this surface and no
wider: every solo stage and every attract demo; one four-player route (the
bench suite's `temple-4p`); a scripted walk through the menus and the watch;
each of those under two settings profiles, the defaults and one with every fork
feature switched on. The fork's Windows-only PowerShell audits are not ported
to Linux; the fork's ROM-free tests run at both widths instead (item 10).

**Rules for the comparison runs.**

- *The reference must agree with itself first.* A trace, screenshot or
  recording is evidence only if two runs of the 32-bit build produce the same
  result. `FORK_CHANGES.md` records that hand-recorded input once drifted on
  replay of the 32-bit build, so generated routes and the attract demos are
  preferred, and each source is checked for repeatability before it is used.
- *Differences from outside the port are removed in the test, not in the
  shipped game.* The comparison builds share a test-only option that links one
  in-tree `sinf`/`cosf` implementation at both widths, so C library rounding
  cannot cause a divergence. Shipping builds keep their current maths. If a
  divergence still needs a change to a shipping build, that goes to the owner
  with the evidence.
- *A difference is "explained" only when* its first differing tick or frame is
  identified, its cause is named, and the runs match again with that cause
  removed. Anything less stays open against LP64-006.

1. **Compile gate.** The 64-bit build compiles with pointer/integer conversion
   warnings as errors, across game, engine, port and renderer. CI builds both
   widths. Proves: no conversion the compiler can see remains, and none
   returns.
2. **Layout probe.** A native test asserts the size and key offsets of every
   file layout and renderer format at both widths, modelled on
   `scripts/port/web_probe/probe.c`. Proves: file layouts do not drift.
3. **Unchanged 32-bit program.** The 32-bit Windows executable's loaded code
   and data are compared before and after each commit, ignoring the build
   stamp. Proves: the commit did not alter the shipping build, and shows which
   gallery-pending commits could have changed pixels at all. It does not gate
   the gallery. Where the 32-bit program legitimately changes, the commit says
   why. LP64-001 established that the Windows cross-build is reproducible
   enough for this comparison once `SOURCE_DATE_EPOCH` is fixed for both builds
   (`scripts/port/compare_win32.py`; FORK-089 has the runs).
4. **Converter oracle.** On a 32-bit build, each converter's output is compared
   with what the in-place path produces for the same file, for every file of
   that class in the ROM. On 32-bit the two layouts are the same, so they must
   match byte for byte apart from the base address. Proves: the converter
   reads the format correctly, before it is trusted on 64-bit. Runs locally
   with a ROM; small hand-written fixtures cover the same converters in CI.
5. **Simulation equivalence.** Both builds replay the ROM's attract demos with
   per-tick tracing (`GE_DEMODIAG`, `GE_RNGTRACE`, `GE_DEMOSTRICT`) and replay
   input recordings (`GE_INPUTPLAY`) across every stage, headless, with fixed
   seed and tick. The traces are diffed. Proves: the simulation is the same.
6. **Picture equivalence.** The primary check does not depend on a rasterizer:
   both builds write a per-frame digest of what the renderer submits (vertex
   data, texture uploads, draw state), and the digests are diffed. The digest
   must be obtainable without drawing pixels, so the emulated i686 reference
   stays practical. Proves: the
   two builds ask for the same picture. Screenshots per stage (`GE_SHOT`) and
   the hand gallery, rendered by both builds with the same software renderer,
   are the secondary check; 32-bit and 64-bit builds of that renderer may round
   differently, so a screenshot difference with matching digests is recorded as
   the rasterizer's, and one with differing digests is a port bug.
7. **Sound equivalence.** Both builds record a fixed route with
   `GE_AUDIO_CAPTURE`; the recordings are compared. Proves: bank and sequence
   conversion.
8. **Sanitizers.** The 64-bit build runs the per-mission smoke
   (`scripts/port/audit_buffers.py --mission-smoke`) with video under
   AddressSanitizer and UndefinedBehaviorSanitizer. Proves: no overruns from
   grown structs. The Perfect Dark port found latent original bugs this way.
9. **Soak.** Every stage, several laps, with memory statistics. Proves: the
   budgets hold.
10. **Existing ROM-free tests.** The `scripts/port/test_*.sh` harnesses that
    compile game code with `-m32` also run as 64-bit.
11. **Saves.** A campaign save written by one width loads in the other.
12. **ARM check.** A Linux aarch64 build is built from LP64-001 onward, since
    it is the native one on the working machine. It runs the simulation traces
    of item 5 and is compared with x86_64. It is not packaged or shipped.
    Proves: goal 5, before the macOS project depends on it.

## Working machine

The owner decided on 2026-11-15 that this work runs on their Apple Silicon Mac.
The US ROM is in that checkout's root, ignored by Git (`*.[zZnN]64`), and its
SHA-1 matches `ge007.u.sha1`. Tools find it through `GE_ROM`; it is never
copied into a tracked path.

What that machine can and cannot do:

- **Linux aarch64 containers run natively.** That is the fast loop: the
  aarch64 build, the sanitizers and the soak run there.
- **Linux x86_64 and i686 are not native here.** x86_64 is translated by Rosetta 2 at close to native speed (found 2026-11-19); i686 runs under qemu-i386 and is the
  slowest. The i686 reference is therefore used headless wherever possible.
- **A divergence seen only under emulation is not yet a finding.** It is
  confirmed on real x86 hardware before it is treated as a port bug or as
  explained.
- **The review gallery cannot be regenerated here.** It was rendered on the
  owner's Windows laptop (`review/hand-gallery/manifest.json` records an
  NVIDIA RTX 3070), and a regeneration needs the 32-bit game running on that
  graphics card. The gallery's fingerprint covers the game's source files.
  Under the two-tier rule below, a change to a strict-tier input still needs
  that laptop before it is committed; a deferred-tier change may leave the
  gallery pending, and the laptop clears it at the end of each milestone at the
  latest.

### Gallery rule: two tiers

The owner chose on 2026-11-15 to loosen the gallery requirement to two tiers.
This changes the gallery contract for the whole repository, not only for this
project, and was built in LP64-001 Task 1.

**Strict tier.** A commit that touches any of these still needs a fresh,
reviewed gallery, exactly as today:

- The fork's hand, arm and wardrobe code: `port/src/port_arm_mesh.c`,
  `port_native_arms.c`, `port_grip_mesh.c`, `port_wardrobe.c`, `ads_hands.c`,
  `port_knife_ads.c`, `port_graphics.c`, `port_shiny.c`.
- The game files the arm contracts name: `src/game/gunfire.c`,
  `src/game/model.c`, `src/game/bondview2.c`.
- The renderer: everything under `port/fast3d/`.
- The weapon icons: `port/src/port_weapon_images.c`, `port_hud_weapons.c`.
- The capture path and the tool: `port/src/joy_port.c`,
  `scripts/port/hand_gallery.py`, `scripts/port/hand_gallery_template.html`.

**Deferred tier.** Every other rendering input the fingerprint covers today,
including the rest of `src/`, `include/` and `port/`, and the build files. A
commit that touches only these may leave the gallery **pending**.

How it works:

- The manifest records two fingerprints, one over the strict tier and one over
  everything. A strict mismatch fails the hook and CI, as a stale gallery does
  today. A mismatch only in the full fingerprint is reported as pending and
  passes.
- Pending is opt-in per clone, through a local Git setting beside
  `midas.galleryPython`. The Mac's clone sets it. A clone without it, such as
  the laptop's, keeps today's behaviour: the hook regenerates on any staleness,
  which clears anything pending.
- CI accepts pending and says so in its log. It still fails on a strict
  mismatch.
- Pending is cleared on the laptop at the end of each milestone at the latest.
  The milestone status table records the last full regeneration.

What this costs, accepted by the owner: a deferred-tier commit that does alter
the hands is caught at the next regeneration, not at the commit, and may ship
in a development build first. Verification item 3 narrows the search: it shows
which pending commits changed the 32-bit program at all.

**Branch exception (owner decision, 2026-11-16).** As first built, the hook
refused a strict-tier change in a deferring clone on every branch, so the Mac
could not commit that batch at all. The owner chose to let a clone with
`midas.galleryDefer` set commit a strict-tier change on any branch other than
`main`: its hook still runs every other gallery check, then passes and prints
`Hand gallery is STALE on branch <name>: strict inputs changed. Regenerate on a machine that can render before this reaches main.` On `main`, on a detached HEAD and in a clone without the
setting the hook is unchanged, and the `verify` command and CI still fail on a
strict mismatch, so `main` and CI are exactly as strict as before. Built in
LP64-002 Task 2, the commit "Let a deferring clone commit strict-tier work on a branch".

What it means for this project: 64-bit commits to strict-tier files are
batched on a branch and reach `main` through a regeneration on the laptop. The
three game files in the strict tier hold about 200 of the 1,600 measured
places, so those batches are part of the plan, not an exception.

`AGENTS.md` ("Render review gallery"), `scripts/port/hand_gallery.md`,
`scripts/port/test_hand_gallery.py` and `FORK_CHANGES.md` change with the rule.

The pre-commit hook is installed in the Mac's clone with the defer setting on
(`hand_gallery.py install-hook`, `midas.galleryDefer=true`).

## Milestones

1. **LP64-001 Scaffolding.** The Mac's toolchain: containers that build i686,
   x86_64 and aarch64, the hook installed, the i686 reference running headless
   with the ROM. The two-tier gallery rule. The unchanged-32-bit comparison.
   Pointer-width option; layout probe; a script that counts 64-bit
   diagnostics, with CI failing if the count rises. No behaviour change.
2. **LP64-002 Type hygiene.** Declarations fixed until the compile gate reaches
   zero; the gate becomes an error. 32-bit code compared before and after.
3. **LP64-003 Addresses and memory.** Address plumbing widened, arenas
   unpinned on 64-bit, the host-pointer test and its startup check. Done when a
   headless 64-bit build starts and reaches its first asset load. *Sharpened
   by the owner on 2026-11-16:* a 64-bit build already reached that load and
   crashed in it when first run, so done means the headless 64-bit executable
   passes the startup check and stops with status 5 at a guard naming the
   first asset class without a converter (`animation tables`), plain and
   under the sanitizers, on aarch64 and x86_64.
4. **LP64-004 Converters and display lists.** One asset class at a time, each
   with its file layout, converter, oracle result and fixtures. Display-list
   expansion lands with the first class that needs it. *Settled by the owner
   on 2026-11-16 with [its plan](native_64bit_lp64_004.md):* the classes are
   converted in the order the boot reaches them (animation tables, text
   banks, audio banks, music sequences, textures, fonts, models, backgrounds,
   collision tiles, setup files), and done means that on `main` all ten are
   marked converted; the 64-bit headless executable runs the title for 600
   frames and the 20-mission smoke 20 of 20, plain and under the sanitizers
   (the plain runs in the aarch64 and the x86_64 container); the oracle
   reports no difference for any file of any class; the load census is equal
   at both widths; and the 32-bit smoke and Windows executable are unchanged
   or every changed function is explained.
5. **LP64-005 First playable.** Headless 64-bit replays every attract demo and
   loads every stage; then the same with video. *Settled by the owner on
   2026-11-17 with [its plan](native_64bit_lp64_005.md):* "every stage" is
   the 34 that have their files on the cart (the 20 missions, Cuba and the 13
   multiplayer arenas, in every scenario); the nine cut level ids, Citadel
   and solo loads of Temple and Complex stop by name. "With video" on the
   working machine is SDL's offscreen driver and Mesa's software renderer in
   the containers, a run judged by the pictures it drew; a GPU and real x86
   hardware stay with LP64-006 and LP64-007. Done means that on `main` the
   plan's play sets (`scripts/port/lp64_play.py`: demos, stages, stops,
   menus, tours, the attract loop, scripted input) pass on the plain 64-bit
   headless executable in the aarch64 and the x86_64 container and on the
   sanitizer executable with poisoned arenas in aarch64; the same sets pass
   with video on the plain and the sanitizer video executable in aarch64,
   and the demos with video in x86_64; the sound set passes and the
   synthesizer test passes at both widths in CI; every demo's per-tick trace
   is identical to the i686 build's or its first difference is classified;
   the list of records read through another type is reviewed and checked in
   CI; and the 32-bit smoke and Windows executable are unchanged or every
   changed function is explained.
6. **LP64-006 Equivalence.** Verification items 5 to 12 pass, or each
   difference is explained under the rules above. Budgets set from the soak.
7. **LP64-007 Shipping.** Linux x86_64 packages beside the i686 ones, a native
   Steam Deck package, Sentry and crash reports, CI jobs, player and developer
   documentation, the suspend-point notice.

Milestones 2 to 4 land on `main` in small commits. Each must leave the 32-bit
and web builds passing their existing checks. Commits to deferred-tier files go
straight to `main` from the Mac. Commits to strict-tier files are batched on a
branch and merged through a gallery regeneration on the laptop. The compile
gate of LP64-002 becomes an error only after the strict-tier batch it depends
on has landed.

## Decisions

Owner's decisions:

- Targets: the owner's Apple TV and self-building developers; macOS; Linux
  64-bit. No App Store distribution.
- Widen pointers rather than keep 32-bit addresses.
- Write our own rather than start from upstream's pull request.
- The work runs on the owner's Mac, with the ROM in that checkout (2026-11-15).
- The gallery requirement loosens to two tiers (2026-11-15).
- LP64-003 (2026-11-16, all nine recommendations of
  [its plan](native_64bit_lp64_003.md)): its done condition as sharpened under
  "Milestones"; a guard stops a 64-bit build at each asset class without a
  converter; CI gains two checks, against implicit narrowing of pointer-sized
  integers and against calls without a prototype to pointer-taking functions;
  `Vertex` becomes 16 bytes in that milestone; its strict-tier work, four arm
  files included, lands as one batch through one laptop trip; no low-address
  development mode; `Acmd`, the Op11 union, `SetupIntroCamera` and the bg env
  slot belong to LP64-004; the suspend-point mechanism lands in LP64-003 and
  its notice in LP64-007; pool sizes come with LP64-005 and LP64-006.
- LP64-004 (2026-11-16, all ten recommendations of
  [its plan](native_64bit_lp64_004.md)): the tasks follow the measured boot
  order, not this document's class list; animation headers, text banks and
  collision tile data keep the file's bytes (the header packed in place, the
  text slots as offsets the reader adds the base to), while the other seven
  classes get new memory as designed; its done condition as settled under
  "Milestones"; the graphics buffer is scaled by `sizeof(Gfx) / 8` at 64
  bits in this milestone, an exception to the LP64-003 decision on pool
  sizes because its unit is the display-list command; the three fixed model
  buffers in `gun.c` are scaled by 13/8 at 64 bits; the background's room and
  portal tables are built fresh at each stage load at 64 bits, as on the N64,
  and whether the 32-bit build's reloads differ is measured and left to
  LP64-006; the eleven cut stages (eleven background files, which belong to
  nine level ids) keep stopping by name, and LP64-005's decision 4 made that
  the design: each stage number without its files stops by name with status
  1, the `stops` set checking all twelve (those nine level ids, Citadel, and
  solo Temple and Complex); the
  converter oracle is a build option (`-DGE_LP64_ORACLE=ON`, 32-bit and
  headless only), never part of a shipped build; strict-tier fixes the first
  64-bit runs need land through one laptop trip at the milestone's end;
  `Acmd` is widened in this milestone although the mixer first runs in
  LP64-005.
- LP64-005 (2026-11-17, all fourteen recommendations of
  [its plan](native_64bit_lp64_005.md)): "with no `ge: lp64:` line" is read
  as no pool, refusal, converter or stop line, the informational kinds
  allowed; `bondview2.c`'s intro walk, which mis-steps after a WATCH record
  at both widths, is left as inherited; the wallet Bond's room is scaled by
  13/8 at 64 bits by moving the file-select background's start; what "every
  stage" and "with video" mean and its done condition, as settled under
  "Milestones"; the demos' per-tick traces are compared with the i686
  build's in this milestone and each difference diagnosed, arithmetic ones
  going to LP64-006; a headless build replays recorded pads
  (`GE_INPUTPLAY`); overruns are searched by sanitizer runs with poisoned
  arenas and no common symbols, in sanitizer builds only, and records read
  through another type by a reviewed list checked in CI; a pool is raised at
  64 bits in this milestone only if it printed an exhaustion line or peaked
  above 80 percent, final sizes staying with LP64-006's soak; sound counts
  as running when the parameter pool is fixed, a ROM-free synthesizer test
  runs in CI at both widths and recordings are not silent and near a 32-bit
  one's figures; strict-tier fixes land through one laptop trip at the
  milestone's end; a defect the 32-bit game has too is fixed when the fix is
  a few lines, the changed 32-bit function named; the test harnesses'
  macOS failures are left to the macOS project.

Calls made in drafting, accepted with the plan on 2026-11-15:

- Which files are in the gallery's strict tier, and that the build files are
  in the deferred tier.
- Pending is opt-in per clone, and cleared at each milestone's end at the
  latest.
- Linux x86_64 is the first shipped 64-bit platform, compared with i686 in one
  container; Linux aarch64 is built alongside as the fast local loop.
- 32-bit builds keep their current loaders; converters are tested against
  them.
- The host-pointer test is "above 4 GB" with a startup check, not low-bit
  tagging.
- Suspend points are unavailable on 64-bit builds for now.
- Windows x64 is out of scope.
- Equivalence is claimed for the stated surface: all solo stages and demos,
  one four-player route, a menu and watch walk, under default settings and
  with every fork feature on.
- Comparison builds share a test-only maths option; shipping builds are not
  changed to make the comparison pass without the owner's say.
- The Linux aarch64 build is a development and verification target, not
  shipped.
- A divergence seen only under emulation is confirmed on real x86 hardware.
- The plan lives here, with the status table below, and gets a pointer from
  `AGENTS.md` and a `FORK_CHANGES.md` entry when accepted.

## Left to the builder

- The option's name and the CMake structure.
- File layout naming, and whether converters share a pointer-remap helper.
- Measure-then-allocate or bounded allocation, per asset class.
- The order of type fixes within LP64-002.
- The trace and screenshot comparison tooling.
- The replacement backtrace mechanism.

## Milestone status

Update this table before handing off. Record evidence in the notes; do not
mark a milestone done without it.

| ID | Status | Notes |
|---|---|---|
| LP64-001 | Done | **All eight tasks are built (Tasks 1 to 6 were each reviewed before reaching `main`; Task 7 was reviewed before it was pushed). CI run 43803408580 on f4c2c10 passed every job: on the runner the layout probe passed at 64 and 32 bits, the headless configure test passed, and the diagnostics check printed `OK: 1432 diagnostics, baseline 1432`. A whole-milestone review followed the tasks; its fixes are the commits "Compare the Windows executable's load header and new untracked files", "Build the headless tree before counting 64-bit diagnostics", "Assert the light structs in the layout probe and the 32-bit link options", "Refuse a GE_ROM that is not an existing file" and "Correct the LP64-001 record after the milestone review" (FORK-089 records what each changed and ran; CI run 43807622647 below ran on d2feae1, which follows them). The fix wave changed the diagnostics procedure to configure, build the headless tree, then count (CI gained `cmake --build build/lp64/diag -j"$(nproc)"`; the build took 19 s in the x86_64 container), and re-recorded the baseline from the built tree: 1,432 places in 92 files, 59 errors (before: 1,432 in 93 files, 62 errors, three of them a missing generated header). CI run 43807622647 on d2feae1 passed every job: its tests job built and linked the headless tree on the runner, and the diagnostics check then printed `OK: 1432 diagnostics, baseline 1432`. The review gallery was regenerated on the owner's Windows laptop on 2026-11-15, in the commit "Regenerate the review gallery and close LP64-001", clearing the pending state that began at Task 5b (be840ef, the first commit after the 8d6760d regeneration to edit a gallery input, `CMakeLists.txt` and `port/src/headless_stubs.c`; Tasks 5b, 6 and 7 changed deferred-tier gallery inputs on a machine that cannot render).** That regeneration changed only `manifest.json` (its `inputs_sha256`; `strict_inputs_sha256` unchanged) and `index.html`, no image, and `verify` then reported the gallery fresh (the two-tier rule: pending is cleared by a regeneration on a machine that can render, at the end of each milestone at the latest). The previous full regeneration was 8d6760d, also on that laptop. **Commits on `main`:** Task 1 (two-tier gallery rule) 2a3101e, 2c4d7f1; Task 2 (container runner) 54b1209, 857398a, a0ed9b2, 8167b54; Task 3 (layout probe) 803e4c6; Task 4 (diagnostics ratchet) 59d1755, 02e7c35; Task 5 (Windows comparison) ebf3912, c7f9701; Task 5b (headless repair) be840ef, 36d7ed2; Task 6 (`GE_POINTER_BITS`) 72064fe, f126afc; Task 7 (CI wiring and this evidence) 364fb13, f4c2c10. **Task 7 on the Mac, x86_64 container, each CI line run through `lp64_env.py run --arch x86_64 --`, all exit 0:** `test_lp64_env.py` (11 tests), `test_abi_probe.sh` (`ABI probe OK (64-bit)` and `(32-bit)`), `test_lp64_diagnostics.py` (16 tests), `test_compare_win32.py` (22 tests), `test_pointer_bits.sh` (5 cases; the container has MinGW, the CI runner does not, so there case 5 skips), `test_headless_configure.sh` (`Headless configure OK`), the Debug `GE_HEADLESS=ON` configure into a wiped `build/lp64/diag`, and `lp64_diagnostics.py check` (`OK: 1432 diagnostics, baseline 1432`). The runner's tools (`gcc-multilib` and `g++-multilib` from `ci_dependencies.py`'s tests profile; CMake, make, bash, Python 3 and Git from the hosted image) were established by reading, and CI run 43803408580 then passed with them. **Measured:** the emulated i686 headless reference builds and runs the 20-mission smoke with the ROM (exit 0, 20 of 20 PASS, about 184 s); the Windows cross-build is reproducible with a fixed `SOURCE_DATE_EPOCH` (about 56 s a build) and `compare_win32.py` reported the executable identical across the Task 5b and Task 6 changes; the diagnostics baseline was 1,432 places in 93 files with GCC 13 (92 files after the fix wave's re-recording). **Earlier task notes follow.** Task 2 (container runner) done on 2026-11-15, Mac, Docker 29.9.2: `python3 scripts/port/lp64_env.py build-images` took 3 min 58 s (the 2 images). `lp64_env.py run --arch aarch64 -- gcc -dumpmachine` printed `aarch64-linux-gnu` (under 1 s); `--arch x86_64` printed `x86_64-linux-gnu` (7 s); `--arch i686 -- sh -c '... gcc -m32 ... && /tmp/p && echo i686-runs'` printed `i686-runs` (under 1 s), so 32-bit x86 runs under Docker Desktop here. **The i686 headless reference build and smoke run with the stock configuration (Task 5b repaired two defects Task 2 found).** The stock `-DGE_HEADLESS=ON` configuration failed to compile `port/src/port_baked_trace.c` (`SDL.h: No such file or directory`) and failed at link on `portRenderAsyncActive`; Task 2 worked around both with a configure flag and a scratch object. Task 2's run: Configure and build: 20 s. `audit_buffers.py --mission-smoke build/lp64/ref-i686/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 2 --output build/lp64/ref-i686/smoke.json` exited 0 in 184 s with all 20 missions `PASS`. **Task 5 (32-bit comparison) done on 2026-11-15, Mac:** `compare_win32.py commits HEAD HEAD` built twice (54 s and 55 s in the x86_64 container, translated by Rosetta 2, 110 s in all). Without a fixed clock the two builds differed (`.buildid`, `.edata`, 12 `.rdata` bytes: `__DATE__`/`__TIME__` and the About build time); with `SOURCE_DATE_EPOCH` exported in both builds, exit 0, identical, nothing normalised by the tool. `commits 1e3df59 d76b6cf` exited 1, with `.text` and ten other sections different (after the fix). **Fix round 1:** the tool now also compares each loaded section's VirtualSize, VirtualAddress and Characteristics, the entry point, ImageBase, SectionAlignment and every data directory except Debug and Security; it rejects a duplicate section name and leaves out debug sections, `.buildid`, the timestamp and CheckSum, SizeOfImage, NumberOfSections, symbol-table fields and section file offsets (reasons in FORK-089). A debug-only edit (`s32 i` to `int i` in `src/game/bg.c`, restored afterwards) gave exit 0 with only `.buildid`, a debug section and the CheckSum differing, and the tool printed `identical in loaded code and data; .buildid differs`. `commits HEAD HEAD` exit 0; `commits 1e3df59 d76b6cf` exit 1 with `.text` different. 22 ROM-free tests pass. |
| LP64-002 | Done | **All eleven tasks and the final-review fix wave are on `main` since 9fdb6ec (2026-11-16). CI on 9fdb6ec passed every job: run 43918787641 (`tests`, whose log has `build/lp64/gate: 329 files compiled, 0 failed` and `build/lp64/gate-video: 360 files compiled, 0 failed` and the gallery verified; `linux`; `windows`; `sentry-sdk`; `sentry-smoke`; `publish`), run 43918787608 (browser build and deploy) and run 43918787764 (Bright Builds Checks). The review gallery was regenerated on the owner's Windows laptop on 2026-11-16 in 9fdb6ec with 0 changed image cells, and `hand_gallery.py verify` reports it fresh on the Mac. No 64-bit executable has been run; that starts in LP64-003.** Plan: [native_64bit_lp64_002.md](native_64bit_lp64_002.md), written 2026-11-16 from measurements on `main` at ce60a74 (FORK-089 has them and every task's note). **Commits on `main`:** plan f40df14; Task 2 (deferring clone commits strict-tier work on a branch; owner's decision 2026-11-16, built on the Windows laptop) c9ff3a2; Task 1 (video configuration counted, cast allowlist) 1e691b9, 82ada88; Task 3 (prototypes for pointer-returning functions) 283b516, 200230b, 16d5ea8; Task 4 (fonts and text) 4dd051e, 2ea1631; Task 5 (monitor scripts, animation tables) b4e409a, 3b3053e; Task 6 (image bank, textures, model relocation) 5a39418; Task 7 (backgrounds, collision, setup) 9b6ed2d, c6a5a69; Task 8 (addresses, memory, boot, audio, port layer) 6015bdb, 3261ad6, 380a50a, 5c77e73. **Built on the branch `lp64/types-strict` (strict-tier gallery inputs) and landed by fast-forward:** Task 9 (the strict batch: `model.c`, `bondview2.c`, `gunfire.c`, three C++ items) d85c24e, 84a15b0, 80d73c5, e5b7079, 79138e5; Task 10 (the gate) ac8bbe3, dd408f1; the final-review fix wave 3a36ac6, df0346b, 093fe7b (FORK-089, "LP64-002 final review fix wave"); Task 11 (the hook's `main` test fixed and the gallery regenerated, on the Windows laptop) 9fdb6ec. **Count** (GCC 13, int/pointer places of the C files): 1,432 at the start (headless only); Task 1 1,441 (video configuration added); Task 3 1,372; Task 4 945; Task 5 711; Task 6 587; Task 7 460; Task 8 336, 279, 208, then 188; Task 9 133, 40, then 0 with no hard errors. Task 10 removed the count, its baseline and the ratchet. **Runs:** every fix task ran the task check of its time (the count; the i686 headless build and the 20-mission smoke, 20 of 20 `PASS` each time; `compare_win32.py commits HEAD WORKTREE`) and the CI harness lines; each note in FORK-089 records its run. Against `main` at 5c77e73 the 32-bit Windows executable differs in `_loadAnimationFrame` (`model.c`) and `_gunTickHandState` (`gunfire.c`) only, both read instruction by instruction (FORK-089). **The gate (Task 10, widened by the fix wave):** at `GE_POINTER_BITS=64` C is compiled without `-w`, with `-Werror=int-conversion -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast`, C++ without `-w`, with `-Werror=int-to-pointer-cast`, and a named `-Wno-` flag for each other class the build printed (the C log is clean; eleven option-less C++ warnings remain in `port/fast3d`); `gate` refuses an entry lacking those flags or carrying `-w`; `lp64_diagnostics.py gate` compiled all 329 headless and 360 video entries (336 C, 24 C++) with no failure in the aarch64 and x86_64 containers; CI runs it on a 64-bit headless and a 64-bit video tree. It found one conversion the count had missed (`port/src/port_state_effects_audit.c`, video-only code in a file the count compiled only with the headless command), fixed in the same commit. On the runner, CI run 43880427784 (dd408f1, GCC 13.3.0, `libsdl2-dev` 2.30.0 installed) printed `329 files compiled, 0 failed` and `360 files compiled, 0 failed`, then stopped at the stale-gallery `verify` as designed; run 43880503386 built the browser build on the branch. With the fix wave (093fe7b, C++ gated) run 43890641797 printed the same two lines and run 43890647368 built the browser build; the runs on `main` are at the start of this note. **Left for later milestones:** one list by owner, "LP64-002 hand-off" in FORK-089: LP64-003 layout, sentinels and tooling; LP64-004 converters; the 28 allowlisted truncating casts; GCC 14 and later (LP64-007) and clang (the macOS project), neither run. It includes the calls to functions without a prototype, re-measured on 2026-11-16 at 408 places and 311 names (GCC 13.3, both 64-bit trees; the earlier "about 527" was the planning count at ce60a74, pointer-returning calls included), and about 1,780 incompatible-pointer-type places, neither covered by the gate. The hook's branch test, which missed `main` beside a tag named `main` and under the name `Main`, now reads the full ref (FORK-022, fixed in 9fdb6ec). |
| LP64-003 | Done | **All twelve tasks and the final-review fix wave are on `main` since 9abbe58 (2026-11-16). Done condition, as the owner accepted it (decision 1 of the milestone plan): on `main` at 9abbe58 the 64-bit headless executable logs `ge: lp64: address space OK` with every region above 4 GB, then exits with status 5 at a guard naming `animation tables`; `lp64_boot.py` printed `OK: stopped at "animation tables"` for the plain and the sanitizer executable in the aarch64 and in the x86_64 container. CI on 9abbe58 passed every job: run 44007720420 (`tests`, whose log has `gate: 330 files compiled, 0 failed`, `gate-video: 361 files compiled, 0 failed`, `OK: 0 narrowings in 0 files`, `OK: 2 unprototyped calls in 2 files`, `OK: 23 truncating casts in 10 files`, 14 harness passes at each pointer width and the gallery verified; `linux`; `windows`; `sentry-sdk`; `sentry-smoke`; `publish`), run 44007721496 (browser build and deploy) and run 44007720532 (Bright Builds Checks). The review gallery was regenerated on the owner's Windows laptop on 2026-11-16 in 9abbe58 with 0 changed image cells, and `hand_gallery.py verify` reports it fresh on the Mac. No 64-bit build loads a stage: that starts in LP64-004, which inherits the list "LP64-003 hand-over to LP64-004" in FORK-089. The task-by-task account follows, written as each task was built.** **Task 1 (tooling) is on `main` (9a36c51, 15df016). Task 2 (the `-m32` harnesses at both pointer widths) is on `main` (8c01c6b, f4215de, 4d90a65). Task 3 built: at 64 bits the arenas and the ROM image are mapped above 4 GB with no `mmap` hint, a failed mapping stops the game naming it, `PORT_IS_HOST_ADDR` tests the upper 32 bits, and a startup check (`port/src/port_lp64.c`) stops with status 4 if the executable image, an arena, the ROM image, a heap block or a stack address is below 4 GB; the 64-bit executable passes it in the aarch64 and x86_64 containers, then crashes in the animation code as measured (commit "Keep 64-bit game memory above 4 GB and check it at startup"). Task 4 built: each of the ten asset classes' in-place conversions starts with `portLp64RequireConverter` (`port/include/port_lp64.h`, table in `port/src/port_lp64.c`, all ten unconverted), on the compiled-in paths too, so the 64-bit game stops with status 5 naming the class; `scripts/port/lp64_boot.py` boots an executable to its expected stop, and the plain and sanitizer executables stop at `animation tables` after the address line in the aarch64 and x86_64 containers (also with each of the 20 missions as `GE_STAGE`, aarch64); with `animation tables` marked converted in a scratch build the stop moves to `text banks`, and with text banks marked too the boot crashes in the animation code, before fonts (commit "Stop a 64-bit boot at the first asset class without a converter"). Task 5 built: at 64 bits `port/src/port_state.c` is not compiled and the `embed_state_layout.py` step does not run; `port/src/port_state_unavailable.c` reports "Suspend points are unavailable in 64-bit builds." from the existing Suspend Points row and keys, with no menu row changed; the 32-bit build files are byte-identical to a71b43e's and the Windows comparison is identical; the boot check still stops at `animation tables` (commit "Report suspend points as unavailable in 64-bit builds"). Task 6 built: `port/include/port_hostptr.h` gains `PORT_WORD_IS_HOST`, `PORT_WORD_HAS_SEGMENT` and `PORT_WORD_IS_INDEX`, whose 32-bit forms are the sites' old expressions and whose 64-bit forms read the whole word; the five `u32` copies of command words in `propobj.c` and `bg.c` are `uintptr_t` (the `0xABCD` marker test keeps its uncast expression, so at 64 bits it reads the whole word; a review fix, commit "Test the image tag marker on the whole command word"), `lightfixture.c`'s segment-14 test and the texture-number tests in `othermodemicrocode.c` and `port_textures.c` use them, and `port_textures.c` stores an external texture's whole pixel address and no longer refuses external textures at 64 bits, with a ROM-free slot test at both widths; the Windows comparison is identical, narrowings 112 to 107 and truncating casts 28 to 27, and the boot check still stops at `animation tables` (commit "Classify command words and texture slots at pointer width"). Task 7 built: `texLoad` takes `uintptr_t *` and every caller passes the pointer-sized field itself, so the data address is stored whole (commit "Store whole texture pointers through texLoad"); `port/include/port_gfxwords.h` gives command words their accessors (`GFX_W0`, `GFX_OP`, `GFX_W1_DATA`, `GFX_W1_ADDR`), `bg.c` reads the vertex offset from `w1` instead of the upper half of `w0`, the tile-size read shifts the 32-bit word, the traces print whole words, every non-strict narrowing place is resolved (narrowings 107 to 51, all in the six strict-tier files), the 37 hand-written `words.w0` writers were each read and none can sign-extend, and every pre-LP64-002 cast allowance names its owner; the Windows comparison is identical after each group of sites and the boot check still stops at `animation tables` (commit "Read and write display-list words at their own width"). Task 8 built: `vtxstore.c` sizes its slot arrays by `sizeof`; the collision walk's locus record is written and cleared through its type, so the ladder flag reaches `count` at 64 bits (a ROM-free test at both widths), and the movement code's storage for it holds one at 64 bits; `struct tex`, the alarm sound handle, the tag list head and a collision-bounds out-parameter are typed; the 51 incompatible-pointer places of the plan were each classified (13 resolved here, five no longer in the class, the rest file layouts for LP64-004, float views with the same layout at both widths, or Task 11's seven), and the room union was reviewed and left; the Windows comparison is identical (commit "Size and access run-time structures by their types"). Task 9 built: the non-strict callers of 96 functions that take or return a pointer now have a prototype in scope, 26 through a header that already declared the function, 62 written by hand in the header that owns the defining file, two in a new `port/include/port_options.h` and three in new `src/game/crc.h`, `src/game/line_tri_intersect.h` and `src/game/initunk_0009E0.h`, and three as forward declarations in their own file (138 of the task's 139 places); the prototypes revealed `atof` called as an implicit `int` in two diagnostic far-plane settings (the 32-bit code read `eax` and left the `double` on the x87 stack; both now read the `double`), a local wrong-width `fwrite` declaration in `image.c` (removed) a `sub_GAME_7F0A1DA0` declaration that disagreed with its definition (corrected), `initAnimationsBuffer`'s `s32` parameter given an array address, a pointer cut to 32 bits at 64 bits (the parameter and the unread global it fills are now `void *`), and, once five defining files include their headers, an upstream `display_image_at_position` declaration with `s32` where the definition has `u32` (corrected); `strcmp` in `port_build_time.c` (`<str.h>` needs the game's headers, which break its harness) is blocked and keeps its allowlist line, so `unprototyped` is 159 places in 75 files to 21 in 9 (the 20 strict-tier places and that one); the Windows executable differs only in the two `atof` callers and in register, stack-slot and store-order choices in eight functions whose calls and arguments are the same (commit "Declare the pointer-taking functions that were called without a prototype"), and the second and third commits leave it identical (commits "Declare the remaining pointer-taking functions through new headers and forward declarations" and "Give initAnimationsBuffer's unused parameter a pointer type and check five prototypes against their definitions"). Task 10 built: `include/PR/gbi.h` declares `Hilite`'s alignment words and the `Gsetcolor` and `TexRect` words through `gbi_long` and `gbi_ulong`, which are `long` and `unsigned long` where `long` is 32 bits and `int` and `unsigned int` where it is wider, so the three are 16, 8 and 16 bytes at both widths (asserted by the layout probe); `gu.h`'s `FTOFIX32` converts to `int` there, so a value past 32768.0 no longer wraps at 64 bits; the game's `strtol` (`src/str.c`) bounds overflow by the 32-bit maximum and returns its result through `int`, so at 64 bits 2^32 gives -1 and "-5" gives -5 (ROM-free tests in `test_lp64_address.sh`); every other `long` the brief names was read and is arithmetic, N64-only, uncompiled or a comment (FORK-089 has the table), and the Windows comparison is identical (commit "Keep long-typed data layouts at their N64 size on 64-bit builds"). Task 11 part A built on the branch `lp64/addr-strict`: the game's `Vertex` is 16 bytes at both widths, its collision link a 32-bit offset from the model file's base at 64 bits, read through `VERTEX_COLLISION_NODE` (asserted by the layout probe); `test_wardrobe.sh` passes at 64 bits and `HARNESS_PENDING` is empty; the Windows comparison is identical (commit "Keep the game's Vertex at 16 bytes on 64-bit builds"). Its second commit, "Size Model storage by its type and test model words at full width": at 64 bits the model slots, the hand's gun-model and read-write runs and the player's watch runs hold a whole `Model` and pointer-sized read-write slots (32-bit layouts asserted unchanged by the probe), the watch's raw player offsets are member addresses, `PROMOTE` tests `PORT_WORD_HAS_SEGMENT(var, 0x05)`, and `loadAnimationFrame` takes the copy branch for any host address (the RAM branch never ran in the measured i686 smoke); `model.c` and `bondview2.c` declare 10 of their 11 unprototyped callees (`texSelect` left out); the Windows comparison differs in `loadAnimationFrame` and `solo_char_load` only, a store reorder and a register choice that change no behaviour. A third commit after review, "Read the watch model's fields through its Model at 64 bits and correct the collision vertex record", reads the watch's `render_pos`, `scale` and `animframe1` through its `Model` at 64 bits, records the collision vertex fixups LP64-004's converter must make, and leaves the Windows executable identical to the second. Task 11 part B built on the same branch: commit "Carry texture and model addresses whole through the gun and arm code" keeps texture data addresses and the hand's matrix word pointer-sized through the arm and grip code, reads command words through the Task 7 accessors, gives `gunfire.c` the gun model's `render_pos` and pointer-sized read-write slots at 64 bits, declares the strict files' unprototyped callees (35 narrowing places, 7 unprototyped calls and 4 truncating casts fixed) and leaves the Windows executable identical. Commit "Read display-list words at pointer width in the renderer" reads `gfx_pc.cpp`'s first words and data words through the accessors (its last 16 narrowing places), resolves a host pointer before any segment test in `seg_addr` at 64 bits, prints whole words in two traces, adds a 64-bit fault on a command word with a non-zero upper half, compiled only with `GE_LP64_DL_CHECK` (sanitizer builds, or by hand; a review fix on the same branch), declares `glad.c`'s two string functions and removes the now empty narrowing allowlist, again with an identical Windows executable. A review fix, commit "Compile the display-list word check only when asked for, and correct the narrowing rule's text", makes that fault opt-in (`GE_LP64_DL_CHECK`, defined only by `GE_SANITIZE`; no 64-bit build has run it, since none runs a stage yet) and corrects `AGENTS.md`'s allowlist sentence, with an identical Windows executable. Final review fix wave built on the same branch (FORK-089, "LP64-003 final review fix wave"): model read-write data counted and stored in pointer-sized slots, the gun's, watch's and casing's render data set by member, the casing's throw-matrix offsets and two `Switches` byte walks read by member or entry, the weapon buffer's `Model` step sized by the type, `glass2.c`'s image-table stride and colour read through their types, the renderer's walker taking its opcode from the whole command word, and three tooling and comment fixes; the 32-bit Windows executable is identical to a7f4ca9's. The hand-over to LP64-004 and the 13 functions for the laptop are assembled in FORK-089 ("LP64-003 hand-over to LP64-004" and "For the laptop (Task 12)"). Task 12: the laptop regenerated the gallery and landed the branch in 9abbe58 (FORK-089, "LP64-003 task 12").** Plan: [native_64bit_lp64_003.md](native_64bit_lp64_003.md), written 2026-11-16 from measurements on `main` at 2afe62d and accepted by the owner the same day with all nine of its recommendations; nothing beyond Tasks 1 to 10, part A of Task 11 (three commits), the commits of part B named above and the final review fix wave is built. Twelve tasks: tooling (two static checks for what the gate cannot see), the `-m32` harnesses at both widths, arenas above 4 GB with the host-pointer test and the startup check, a named stop at each unconverted asset class with a boot check, suspend points unavailable, full-width classification of command words and texture slots, display-list words, run-time structures, prototypes for pointer-taking functions, `long` in data layouts, one strict-tier batch on `lp64/addr-strict`, and the landing through the laptop. **Measured while planning** (FORK-089 has the runs): the 64-bit headless and video trees link on aarch64 and x86_64; the headless executable boots through the ROM load and the text banks and dies in the animation table (`model.c:941`; under the sanitizers at `initanitable.c:241`), the same with its arenas pinned low or above 4 GB; it passes 0 of the 20 smoke missions; in a scratch copy, with the animation header stopgapped the next crash is the audio bank (`alBnkfNew`), and with sound switched off the boot reaches the title's stage load. All three are LP64-004's in-place conversions, so this milestone's done condition is sharpened (the plan's decision 1, accepted): the 64-bit executable passes the startup check and stops with status 5 at a guard naming `animation tables`, the first conversion the boot reaches. 225 places narrow a 64-bit integer where the gate cannot see (111 from pointer-typed values); 157 calls without a prototype reach functions that take pointers; 16 of the 18 `-m32` harnesses pass at 64 bits with the flag removed. **Not measured:** any stage load at 64 bits, the video executable at run time, real x86 hardware. |
| LP64-004 | Done | **Done on 2026-11-17. All of LP64-004 is on `main` at bbcac00 ("Regenerate the review gallery after the LP64-004 converters", the owner's laptop), followed by the closing record (FORK-089, "LP64-004 task 17"): the sixteen Mac-side tasks, the commits outside them and the final-review fix wave (the commits after the plan's acceptance commit 41543f9 up to d1d156c), then the regeneration bbcac00, then the closing record's commits. Done condition (the plan's decision 3, accepted), as measured by the closing runs on bbcac00 (task check steps 2, 5 and 6 at `--fixed-tick --frames 1800`, the dated note under the plan's Task check): all ten classes are marked converted in `port/src/port_lp64.c`; `lp64_boot.py --expect-run --frames 600` printed `OK: ran 600 frames` for the plain and the sanitizer executable in the aarch64 and in the x86_64 container; the 20-mission smoke passed 20 of 20 on the plain executable in both containers and on the sanitizer executable in aarch64, with no sanitizer report and no `ge: lp64:` line other than the four informational kinds (`INFORMATIONAL_PREFIXES` holds a fifth, the oracle build's, which no 64-bit run prints); the oracle (i686, all ten classes) reported animation tables 3633, text banks 255, audio banks 42, music sequences 21, textures 2521, fonts 63, models 512, backgrounds 45, collision tiles 47 and setup files 54 files, 0 differ each; the load census was equal for 20 of 20 missions, 1,031 lines compared (811 model, 20 setup, 20 stan, 20 bg, 160 text); the 32-bit smoke passed 20 of 20 (i686), and `compare_win32.py commits f73382a HEAD` printed `identical in loaded code and data; .buildid differs`, exit 0, so no changed function needed explaining; CI on bbcac00 passed (run 44200431317 and the browser and standards runs); the gallery was regenerated in bbcac00 with 0 changed image cells and `hand_gallery.py verify` reports it verified. Step 1 printed 354 and 385 files compiled, 0 failed, 0 narrowings and 2 unprototyped calls in both containers, and the cast allowlist keeps 24 casts in 11 files with every reason rewritten (Task 17 Step 3). Decision 3's "with no `ge: lp64:` line" is read as no pool, refusal, converter or stop line, with the informational kinds every 64-bit run prints allowed (LP64-003's address check prints one on every run); that reading was the controller's at the close, and the owner accepted that reading on 2026-11-17 ("Accept the reading", in the session that closed LP64-004, as that session relayed it). No sanitizer report inherited from the 32-bit game occurred, so none was suppressed. **Departures from the accepted plan:** the collision file's 16-byte header record is kept outside the file at 64 bits (`romfile_stan.c`'s one record), because 18 of the 27 stock stans have no room slot to write it over; the setup converter is wired through `romdataFilePreprocess`, not a call in `src/game/ob.c`, which is at the file-length limit; at 64 bits the bank allocator (`port/src/port_memp.c`) keeps every block 8-aligned; the mission smoke's fixed tick is an option (`--fixed-tick`, default off), used at 1800 frames for the milestone's runs; `portBankConvertedSize` and `portBankConvert` take two parameters the plan's signatures lack, the wave table's size and the bank's name; the model image keeps the file's order, not the plan's grouped order, so that the 32-bit image equals the in-place one; at 64 bits one pad word goes before an intro camera or credits record that would fall at 4 mod 8; nothing went on the strict branch, which was never created, so Task 17's laptop trip only regenerated; Task 17's Step 3 kept all 24 casts and rewrote their reasons instead of compiling functions out; a refused text bank is zeroed and the game carries on (a controller ruling; the plan left the caller's policy open); a zero-byte room list is accepted as an empty list, as at 32 bits (a controller ruling correcting its own instruction to refuse one); and `AGENTS.md` gains a rule on sizing record and display-list storage by `sizeof`, the controller's addition, which the owner may remove. **Storage defects found outside the converters, ten** (64-bit only, each fixed with an identical Windows program): the model hit pool sized 600 times 0x14 (69759f6); the player's gait model storage too small for a 64-bit `Model` (9f37737); the gait run's flip and frame names read by position over the `Model` (46ba04d); the player record, the inventory and the anim model slots' stride sized by 32-bit literals (a27e1bd); the pause list, the watch background lists and the gun barrel list sized for 8-byte commands (73308dd); and the briefing's language bank span stepped as `Gfx`, past its block (a657b4b). **What no run of this milestone exercised:** any input (pause, the watch pages, file select, the briefing, the gun barrel list, multiplayer, the attract cast and demos, cheats, stage transitions), play beyond 1800 frames, the renderer's walk of a display list and `GE_LP64_DL_CHECK`, sound synthesis and mixing, the cut stages, the video executable and real x86 hardware; the fix wave's three display-list fixes (73308dd) are pinned by the layout probe and reading, and its briefing span fix (a657b4b) by reading only. **Hand-over:** what LP64-005 inherits, with the three decisions that waited for the owner at the close (the reading of decision 3, `bondview2.c`'s intro walk, the wallet Bond's room; all three answered on 2026-11-17, the last two with the LP64-005 plan), is in FORK-089, "LP64-004 hand-over to LP64-005". The task-by-task account follows, written as each task was built; where a note names the class the boot stops at, a path no run has reached or a commit not yet pushed, it describes its own commit, and this statement supersedes it.** **Task 1 built: the converters' shared helpers (`port/include/port_romconv.h`, `port/src/romconv_common.c`) and first file layouts (`port/include/port_romlayout.h`), compiled only at 64 bits and in the new 32-bit headless oracle build (`-DGE_LP64_ORACLE=ON`, refused at 64 bits and outside a headless build); the oracle (`port/src/port_lp64_oracle.c`, `scripts/port/lp64_oracle.py`), which with no class named ran the boot and the 20 missions and exited 0; `lp64_boot.py --expect-run`; and `RomDemoHeader` at the cart's 0xE8 bytes (the demo header needs no change: playback steps by `RAMROM_HEADER_ROMSIZE`, and the struct is 0xE8 at 64 bits and with MinGW, 228 only on i686 Linux). The Windows comparison is identical, the 64-bit executable still stops at `animation tables`, and no class is converted (commits "Add the converter interface, the oracle build and the boot check's run mode" "Keep the demo header at the cart's size and describe it as it is" and "Make the reservation and frame-count checks able to fail, and reuse the offset map's storage"; FORK-089, "LP64-004 task 1").** **Task 2 built: at 64 bits the four pools that failed without a word name themselves in the log, `ge: lp64: pool "POOL" exhausted (request N bytes)` (N is the block tried or the free space wanted, not the size a fix needs; `portLp64PoolExhausted`, at most eight lines a pool a stage; `port/include/port_lp64.h`): room memory (`bg.c`, both failures of `bgLoadRoomModelData`) and the texture pool (`image.c`, beside `ge: TEXOOM`) log and go on as at 32 bits, and the music heap (`heapalloc.c`) and the graphics buffer (`dyn.c`, a frame that ends past the buffer) stop the game with exit status 6 (`portLp64PoolFatal`); the graphics buffer is doubled again at 64 bits, by `sizeof(Gfx) / 8` (decision 4); a model slot that returns `NULL` (`model.c`, strict tier) is left for the hand-over. the boot check and, from a second commit, the 20-mission smoke fail a run whose log has a pool line or a refused conversion (commit "Fail a smoke mission whose log names an exhausted pool or a refused conversion"). No real run reaches these pools yet (the boot still stops at `animation tables`): they are proven by a ROM-free test, which also runs the music heap's own allocator, by reading and by the gate. The Windows comparison is identical; the frame-end check's benchmark is owed by LP64-005 (commit "Name the pools that fail quietly on a 64-bit build"; FORK-089, "LP64-004 task 2").** **Task 3 built: the animation tables are the first class converted. At 64 bits `ModelAnimation` is packed and 4-byte aligned (`src/bondtypes.h`), and `portAnimHeadersConvert` (`port/src/romconv_anim.c`, file layout `RomAnimHeader`) writes each of the 173 headers over its 0x14-byte file header in the permanent copy, in 0x20 bytes, with `address` a host address into the entries image and the descriptor and stream pointers into the staging image; it refuses, writing nothing, a header that does not fit, two less than 0x20 apart, or (since commit "Refuse an animation header whose descriptor or stream lies outside the staged image") a header whose descriptor or stream offset is at or past the staged image's size. At 32 bits `expand_ani_table_entries` is unchanged and the Windows comparison is identical. The 64-bit executable, plain and under the sanitizers, now stops at `text banks` (aarch64 and x86_64); the oracle reports `animation tables: 173 files, 0 differ` in each of its 21 runs; `model.c` is not edited. The real boot reaches the conversion only after text banks (`langInit` runs first), so on the stock ROM it was run in a scratch copy that moved it before the text banks stop: 173 headers converted, every animation's first and last frame loaded, no sanitizer report (commit "Convert the animation table's headers on a 64-bit build"; FORK-089, "LP64-004 task 3").** **Task 4 built: the text banks are converted. At 64 bits `portTextBankConvert` (`port/src/romconv_text.c`) keeps each slot of a bank's table as its bank-relative offset in native byte order, and `langGet` (`src/game/language.c`) adds the bank's address, as its N64 branch does; each port relabel (`sTextPatches`, `port/src/romdata.c`) is copied over the string it replaces. A relabel longer than its string, of a slot that names no string, or over bytes another slot's string shares refuses the bank, at both widths, so the 32-bit oracle checks the stock relabels: LoptionsE's three fit. At 32 bits the in-place rebase is unchanged and the Windows comparison is identical. The 64-bit executable, plain and under the sanitizers, now stops at `audio banks`, which is also the first real run of Task 3's animation converter (no refusal, no sanitizer report); the oracle reports `text banks: 255 files, 0 differ` over its 21 runs (95 banks at boot, 8 in each mission) (commit "Keep text bank slots as offsets on a 64-bit build"; FORK-089, "LP64-004 task 4"). Task 5 built: the audio banks are converted. At 64 bits `portBankConvert` (`port/src/romconv_bank.c`) builds the effects and instruments banks' run-time records (one `ALBankFile` and one record per distinct bank, instrument, sound and wave table, taken from the music heap after a measuring pass) and leaves envelopes, key maps, ADPCM books and loops in the file image in native byte order, as the 32-bit `alBnkfNew` does; `ALInstrumentAlt_s` (`src/snd.h`) drops a padding word at 64 bits so that `sndPlaySfx`'s view of the effects instrument stays one pointer before `ALInstrument.soundArray`, which the ABI probe asserts. At 32 bits `alBnkfNew` is unchanged and the Windows comparison is identical. The 64-bit executable, plain and under the sanitizers, now stops at `music sequences`, having built 16,448 bytes of records for `sfxctl` and 10,520 for `instrumentsctl`; playback is not yet reached by a 64-bit run. The oracle reports `audio banks: 42 files, 0 differ` over its 21 runs, and it reported every bank as differing when the converter was broken in one field of a bank, an instrument, a sound, a wave table or an envelope (commit "Build the audio banks' run-time records on a 64-bit build"; FORK-089, "LP64-004 task 5"). The review fixes refuse, when measuring, two records that share a byte (an in-file swap could otherwise change a record the write pass had yet to read, and outgrow the measured buffer) and an ADPCM or raw wave whose samples pass the wave table; both converter functions now take the table's size and the bank's name, unlike the plan's signatures; the stock ROM still converts, with the same sizes and `audio banks: 42 files, 0 differ` (commit "Refuse audio banks whose records overlap or whose waves pass the table"; FORK-089, "LP64-004 task 5, review fixes"). Task 6 built: the music sequences are converted. At 64 bits `portSeqTableConvert` (`port/src/romconv_seq.c`) writes the sequence table at its run-time size (1,016 bytes for 63 sequences, each offset zero-extended into its pointer slot) from the ROM segment into the music heap, refusing a file too short for its count's entries or a room too small, and `musicSeqFileNew` rebases it unchanged; at 32 bits the in-place copy is unchanged and the Windows comparison is identical. The 64-bit executable, plain and under the sanitizers, now reaches the title's stage load and stops at `textures`; its music heap holds 281,584 of 753,664 bytes (219,120 at 32 bits). No committed run plays a sound or a sequence (a headless build has no synthesizer); a scratch probe, not committed, decompressed all 62 tracks through the converted table and read the 261 effects through `sndPlaySfx`'s view with the same results at both widths. The oracle reports `music sequences: 21 files, 0 differ`, one file in each of its 21 runs (commit "Build the music sequence table at its run-time size on a 64-bit build"; FORK-089, "LP64-004 task 6"). Task 7 built: no class is converted; the audio command words are pointer-sized at 64 bits (`Aword` in `include/PR/abi.h`, `uintptr_t` there and `unsigned int` at 32 bits, so `Acmd` is 16 bytes at 64 bits), the nine address macros store the whole address, and `portAcmdValidate` reads the second word at full width. A ROM-free test runs every address command the interpreter can run without a synthesizer through it at both widths, at 64 bits on pages above 4 GB; it is the only 64-bit run of the interpreter, since a headless build emits no audio command. The boot still stops at `textures`, the music heap holds 329,584 of 753,664 bytes at 64 bits (the game's two unused command lists double), and the Windows program is identical (commit "Carry whole buffer addresses in audio commands on a 64-bit build"; FORK-089, "LP64-004 task 7"). Task 8 built: `textures` is marked converted and the 64-bit boot stops at `fonts`, plain and under the sanitizers. The display-list expander (`port/src/romconv_gfx.c`: `portGfxFromRom`, `portGfxFromRomBackward`, `portGfxFromWords`) turns 8-byte ROM commands into host `Gfx` with both words zero-extended, under overlap rules its header states; `texLoadFromGdl` counts host commands at 64 bits and `texCopyGdls` expands backwards there, both 32-bit bodies kept. The read of `texLoad` found no address held in 32 bits and no literal record size; the two room loaders still size their lists in ROM bytes, for Task 12. The oracle compares the expander with `texCopyGdls`'s 32-bit copy of every room list a run loads, and the Windows program is identical (commit "Expand ROM display lists to host commands and count them by size"; FORK-089, "LP64-004 task 8"). Outside the tasks, commit "Size record pools by their records on a 64-bit build" sizes the PC `ModelHitEntry` pool by its record (40 bytes at 64 bits, not the literal 0x14), whose overrun the sanitizers could not see in a common symbol; it converts no class (FORK-089, "record pools sized by their records"). Task 9 built: `fonts` is marked converted and the 64-bit boot stops at `models`, plain and under the sanitizers on aarch64 and x86_64 (the x86_64 sanitizer boot only with the record pool fix beneath it). `port/src/romconv_font.c` builds each Latin font's `struct font` (0x2F4 bytes longer at 64 bits) with each glyph offset moved by that growth, refusing a short file or room, a size that would wrap and a glyph offset outside the glyph bytes; `segments.c` stages the two fonts in their converted size rounded up to 16 (+0x300, so the stage bank stays 8-aligned) and the font list in 0x180 bytes of `Gfx`, all inside `IS_64_BIT`. The oracle finds 63 font files (3 per run over 21 runs) identical, and the Windows program is identical (commit "Build the font tables at their run-time layout on a 64-bit build"; FORK-089, "LP64-004 task 9"). Task 10 built: the model file converter (`port/src/romconv_model.c`, `_walk.c`, `_refs.c`, `_records.c`), not yet called, so `models` stays unconverted and the boot still stops there. It walks a file as `romfile_model.c` does, refusing what that walk refuses and the malformed graphs that would loop or overlap, places every item in file order at its run-time size, and writes every reference as its item's new offset; at 32 bits the image equals the in-place one. A scratch harness, not committed, found all 512 stock model files converting at 64 bits (3,289,344 bytes in, 4,015,208 out) and identical to the in-place result at 32. After review, the converter also refuses an item that would end past 16 MB (where `0x05000000 | offset` meets the segment byte) and a segment-5 word in a list command carrying an address it does not move (the stock ROM has none) (commit "Refuse model references past 16 MB and other list addresses"). The Windows program is identical (commit "Add the model file converter"; FORK-089, "LP64-004 task 10"). Task 11 built: `models` is marked converted, and the 64-bit headless executable runs frames for the first time: it converts the legal screen's and the Nintendo logo's models at load and runs 600 frames of the title, plain and under the sanitizers on aarch64 and x86_64, and a mission (`--stage dam`, which now also sets `GE_STAGE_AFTER=1`) stops at `backgrounds`. `load_object_fill_header` places the texture entries by pointer stride and converts each served file into its own block through `romfileModelConvertWide` (`romfile_model.c`), which refuses by name an image larger than the room (the caller's size, or `fileGetCapacity` for a bank load) and on any refusal leaves the tables empty for the existing "rendering nothing" branch; `gun.c`'s three model rooms and the weapon buffer are 13/8 of their 32-bit sizes at 64 bits. Measured over the 512 stock files against every address-and-size model load in `src/game/` and `port/src/`, no model passes its 64-bit room; the tightest is the wallet Bond's (`front.c`, 0xA000, unscaled because the file-select background and the briefing follow it in the same buffer): `PwalletbondZ`'s 38,368-byte image leaves 2,592 bytes, accepted (review fixes, commit "Record the wallet Bond's model room and correct the tightest room"). No real run has yet loaded a model through a bank, `gun.c`, `bondview2.c`, `title.c`, the wallet or the attract cast. The oracle's sweep converts the 512 files the compiled header tables name by both paths: `models: 512 files, 0 differ`. The Windows program is identical (commit "Convert model files at load on a 64-bit build"; FORK-089, "LP64-004 task 11"). Task 12 built: `backgrounds` is marked converted, and a 64-bit mission (`--stage dam`) now stops at `collision tiles`, plain and under the sanitizers on aarch64 and x86_64. At 64 bits `load_bg_file` builds the room and portal tables (40- and 16-byte records, offsets kept as zero-extended segment 0x0F words) in the stage pool at each load through `portBgTablesLoad` (`romfile_bg.c`, converter `romconv_bg.c`), which stops the game by name on a bg that is not served or that the converter refuses; the visibility commands and portal points stay in the file. Dam's tables take 8,688 bytes of the stage pool, the most of the 20 missions; Runway's 1,256 the least. The two room loaders expand a room's E bytes of commands into 2E bytes of `Gfx` at the block's end (`portBgRoomListExpand`), failing to the existing `room memory` line when the block is smaller; no run has reached a room load yet. Two portal strides in `bg.c` keep their 32-bit tokens under `#if !IS_64_BIT`, because the typed form changed the 32-bit code. The oracle compares the 32-bit tables with the converter's for the 25 bg files the ROM holds and the 20 missions' loads: `backgrounds: 45 files, 0 differ`. A reload of Dam measured 334 portal fields left by the first load's passes and play, none in the rooms (FORK-089, "LP64-004 task 12"). The Windows program is identical (commit "Build the background's room and portal tables and expand room lists on a 64-bit build"). Review fixes: the converter ends the room table where the game does (the first record after record 0 whose primary offset is 0) and its messages and the record now say so; all 25 stock closing records are zeros. A 0-byte room list is an empty list, as at 32 bits; the oracle's sweep counts a file the ROM holds but the game does not serve as a difference. Task 13 built: `collision tiles` is marked converted, and a 64-bit mission now stops at `setup files`, plain and under the sanitizers on aarch64 and x86_64, for Dam (5 room slots) and Caverns (none). The plan wrote the 16-byte header record over the file's first bytes and refused a file with no room slot, but 18 of the 27 stock stans have none (their first tile at 0x0C), so by the coordinator's ruling the record lives outside the file at 64 bits: `portStanConvert` (`romconv_stan.c`) checks the whole file, then converts the tiles in place and leaves the header as the file has it, and `load_bg_file` points `gptr_stan` at `romfile_stan.c`'s one static record, filled by `portStanRecordFill` from the header (`{NULL, file + first_tile}`); one record is enough because `load_bg_file` is the only path that loads a stan and the game reads the file only through `stan_prefix`. At 32 bits the converter writes the record and rebases the room slots as before, and the oracle compares it with the in-place pass: `collision tiles: 47 files, 0 differ` (the 27 distinct stans, each identical, and the 20 missions' loads). Dam's load runs every room through `bgRoomCalcBB` one at a time with no `room memory` line; many rooms at once in play is unmeasured. The 32-bit special-portal walk still depends on `g_BgCurrentRoom` following its array. The Windows program is identical (commit "Keep the collision file's header record outside the file on a 64-bit build"; FORK-089, "LP64-004 task 13"). Task 14 built: the setup file converter for the tables and the intro (`port/src/romconv_setup.c` and four companion files), not yet wired in, so `setup files` stays unconverted and a 64-bit mission still stops there. It builds the header, pads, bound pads, waypoints, waygroups, paths, AI lists, name tables, intro and credits table at their run-time sizes in new memory, every reference through the offset map, names and AI code resolved inside the runs of bytes no table owns; the prop records go through stubs that accept only the end record, which Task 15 replaces. At 64 bits a camera or credits record at 4 mod 8 gets one `INTROTYPE_MAX << 24` word first, which every intro walker steps over. A scratch sweep of the 34 stock setup files: 0 refused at both widths, 0 differ from the in-place pass at 32 bits, 1,600,720 bytes to 1,760,968 at 64 bits. The in-place pass swaps a list word twice where two lists share it (none in the stock ROM). The Windows program is identical (commit "Add the setup file converter for the tables and the intro"; FORK-089, "LP64-004 task 14"). Review fixes after it: an intro test fixture with one record of every kind (type words, pad words after a CUFF and a WATCH, the three game intro walks reproduced, `bondview2.c`'s WATCH mis-step recorded: its PC `INTRO_TYPE` reads a WATCH's hours as a SPAWN, at 64 bits too unless a CREDITS record follows, and fixing it in that strict-tier file would change the 32-bit game, an owner's decision; the walk runs only on a multiplayer respawn, and none of the 13 stock `Ump_` setups has a WATCH), the prop converter run in the checking pass so that a refusal never leaves OUT written, and two test and comment corrections; the Windows program is identical. Task 15 built: the tenth class, `setup files`, marked converted. The prop records are written by a per-type table derived from the code that reads them (`portSetupWide`: file words, run-time words, pointer words, each pointer at an 8-aligned offset), `sizepropdef` returns the run-time size at 64 bits, `prop.c`'s fix-up loop works by member, and `romdataFilePreprocess` converts a served setup into its own stage-bank block, whose committed size grows to the image's; an unserved setup stops the game by name. The oracle with all ten classes: `setup files: 54 files, 0 differ` (the 34 retail setups and the 20 missions' loads). A 64-bit Dam start runs 600 frames, plain and under the sanitizers, on aarch64 and x86_64, after a preparatory fix of the player's gait model storage (commit "Give the player's gait model room for a 64-bit Model"). The Windows program is identical (commit "Convert setup files at load on a 64-bit build"; FORK-089, "LP64-004 task 15"). Review fixes after it, each with an identical Windows program: the setup oracle's credits rebase without a truncating cast (the cast check had failed); the gait run's two names read as `Model.gunhand` and `animframe1` (`bondhead.c`) taken from the 64-bit `Model` (they had flipped a byte of its scale and read padding), with every name of the run pinned in the ABI probe; the player (0x2A80 bytes against a 0x30A0-byte record) and the inventory (0x14 bytes an item against 32), which every 64-bit mission had overrun, Dam included, unseen by the sanitizers inside the one stage-bank allocation, sized by their records, and the anim model slots' 268-byte stride rounded to 16, which had left the bank at 4 mod 8 and stopped Caverns and Facility; and the setup block's capacity consumed by the preprocess, so a load that records none is refused. After them Dam, Caverns and Facility each run 600 frames, plain and under the sanitizers, on aarch64 and x86_64; the other 17 missions are not yet run at 64 bits, the other setups were converted only by the boot probe, and the record types those three missions do not load (CCTV, vehicles, aircraft, safes and tanks among the object records; the other kinds were not counted) are covered only by the table tests and reading. Second review round: at 64 bits the bank allocator keeps every cursor 8-aligned from its bank's start (`port/src/port_memp.c`; the 32-bit allocator unchanged), so a stage-bank block whose size is not a multiple of 8 (the font tables, the anim model slots, `model.c`'s reset-time slot of 268 bytes (corrected in the fix wave below: not the font tables, which are 0x27B0 and 0x3840 bytes at 64 bits, padded by Task 9), which no stock run of the title, Dam, Caverns or Facility reaches) no longer misaligns the next block; those three missions use the same stage-bank bytes with and without it, and the ABI probe's search for unread gait names allows a space after `->` and checks that it read every name the probe declares. The Windows program is identical. Task 16 built: all 20 missions run on a 64-bit build (600 frames on the game clock, and 1800 at one field a frame), plain on aarch64 and x86_64 and under the sanitizers on aarch64, 20 of 20 each, with no pool, refusal or stop line and no sanitizer report; no defect was found, so no fix was needed. The Windows program is identical. The load census (`scripts/port/lp64_census.py`, its ROM-free test in CI) compares each mission's model, setup, stan, bg and text lines at both widths: 20 of 20 equal, 1031 lines. For it the 64-bit model, setup and stan converters print the in-place passes' lines with the same counts, and the census runs use the smoke's `--fixed-tick` (one game field a frame) for 1800 frames, measured as the frame by which every mission has loaded its first-person weapon, since on the game clock the slower emulated 32-bit run reached the weapon load sooner; owner decision 3's "no `ge: lp64:` line" is read as no pool, refusal, converter or stop line, informational lines allowed (the controller's reading then; the owner accepted it on 2026-11-17). Prop record types 0, 15, 16, 31 and 41 and model opcodes 0, 3, 5, 6, 7, 11, 14, 15, 16, 17, 19 and 20 are verified by fixtures only (commit "Run the 20 missions on a 64-bit build and compare what they load"; FORK-089, "LP64-004 task 16").** **Fix wave after the final whole-milestone review** (eight commits, on `main` since d1d156c, each with an identical Windows program; FORK-089, "LP64-004 fix wave A" to "H"): the pause and watch display lists and the gun barrel list get room for 16-byte commands, with the ABI probe pinning four player list rooms ("Give the pause, watch and gun barrel display lists room for 16-byte commands"); the briefing's language bank starts `argg` bytes after the briefing at 64 bits, where it had run 0x200 bytes past its 0x78000-byte allocation ("Place the briefing's language bank after the briefing on a 64-bit build"); a file of a served class the ROM does not serve, a missing ROM segment and the model loader's compiled-in branch stop a 64-bit build by name, the `GE_ROM*=0` knobs are ignored at 64 bits and a compressed text bank is zeroed ("Stop a 64-bit build by name on a file the ROM does not serve"); the narrowing check counts `Aword` as pointer-sized ("Count audio command words as pointer-sized in the narrowing check"); the audio and background converter declarations move to their own headers, each under 500 lines ("Split the audio and background converter declarations into their own headers"); the relabel rule is stated at `sTextPatches`, `PROPDEF_UNK41` leaves the 64-bit prop fix-up, the oracle's stan sweep counts an unserved stan as a difference and the font staging arrays are asserted at compile time ("Note the relabel rule, drop UNK41 from the fix-up, sweep unserved stans and pin the font arrays"); the smoke and the boot check fail any `ge: lp64:` line outside an informational list ("Fail a 64-bit run on any lp64 line outside the informational list"); and this row and FORK-089 are corrected where the review found them wrong ("Correct the LP64-004 record after the final review"). No 64-bit run has pressed a button or walked a display list: pause, the watch pages, file select, the briefing, the gun barrel list, multiplayer, the attract cast, cheats and stage transitions are all unexecuted at 64 bits, the fix waves' paths included; the first 64-bit run with input and a renderer should be a sanitizer build with the renderer, which compiles in `GE_LP64_DL_CHECK`. Plan: [native_64bit_lp64_004.md](native_64bit_lp64_004.md), written 2026-11-16 from measurements on `main` at f73382a and accepted by the owner the same day with all ten of its recommendations ("Decisions" above lists them): seventeen tasks. The tasks follow the order the boot needs the classes, measured in a scratch copy by treating each as converted in turn: animation tables, text banks, audio banks, music sequences, then in the title's stage load textures and fonts, then models at the legal screen; backgrounds, collision tiles and setup files follow in a mission's load (by reading; not run). The design's list has animation tables fourth and audio ninth. **Measured while planning** (FORK-089, "Milestone LP64-004", has the runs): with scratch stand-ins for the first six classes the plain and the sanitizer 64-bit executable reach the legal screen and stop at `models` with status 5; animation headers cannot move (83 sites address them by constant offset) but fit in place when `ModelAnimation` is packed at 64 bits, with no change to `model.c`; converted data costs a mission 34 to 82 KB for its models and 4 to 17 KB for its setup file, the audio banks 27,664 bytes of the music heap, a stage's room lists at most 115 KB, and the stage bank has about 63 MB free at 32 bits; three fixed model buffers in `gun.c` fit their largest converted file by about 5 percent; `ALInstrumentAlt_s` and the demo header `ramromfilestructure` change layout at 64 bits. **Planned shape:** each class gets a file layout, a converter compiled only at 64 bits and in an opt-in 32-bit oracle build (`-DGE_LP64_ORACLE=ON`), a ROM-free test on input built in the test, and a boot check showing the stop move; animation headers, text banks and tile data keep the file's bytes and the other seven classes get new memory; no strict-tier edit is planned, and what the first 64-bit runs need there lands through one laptop trip. **Done means** (the plan's decision 3, accepted): all ten classes converted, the 64-bit headless executable running the title for 600 frames and the 20-mission smoke 20 of 20, plain and under the sanitizers, the oracle reporting no difference, and the 32-bit program unchanged. **Not measured while planning:** any stage load at 64 bits; the model, background, collision and setup paths at run time; the video executable and the mixer; real x86 hardware (the tasks measured the first two since; the last two remain unmeasured). |
| LP64-005 | Planned, accepted | **Planned on 2026-11-17 and accepted by the owner the same day with all fourteen recommendations ("Decisions" above lists them); execution started at Task 1.** Task 1 built: `scripts/port/lp64_play.py` runs the sets `demos`, `stages`, `stops`, `menus`, `tours` and `attract` (117 cases) and judges each by the lines that prove it ran, with `traces` and `callers`; the plain aarch64 executable passed 117 of 117, the sanitizer one 114 (the three setup stops fail on the planned `strncpy` overrun, Task 7), x86_64 passed the 14 demos, and all fourteen demos first differ from the i686 build, which agrees with itself, at tick 2, as `lp64_demo_traces.txt` records. Task 2 built: in sanitizer builds the game's arenas are poisoned (`port/include/port_poison.h`; the bank, with a 32-byte poisoned gap after each block, the room heap, the music heap in whole 16-byte lines and the graphics pool from each frame's start) and C is compiled with `-fno-common`, so global arrays get red zones; shipped builds are unchanged (`compare_win32.py`: identical in loaded code and data); `scripts/port/test_lp64_poison.c` pins the hooks with 17 probes, each with a deadline; with their fixes reverted, the hit pool's overrun was reported through `-fno-common`'s red zone (not the poisoning), the player's and the inventory's through the poisoned bank, and the gun barrel list was not reported, by construction (it overruns inside one block its user carves up); the first full sanitizer pass found an inherited fifth explosion vertex written into room for four, fixed in its own commit ("Stop writing a fifth explosion vertex into room for four", which changes `explosionRenderPart` in the 32-bit program by one store); the sanitizer executable then passed 114 of 117 cases, failing only the three planned setup stops (FORK-089, "LP64-005 task 2"). Task 3, first part built: `scripts/port/lp64_overlays.py list` finds five kinds of record read through another type in `src/`, `port/src/` and `port/include/`, keyed by file, function, kind and normalized text, and after three rounds of review fixes lists 1,355 sites (113 `index`, 165 `offset`, 56 `opaque`, 995 `record`, 26 `run`): every measured positional read, and every compiled dereference of a pointer cast that a scan of each `*` followed by one finds (the scan's 170 are 165 sites and 5 multiplications); the forms it still cannot see are those of its blind-spot statement, with `list --locals` printing the 279 stored scalar-pointer casts as a survey aid; `check` fails today because the list is not yet reviewed, and it joins CI when the review batches finish (FORK-089, "LP64-005 task 3, first part", "review fixes to the overlay lister", "offsets of zero" and "a cast before a dereference"). Task 3, review batch 1 built: the tool takes the verdict `defect` (`check --no-defects` fails on one), and the 195 `index`, `opaque` and `run` sites and `src/sched.c`'s dead read have their verdicts, 82 `same`, 71 `typed`, 11 `dead`, 2 `overlay-64` and 30 `defect` (27 for Task 4, three of them the same at both widths, and 3 strict), with 25 layout assertions in `scripts/port/abi_probe/probe_overlays.h`, each failing when its sites' layout drifts (after the batch's review fix); `check` reports 1,159 `offset` and `record` sites unreviewed (FORK-089, "review batch 1"). Task 3, review batch 2 built: the 164 remaining `offset` sites have their verdicts, 54 `same`, 46 `typed`, 53 `dead`, 8 `overlay-64` and 3 `defect` (1 strict, 2 for Task 4, the same at both widths), with 8 more layout assertions; `check` reports the 995 `record` sites unreviewed (FORK-089, "review batch 2"). Task 3, review batch 3 built: the 492 `record` sites in `src/game/` from `bg.c` to `options.c` have their verdicts, 322 `typed`, 145 `same`, 21 `overlay-64` and 4 `defect` (3 for Task 4, 1 strict) after its first review fix, with 25 more layout assertions and a ROM reader (`lp64_model_scan.py`) behind the 45 `measured:` verdicts; `check` reports the other 503 `record` sites unreviewed (FORK-089, "review batch 3" and its fix round 1). Task 3, review batch 4's first commit built: the ROM reader counts a slot past a model's table as `out of range`, fails on an unreadable or unscanned model and reads all 512 stock models; the re-measured `measured:` lines keep their verdicts (FORK-089, "review batch 4, first commit"). Task 3, review batch 4 built: the last 503 `record` sites have their verdicts, 321 `typed`, 169 `same`, 10 `dead` and 3 `defect` (Task 4: UNK41 read as an ObjectRecord, which no stock setup holds), with 11 more layout assertions and a setup reader (`lp64_setup_scan.py`); `check` passes with every one of the 1,355 sites reviewed and 40 defect lines (FORK-089, "review batch 4, second commit"); fix round 1 corrected the UNK41 defect (different bytes at each width; each of its four defect lines names the same places: four that take the record as an ObjectRecord, prop.c:2530 and 2739, cleanup_objects.c:46 and loadobjectmodel.c:506, seven uses of what the last returns, and a 32-bit-only byte-order swap; `cleanup_objects.c`'s re-judged, 41 defect lines), the other flagged reasons (45 lines changed in all) and three weak assertions, and made the setup reader fail on what it could not resolve. Task 3, review batch 5's first commit built: stored scalar-pointer casts are a sixth kind, `stored` (`list --locals` removed), and its 279 sites have their verdicts, 177 `typed`, 41 `same`, 1 `overlay-64`, 59 `dead` and 1 `defect` (Task 4: the playerTick store behind chraction.c's `unk2c[3]` read, the same defect as batch 1's), with 4 more layout assertions in `probe_stored.h`; `check` passes with all 1,634 sites reviewed and 42 defect lines (FORK-089, "review batch 5, first commit"). Task 3, review batch 5's second commit built: `lp64_pointer_types.py scan` lists the 113 record types a 64-bit build reaches only through pointers (81 used, 32 unused); each is judged in FORK-089's table, with 8 more layout assertions in `probe_views.h` and no new defect ("review batch 5, second commit"). Task 3 built: `lp64_overlays.py check` runs in CI's tests step and passes with all 1,634 sites reviewed (113 `index`, 165 `offset`, 56 `opaque`, 995 `record`, 26 `run`, 279 `stored`; 935 `typed`, 492 `same`, 32 `overlay-64`, 133 `dead`, 42 `defect`: 37 for Task 4 and 5 `strict:`), printing 42 defect lines and 114 `measured:` lines; `AGENTS.md` states the rules; FORK-089's "task 3 as built" summary lists the blind spots with what covers each and the 42 defect lines as 16 defects, in the order a 64-bit run meets them, for Task 4 and the strict branch. Batch 5 review fix 1, first commit: the stored finder takes conditional arms, groups, comma and `+` operands and initializer lists (7 more sites, 1,641), the `playerTick` store is `typed` (41 defect lines), and `measured:` counts only reasons that start with it and name a scanner command (114). Second commit: the pointer-only scan counts an embedding only inside an object, lists 123 types (10 more rows, one new assertion), and `lp64_pointer_types.py check` compares it with the committed `lp64_pointer_types.txt` in CI. Third commit: the Task 4 defect list is rewritten (41 lines, 16 defects: 13 `Task 4:`, 3 `strict:`); the five wrong at both widths (entries 10 to 13 and 16) get 64-bit-only repairs inside `#if IS_64_BIT`, with the 32-bit side inherited and listed for the owner, and the other eleven name the identical 32-bit load that makes their repair the same code at both widths; counts now 939 `typed`, 492 `same`, 32 `overlay-64`, 137 `dead`, 41 `defect`. Fix round 2: the UNK41 repair returns NULL from `setupCommandGetObject` at 64 bits (removing its label there was a no-op), the animation table store is `same` with a new probe assertion, and the stored finder no longer reads a later declarator as a dereference; 938 `typed`, 493 `same`. Fix round 3: entry 10 names its 21 object types, and entry 12 also covers the NOTHING, DEBRIS and UNK16 records `setupCommandGetObject` returns through its missing `default` (64-bit repair: `default: return NULL;`); no stock setup holds any of them (measured). Task 4 built: the 13 `Task 4:` defects (36 lines) are repaired, one commit each in the list's order: entries 1 to 9 read or write the member by name or through the record's own type, the same 32-bit load (the Windows executable identical at each commit), and entries 10, 11, 12 and 13 are 64-bit-only repairs inside `#if IS_64_BIT` with today's tokens in the `#else`; `AttachmentChild` is gone (`AttachedObj.child` is a `struct PropRecord *`, 122 pointer-only types); `test_lp64_typed_reads.sh` (new, in CI) and eight 32-bit probe assertions were each seen failing first; `lp64_overlays.py check` passes with 1,619 sites, 5 defect lines (the `strict:` entries 14 to 16) and 108 `measured:` lines. Task check on the last commit's tree: step 8 plain 117 of 117 on aarch64 and on x86_64, sanitizer 114 of 117 (the three allowed `strncpy` stops); 13 of 14 demos are identical to the i686 trace, and frigate2 first differs at tick 273, not yet diagnosed (Task 5). The twelve earlier commits leave `test_lp64_overlays.py`'s pinned site counts stale; the last commit pins them (FORK-089 has every figure). Task 5 built: frigate2's difference at tick 273 was a 64-bit defect, a door's clearance test reading a guard's hidden flag through the wrong member of PropRecord's union (bit 0x400 of the guard's prop pointer at 64 bits), repaired in `src/game/propobj.c` with the same 32-bit code and the ROM-free `test_lp64_typed_doorclear.c`, seen failing first; all 14 demos are now identical to the i686 trace, none arithmetic or unstable; task check step 8 plain 117 of 117 on aarch64 and on x86_64, sanitizer 114 of 117 (the three allowed `strncpy` stops). Task 6 built: the `demos` and `attract` sets passed 14 of 14 and 4 of 4 on the plain 64-bit headless build in the aarch64 and in the x86_64 container (translated by Rosetta 2 on this Mac at close to native speed, not real x86 hardware) and on the sanitizer build (aarch64), every demo ending at its measured tick, and `traces` found all fourteen demos' per-tick traces identical between the aarch64 and the x86_64 runs. Task 7 built: the inherited `strncpy` overrun in `indyrescmdCheckFileExists` is bounded (the one 32-bit function changed, at 5a88e66) and a 64-bit miss no longer reads the absent host's unwritten answer (e17c452); the `stages`, `stops` and `tours` sets passed 54, 12 and 26 on the plain aarch64 and x86_64 (Rosetta 2) and the sanitizer aarch64 executables, every tour ending at the same room as on the i686 reference, step 8 passed 117 of 117 on all three, and `lp64_census.py types` reads play folders, which leave model opcodes 0, 3, 5, 6, 7, 11, 14, 15, 16, 17, 19 and 20 and prop types 0, 15, 16, 31 and 41 to fixtures (LP64-006's hand-over). Task 8 built: a headless build replays a `GE_INPUTPLAY` route as pad 0, `GE_PLAYLOG` prints `ge: play:` menu, stage and kill lines and `GE_MENU=briefing` opens a briefing (the shipped 32-bit program unchanged), and the new `input` set (pauses, the five watch pages, walking and firing, the front end from boot to Dam, the watch's abort, the briefing and a respawn in each of the 13 arenas) passed 27 of 27 on aarch64, plain and under the sanitizers, and step 8 144 of 144 on the plain aarch64 and x86_64 (Rosetta 2) and the sanitizer aarch64 executables, those 64-bit runs being of `main` plus the three strict repairs the branch `lp64/play-strict` carries (in `gunfire.c` and `bondview2.c`; `main` alone ran only the `input` set at 64 bits), and reverting 73308dd or a657b4b makes the watch or briefing cases fail with a sanitizer report; `GE_KILLBOND`'s player-order defect in `bondview2.c` is reported for the strict batch; fix round 1 makes the watch cases require the tab drawn, the walks a distance walked and a shot, logs a kill only when it happened, and has the probe check the briefing step `front.c` compiles. Task 9 built: `lp64_play.py run --video [--shots N]` runs a `GE_VIDEO` executable offscreen and requires the GL line and at least 30 pictures a case, and `lp64_env.py run --arch i686 --video` runs in the new `midas-lp64-i686-video` image (`goldeneye` plus the i386 EGL), where the 32-bit video reference builds and plays runway2 to its measured end at tick 388 in about 405 s at about one picture a second (under qemu-i386), so at the build's default 120 Hz its `runway2` case with video ran out of its 3,000 frames first; fix round 1 paces video cases at 60 Hz (`GE_TICKHZ=60`) and runs them windowed at 640x480, and its part 2 adds `--timeout-scale F` for slow hosts, with which `runway2` with video passes on the 32-bit reference in about twelve minutes (`--timeout-scale 3`, demo end at tick 388, 2,980 pictures), so the fallback is not invoked; a whole set with video there would take hours, not a routine check on this Mac. Task 10 built: the synthesizer's parameter pool is allocated and stepped by `ALParamRoom`, the union of the four update views (28 bytes at 32 bits as before, 40 at 64, where a start's wave pointer had overrun the next record's link), and the reverb reads its delay taps through `-(s32)` of the `u32` tap, which at 64 bits had sent every reverb load 8 GB past its line and every audio frame to silence; the Windows program is identical, and `test_audio_synth.sh` (ROM-free, CI) fails against either old file. The `sound` set (video only) records the title and Dam to `DIR/CASE.wav`; on aarch64 with video both pass plain and under the sanitizers with no skipped frame, and against the 32-bit reference (i686 video, emulated) their peaks and non-zero shares are within a factor of two, while their lengths are not on three of four, because under the dummy audio driver the pump's top-ups make the length follow the host's speed (FORK-089, "LP64-005 task 10"). Plan: [native_64bit_lp64_005.md](native_64bit_lp64_005.md), written from measurements on `main` at 98a76dd: fifteen tasks and fourteen decisions for the owner, each with a recommendation, all accepted on 2026-11-17 (FORK-089, "Milestone LP64-005", has every figure). **Measured while planning:** each of the 14 attract demos plays to its end on a 64-bit headless build, plain and under the sanitizers, at the tick the i686 build ends it, but the per-tick traces differ from the second tick: the i686 build stays exact with the N64's recorded random seed through seven whole demos, and the 64-bit build parts from it at tick 2 in all fourteen, because `weaponIsOneHanded` (`src/game/chraction.c`) reads a weapon's record by position through a `ChrRecord` cast (byte 128 at 32 bits, 140 at 64, where the field is at 144), so every armed guard takes the unarmed idle branch; with stand-ins for that and two more positional reads in a scratch copy, three demos trace identically to the i686 build and eleven first differ later (tick 35 to 406), the same on aarch64 and x86_64. The 13 multiplayer arenas (2 and 4 players, every scenario on one) and Cuba run 1,800 frames; twelve stage numbers stop by name with status 1 (the nine cut level ids, Citadel, and solo loads of Temple and Complex), where the i686 build crashes on ten, hangs on one and runs one. A room tour of the 20 missions and six arenas (848 room visits) printed no pool line. A 64-bit `GE_VIDEO` tree configures, compiles and links in both containers and runs there today through SDL's offscreen driver and Mesa's software renderer: the title drew 166 pictures under the sanitizers with no display-list fault. Every run that makes sound fails (signal 11 in `alEnvmixerPull` or `__allocParam`): the synthesizer's parameter pool holds 32-byte records at 64 bits and a 40-byte `ALStartParamAlt` is written into them; with a stand-in for that, the game runs and the recorded sound is silence, every sample load being refused (`ge: audio: WARNING - audio frame skipped (silent)`), cause not found. A headless build cannot press a button (its pads are zeroed and the replay hook is video-only). **Planned shape:** a play check (`lp64_play.py`) that judges sets of runs by what their logs show; sanitizer builds that poison the game's arenas and drop common symbols; a reviewed list of every record read through another type, checked in CI; the measured defects fixed with a check that failed first; recorded pads replayed in a headless build; video offscreen in the containers with a 32-bit video reference; sound sized, made audible and recorded; pool peaks printed; the late-load audit at both widths; strict-tier work on `lp64/play-strict` through one laptop trip. **Not measured:** a mission ended by completing its objectives; sound that is not silence; more than a few dozen pictures a run under the sanitizers; any pool's peak; real x86 hardware and a GPU. |
| LP64-006 | Not started | |
| LP64-007 | Not started | |

## Hazard inventory

Known places where 32-bit assumptions live, from a read of the tree on
2026-11-15. It is a starting checklist, not a complete list; line numbers will
move. The per-site compiler diagnostics are regenerated by LP64-001's script
(`lp64_diagnostics.py sites`).

**Addresses carried in 32 bits**

- `osPiStartDma(..., u32 devAddr, ...)` (`port/src/libultra_shim.c`), reached
  by every `romCopy` (`src/ramrom.c`); `romCopyAligned` does `s32` maths.
- `osVirtualToPhysical` returns `u32` (133 call sites); the
  `OS_K0_TO_PHYSICAL` family in `include/PR/os.h` (52 sites).
- `memp`: `mempCheckMemflagTokens(s32, s32)`, `mempSetBankStarts`
  (`src/memp.c`). `mema`: `s32 addr` (`src/mema.c`), `memaRealloc(s32 ...)`.
- Globals: `ptr_bg_data`, `gptr_stan`, `ptr_bgdata_offsets` (`bg.c`);
  `ptr_texture_alloc_end` and neighbours (`image.c`); `barrelDisplayListPtr`
  (`title.c`); `globalbank_rdram_offset` (`image_bank.c`);
  `ptr_text_first_mp_award` (`bondview.h`); `animation_table_ptrs1/2`
  (`initanitable.c`); `GunfireRecord.BaseAddr`; `ModelAnimation.address`.
- `StateRegion`/`StateHeader` in `port/src/port_state.c`, and
  `scripts/port/embed_state_layout.py` (`<II`).
- Arena hints and `u32` arithmetic in `port/src/pcsched.c`;
  `PORT_IS_HOST_ADDR` casts to `unsigned int`.
- The 23 `(u32)(uintptr_t)` sites: `romfile_setup.c`, `romfile_stan.c`,
  `bg.c`, `port_arm_mesh.c`, `romdata.c`, `propobj.c`, `romfile_model.c`,
  `port_textures.c`, `port_grip_mesh.c`.

**In-place conversions to replace**

- Text banks: `romdataTextPreprocess` (`romdata.c`), read by `langGet`.
- Setup files: `romfileSetupConvert`; promotion in `src/game/prop.c`
  (`SETUP_PROMOTE`); record sizes from `sizepropdef`
  (`src/game/loadobjectmodel.c`). Structs: `stagesetup`, `PadRecord`,
  `BoundPadRecord`, `waypoint`, `waygroup`, `PathRecord`, `AIListRecord`,
  `pname`, `SetupIntroCredits`, and the prop records used in place as runtime
  objects (`ObjectRecord`, `ChrRecord`, `GuardRecord`).
- Collision tiles: `romfileStanConvert`, `portStanPack` (`src/game/stan.c`);
  `StanPrefixRecord` and the room pointer table. `StandTile` has no pointers.
- Models: `romfileModelConvert`, `load_object_fill_header`
  (`src/game/objecthandler_2.c`), `modelPromoteNodeOffsetsToPointers`
  (`src/game/model.c`). Structs: `ModelNode`, the `ModelRoData_*` family,
  `ModelFileTextures`, `Switches[]`, `Vertex`.
- Backgrounds: `romfileBgConvert`, `load_bg_file` (`src/game/bg.c`),
  `BG_SEG_TO_PTR` (`bg.h`); `bg_room_data`, `bg_portal_data_entry`. The
  background is converted inside the ROM image itself.
- Animations: `portAnimDataInit` (`port/src/animdata.c`),
  `expand_ani_table_entries`; `ModelAnimation`'s packed header.
- Fonts: `portFontBuildSegments` (`port/src/segments.c`), fix-up in
  `src/game/textrelated.c`; `struct fontchar`.
- Textures: `texLoad(s32 *updateword)` (`src/game/image.c`) writes a data
  pointer into model texture IDs, image table entries and display-list words;
  `struct tex`.
- Audio: `alBnkfNew` (`port/src/audio_stubs.c`); music sequence table in
  `src/music.c`.

**Display-list stride and layout**

- `texCopyGdls`, `texLoadFromGdl` (`src/game/tex.c`): `>> 3`.
- `((u32 *)gdl)[n]` reads in `bg.c` and `propobj.c`; a 32-bit `w0` shift in
  `bg.c`.
- `swapDL`, `modelRecordSize` and 24-byte nodes in `romfile_model.c`.
- The font list copied into `u32 *ptr_font_DL` (`src/game/lv.c`).
- Port code that writes command words directly: `port_hud.c`,
  `port_console.c`, `port_arm_mesh.c`.
- `long` is 8 bytes on LP64, so `include/PR/gbi.h` structs that use it grow:
  `Hilite`'s `long int force_structure_alignment[4]`, `Gsetcolor`'s
  `unsigned long color` and `TexRect`'s four `unsigned long` words. The pointer
  diagnostics do not report it. Measured: `Hilite` is 16 bytes at 32-bit and
  32 at 64-bit (`Gsetcolor` 8 and 16, `TexRect` 16 and 32); the layout probe
  asserts `Hilite`'s 32-bit size.

**Sentinels and unions**

- `TextureID < MAX_TEXTURES` (`image.c`, `front.c`, `objecthandler_2.c`);
  `< PORTMAX` (`bg.c`); `anim->address & 0x80000000` (`model.c`);
  `!= (s32 *)1` (`initanitable.c`); `BG_SEG_IS_OFFSET` (`bg.h`); the model
  promote mask `0x05000000` (`model.c`).
- Unions: `pname {char *; s32}`, `Vertex`'s link, `Gfx`.

**Hard-coded sizes**

- `Model` allocated as `0x20`; `rwdata` as `numRecords*4` (`model.c`);
  `sizepropdef` word counts; `PORT_FONT_NATIVE_WORDS`; the permanent bank's
  fixed size (`memp.c`); the music heap (`music.c`).
- `bg_room_data` (`src/game/bg.h:152`, three pointers and a `coord3d`) is
  addressed with the literal stride `room * 24` and the `pos` offset `+ 12` at
  `src/game/bg.c:4453,4657,6392,6425` (line numbers at ce60a74), through `(s32)`
  casts of `ptr_bgdata_room_fileposition_list`. Left unchanged by LP64-002;
  LP64-004's.

**Run-time structures (LP64-003 Task 8)**

- Fixed: `vtxstore.c`'s slot arrays are sized by `sizeof` (were `0x14`);
  `StandTileLocusCallbackRecord` is written and cleared through its type (the
  ladder flag was lost in the upper half of `rooms`), and the movement code's
  `move_bond_temp_struct` (`bondview.h`) holds one at 64 bits;
  `sub_GAME_7F0CCB38` (`tex.c`) reads `struct tex`'s `width` by name at 64
  bits (word 2 is half of `data` there); `propobj.c`'s `byrefA` receives
  `chraiGetCollisionBounds`'s polygon pointer in a pointer (it was an `f32`).
- Left: `vtxstore.c` allocates its `Vertex` arrays at `0x10` a slot (Task 11
  makes `Vertex` 16 bytes). On PC `sub_GAME_7F0CCB38`'s 32-bit word read sees
  `width & 3` where the N64 read `depth`; kept, a behaviour question.
- File layouts found by the incompatible-pointer review, LP64-004's: setup
  prop records walked as `u32` words with `sizepropdef`
  (`cleanup_objects.c`, `loadobjectmodel.c`) and the tag records' `NextTag`
  links; model node records (`Switches[n]->Data`) read as `f32 *` by index
  (`chraction.c`, `propobj.c`); `expand_ani_table_entries` on the compiled-in
  animation tables as `s32 **` (`initanitable.c`).
- Unions: the room union in `bgTestRayIntersectionInRoom` (`bg.c`) is
  consistent at 64 bits (its `s32` member is N64-only). File-record unions for
  LP64-004: `LinkRecord`, `LockDoorRecord` and `SafeObjectRecord`'s
  pointer/index pairs, `SetupIntroCamera`'s `lang1c` and `lang20`, `pname`,
  and the firing animation tables' `anim` (`chr.h`) and `initialanim`
  (`initactorpropstuff.c`).
- Seven places of the same review sit in strict-tier files (`bondview2.c` 4,
  `gunfire.c` 1, `model.c` 2): Task 11's.

**Found while planning LP64-004** (2026-11-16; the milestone's plan places each)

- `ALInstrumentAlt_s` (`src/snd.h`), the view `sndPlaySfx` indexes the effects
  bank through, has its array at 12 at 32 bits and at 16 at 64, so its index k
  stops being the standard record's k - 1.
- `ramromfilestructure` (`src/game/ramromreplay.h`) is 228 bytes at 32 bits
  and 232 at 64 (two `u64` fields raise its alignment); `ramromreplay.c` steps
  past the demo header by `sizeof`. (LP64-004 Task 1, 2026-11-16: the cart's
  header is 0xE8 bytes, the struct's size at 64 bits and with MinGW; only i686
  Linux has 228, and playback steps by `RAMROM_HEADER_ROMSIZE`, so nothing
  changed.)
- `texLoadFromGdl` counts commands as `srcsize >> 3` and `texCopyGdls` copies
  two `u32` a command into `Gfx` steps (`src/game/tex.c`).
- Three model loads in `src/game/gun.c` use fixed buffer sizes (0xBD70,
  0xAFD0, 0x7530) that the largest converted files fit by about 5 percent.
- The bg front matter is read and written in place inside the ROM image
  (`obGetBGFileData`), so at 32 bits it keeps what play wrote into it across
  loads of a stage.
- `initSetLevelLoadPropSafeItem` (`src/game/initobjects.c`) writes
  `ObjectRecord.prop` where the reader expects `SafeObjectRecord.next`; the
  intro credits record keeps a pointer in an `s32` (`src/game/bondview_r.c`).
- The graphics buffer's unit is the display-list command: at 64 bits the same
  bytes hold half as many, and nothing checks its end outside `GE_ROOM_AUDIT`.

**Found while planning LP64-005** (2026-11-17; the milestone's plan places each)

- `weaponIsOneHanded` (`src/game/chraction.c`) reads `WeaponObjRecord.weaponnum`
  as `((ChrRecord *) record)->act_bytes.padding[84]`: byte 128 at 32 bits,
  byte 140 at 64 bits, where the field is at 144.
- `chraction.c` takes an animation's frame count as `((u16 *) header)[2]`
  (three places); `unk_092E50.c` writes a display list's fifth command as
  `((u32 *) list)[8]` (two places); `options.c` reads a font record as `s32`
  words (a debug cursor).
- The synthesizer's parameter pool (`src/libultra/audio/synthesizer.c`) holds
  records of `sizeof(ALParam)`, 32 bytes at 64 bits, and
  `alSynStartVoiceParams` writes a 40-byte `ALStartParamAlt` into one.
- At 64 bits the interpreter's bounds check refuses audio frames at a sample
  load and plays silence instead (a 1,500-frame recording of Dam was all
  zeros); the cause is not known.
- `ALSndpEvent` (`src/snd.c`) is copied as an `ALEvent`; both are 32 bytes at
  64 bits by coincidence, and nothing asserts it.
- `roc_part_key` (`port/fast3d/gfx_roomcache.inl`) shifts a 64-bit list
  address left by 20 bits; `phVector` (`port/src/port_hud.c`) leaves the upper
  halves of three commands' words unwritten.
- `CMakeLists.txt` compiles all C with `-fcommon`, so uninitialised global
  arrays get no sanitizer red zone; the stage bank, the room heap, the
  graphics pool and the music heap are each one allocation.
- `indyrescmdCheckFileExists` (`src/game/indy_commands.c`) overruns a stack
  buffer in `strncpy` at both widths, on the load of a file the ROM lacks.
- A demo must not be run with `GE_FIXEDTICK` (it then plays at one field a
  tick, not at its recorded pace), and `GE_NOSLEEP` lets `GE_FRAMES` pass
  before the game has run.

**x86-only code**

- Frame-pointer walks in `port/src/main.c` and `port_profile.c`; `cpuid` in
  `sysstubs.c` (already guarded); the i386 PipeWire workaround in
  `audio_pc.c`; top-level assembly blocks in `segments.c`, `ramrom_data.c`,
  `ucode_dl_stubs.c` (including an absolute `.set` that assumes a
  non-relocated link) and `port_weapon_images.c`.

## Technical references

- [ABI and portability constraints](../../docs/port/abi-and-portability.md):
  the 32-bit lock as inherited.
- [Save states](save_states.md), [buffer audit](buffer_audit.md),
  [hand gallery](hand_gallery.md), [benchmarks](benchmarks.md),
  [crash reporting](crash_reporting.md), [releases](releases.md).
- `port/src/romdata.c` and `port/src/romfile_*.c`: today's in-place loaders.
- `port/src/pcsched.c`: arenas and fixed addresses.
- `port/src/port_state.c`, `scripts/port/embed_state_layout.py`: suspend
  points.
- Perfect Dark port, 64-bit work: fgsfdsfgs/perfect_dark pull request 521,
  `port/src/preprocess/`, and its issues 317, 529, 541, 542, 545, 553, 567 and
  652 for the bug classes it met: use-after-free exposed by layout change,
  pointers passed through `u32` parameters, sentinel constants, alignment of
  converted data, signedness of widened fields, and `sizeof` of structs ending
  in a one-element array.
- Upstream's unmerged 64-bit work: murkantor/Project-Midas pull requests 42
  and 95, issue 79.
