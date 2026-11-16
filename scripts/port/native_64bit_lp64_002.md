# LP64-002 Type Hygiene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix declarations until a 64-bit compile of the whole tree reports no
pointer/integer conversion, then make that an error in the 64-bit build and in
CI, without changing the 32-bit program.

**Architecture:** The work is ordered by root cause, not by file: 66 wrongly
typed declarations account for 1,384 of the 1,432 measured places. Each task
fixes one group of declarations on `main`, lowers the recorded count in the
same commit, and proves the 32-bit Windows executable unchanged. Fixes inside
the gallery's strict-tier files are batched on one branch with the gate itself
and reach `main` through one regeneration on the Windows laptop.

**Tech Stack:** C (GCC 13, Ubuntu 24.04 containers), CMake, Python 3 standard
library, Bash test harnesses, Docker Desktop on Apple Silicon.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md): "Design > Game code
types", verification items 1 and 3, "Gallery rule: two tiers", milestone
LP64-002 and the hazard inventory. Read it first, then `AGENTS.md` and
`scripts/port/arm_rendering.md`.

## What was measured for this plan (2026-11-15, `main` at ce60a74)

- **Root causes.** The 1,432 places (92 files, headless C files) were
  attributed one by one: 1,384 to 66 named declarations or idioms, 48 to
  one-off sites. The cluster table is below. 167 places are in strict-tier
  files (`bondview2.c` 82, `model.c` 47, `gunfire.c` 38).
- **A real 64-bit compile already works for almost everything.** With
  `-DGE_POINTER_BITS=64 -DGE_HEADLESS=ON`, 328 of 329 translation units
  compile, identically in the aarch64 and x86_64 containers. Only
  `src/game/propobj.c` fails: 59 `initializer element is not computable at
  load time` errors, all from `MONJUMPTO`/`MONJUMPCHANCE` putting a table's
  address into a `u32` element. No assembly, intrinsic, `#error` or missing
  header stands in the way of compiling. Linking is untested.
- **The video configuration** needs only the distribution's `libsdl2-dev`
  (neither image has a 64-bit SDL2). With it, 356 of 360 translation units
  compile at 64 bits. `video.c`, `input_pc.c`, `audio_pc.c`, `raphnet.c`,
  `glad.c` and `puff.c` compile; `audio_pc.c` has 8 int-to-pointer casts and
  `glad.c` 1 int-conversion that the headless count does not include.
- **C++ (24 files, all under `port/fast3d/`, strict tier)** has no int/pointer
  conversion. `gfx_pc.cpp` and `gfx_opengl.cpp` fail only at 64 bits because
  `port/fast3d/system.h:12` declares `typedef int64_t s64` against the game's
  `long long s64`. `gfx_pc.cpp:8558` has one `-Wnarrowing`. `gfx_shiny.cpp`
  fails in the container at both widths with six C-linkage conflicts, because
  the game's `src/sched.h` shadows `<sched.h>` on the system-SDL include path.
- **`-w` disables the gate.** `CMakeLists.txt` passes `-w`, which also switches
  off `-Werror=int-conversion` and its siblings. Without `-w` there are about
  8,000 other warnings.
- **Missing prototypes.** 102 of the places are calls to pointer-returning
  functions with no prototype in scope (about 50 functions), so the result is
  an implicit `int`. Inserting the prototypes in a scratch copy removed 92
  places and revealed at least 30 that are hidden today (17 of them in
  `image_bank.c`).
- **`#line` files.** `src/game/gunfire.c`, `gun.c`, `lv.c` and
  `src/libultrare/audio/env.c` use `#line`, so the compiler's line numbers for
  them are not the file's lines.
- **The Mac cannot commit a strict-tier change.** The pre-commit hook treats a
  strict mismatch as stale and fails, on any branch. Task 2 settles how the
  strict batch is committed.

The per-site attribution (`attributed.tsv`, `sites_with_messages.tsv`,
`table.md`, `site_tail.md`, `compile_state.md`) is kept in the ignored folder
`.superpowers/sdd/native_64bit_lp64_002/research/` of the Mac checkout. Task 1
makes the tool able to regenerate the per-site messages.

## Cluster table

Places are diagnostics at `main` ce60a74. "Strict" is how many of them sit in
strict-tier files and therefore stay until Task 9.

| Task | Cluster | Declaration (where) | Now | Wanted | Places | Strict |
|---|---|---|---|---|---|---|
| 3 | Missing prototypes | about 50 pointer-returning functions; list from `sites --messages` (`implicit declaration`, then `makes pointer from integer`) | implicit `int` | include the declaring header | 102 | 34 |
| 4 | `frontPrintText` fonts | params 5, 6 (`src/game/front.c:1114`; extern copies in `port/src/port_headshots_menu.c:20`, `port_mission_times.c:19`, `port_reinforcements_menu.c:15`) | `s32` | `struct fontchar *`, `struct font *` | 252 | 0 |
| 4 | Font locals | `pFontFile`, `pFontChars`, `pFontFile2`, `pFontChars2`, `sp78`, `sp7C` in `src/game/options.c` | `s32` | font pointers | 102 | 0 |
| 4 | Text locals | `langGet()` results in `options.c`, `front.c`, `mpmenu.c` | `s32` | `char *` | 53 | 0 |
| 4 | White-text fonts | `display_aligned_white_text_to_screen` args 7, 8 (`front.c:1802`; extern in `port/src/port_startup.c:11`) | `s32` | font pointers | 6 | 0 |
| 4 | Award text | `ptr_text_first_mp_award`, `ptr_text_second_mp_award` (`src/game/bondview.h:2307`) | `s32` | `char *` | 4 | 0 |
| 4 | Language banks | `g_LangBanks[45]` (`src/game/language.c:12`); 2 of its 12 places (the slot reads in `langGet`) move to Task 8 | `s32` | `u8 *` | 12 | 0 |
| 5 | Monitor scripts | `u32 monAnim*[]` (`src/game/propobj.c:2523` on), `MONJUMPTO`/`MONJUMPCHANCE` (`src/game/chrai.h:77,84`); read back at `propobj.c:7444-7480` | `u32` elements | `uintptr_t` elements | 121 | 0 |
| 5 | Animation offsets | `PTR_ANIM_x + (s32)&ptr_animation_table->data` | integer maths | one helper macro yielding `ModelAnimation *` | 78 | 8 |
| 5 | `struck_anim` tables | field `src/game/chr.h:34`, tables `chr.c:530` on, relocation `initactorpropstuff.c:180-200` | pointer field holding an offset | builder's choice; see Task 5 | 33 | 0 |
| 5 | Animation pointer tables | `animation_table_ptrs1/2` (`src/game/initanitable.h:31`); 1 of its 11 places (`initanitable.c:249`, which writes `ModelAnimation.address`) moves to Task 9 | `s32[]` | `intptr_t[]` or pointers; see Task 5 | 11 | 0 |
| 6 | Image bank offset | `globalbank_rdram_offset` (`src/game/image_bank.c:21`) and its `base + (u32)&sym` uses | `s32` | `intptr_t` delta | 87 | 6 |
| 6 | Model-file relocation | locals in `src/game/objecthandler_2.c:120-200` | `s32`/`u32` | `u8 *` / `uintptr_t` | 29 | 0 |
| 6 | Texture index | `sImageTableEntry.index` (`src/bondtypes.h:3980`), written by `texLoad` | `u32` | `uintptr_t` | 5 | 1 |
| 6 | Smaller | `texSetBitstring(s32 pos)`, `vtxstore_allocate` (returns `s32`), `blood_decrypt.c` locals, `tconfig < 100` | | pointer types; `(uintptr_t)` compare for `tconfig` | 14 | 0 |
| 7 | bg room list, globals, macros | `bg.c` address maths; `ptr_bg_data`, `ptr_bgdata_offsets`, `gptr_stan` (`bg.c:110,79,113`); `BG_SEG_IS_OFFSET`/`BG_SEG_TO_PTR` (`bg.h:33-36`); free-list tag; `bgDebugRemoved7F0B9DE4`; `getIndexOfPORTALID` | `s32`/`u32` | `u8 *`, `uintptr_t` | 62 | 0 |
| 7 | Segment resolution | `base + (w1 & 0x00FFFFFF)` in `bg.c`, `lightfixture.c`, `propobj.c` | 32-bit casts | `(u8 *)base + offset` | 10 | 0 |
| 7 | Collision, setup, props | `stan.c` relocation; intro records walked through `(s32)`; `SETUP_PROMOTE` (`prop.c:1455`); `chrAllocate`/`ailist`; `if_actor_able_set_on_path`; `chrpropAddBulletHit` | `s32` | `u8 *`, `intptr_t`, real pointer params | 47 | 8 |
| 7 | Null tests, debug traces | `(s32)ptr != 0` in `chraction.c`; `v[i] = (f32)(u32)ptr` | 32-bit casts | `!= NULL`; `(f32)(uintptr_t)` | 31 | 3 |
| 8 | Segment sizes, alignment | `(u32)&_xEnd - (u32)&_xStart`; `romptr_t` (`src/bondtypes.h:70`); `ALIGN64_V2`, `ALIGN16_a` (`src/macro.h:10,15`); `((u32)p + N) & ~N` | `u32` | `uintptr_t` | 52 | 0 |
| 8 | ROM and address plumbing | `romCopyAligned` (`src/ramrom.h:7`); `osPiStartDma`/`osPiRawStartDma` `devAddr`; `osVirtualToPhysical`; K0 macros (`include/PR/os.h:877-880`, `R4300.h:78`); `port/src/libultra_shim.c` | `s32`/`u32` | `uintptr_t` | 25 | 1 |
| 8 | Memory | `mempCheckMemflagTokens`, `mempStart`, `bankstarts`, `mempAllocBytes` (`src/memp.c`); `memaRealloc(s32 addr)` and `memaspace.addr` (`src/mema.c`) | `s32`/`u32` | `u8 *`; `intptr_t` where ordering is signed | 10 | 0 |
| 8 | Boot, buffers, video | init segment helpers (`src/boot.h:6-11`); `GfxInfo.cfb`; RDP buffer globals; `resolution`, `ptr_menu_videobuffer`, `z_buffer`; title buffers; demo buffers; Indy commands | `s32`/`u32` | typed pointers | 45 | 1 |
| 8 | Audio | `music.c` locals; `alBnkfNew` (`port/src/audio_stubs.c`); `memin`; `ALSetParam` values; `OSMesg` ints; `audio_pc.c` (8) and `glad.c` (1) from the video configuration | mixed | see Task 8 | 43 | 0 |
| 8 | Port state, arenas, zlib, pointer differences | `StateHeader`/`StateRegion` (`port/src/port_state.c`); `port/src/pcsched.c` hints; `zlib.c:348-477`; pointer differences through 32-bit casts | `u32` | `uintptr_t`, `ptrdiff_t` | 70 | 6 |
| 9 | Strict batch | everything left in `bondview2.c`, `model.c`, `gunfire.c`; declarations defined there, with their users in other files (font globals, `setFontTables`, player matrix fields, `Model` fields and callbacks, model render helpers, `start_stan`, `field_A48`, the two `model.h` prototypes); the three C++ items | | | whatever remains: 167 in the three files, about 20 in their users, plus places revealed on the way | 167 |

The 48 one-off sites (19 in strict files) are in `site_tail.md`; each is fixed
in the task that owns its file.

## Global Constraints

- **The 32-bit program does not change.** Before every commit,
  `python3 scripts/port/compare_win32.py commits HEAD WORKTREE` exits 0. Where
  it exits 1, the commit is acceptable only if every changed function is one
  whose source the commit edited, or one that differs because a declaration it
  uses changed in the same commit, and the change is explained by name in the
  commit message and FORK-089 (find the functions by disassembling the two
  executables the tool leaves in `build/lp64/compare` with
  `i686-w64-mingw32-objdump -d` and comparing by symbol). The accepted kinds
  of difference, each established by reading both versions instruction by
  instruction, are: block order and branch direction; register and stack-slot
  choice; an equivalent comparison; a load moved across a call or test that
  cannot affect it; jump threading that duplicates code, when every path is
  shown to make the same calls and stores; a loop written with an indexed
  address instead of a stepped pointer, with the same accesses in the same
  order; two stores to distinct non-volatile objects reordered, with no read
  of either and no call between them; a load moved ahead of a store to
  another, non-overlapping field of the same object, with every address
  keeping its order of reads and writes. Otherwise change the
  fix: a type that keeps the 32-bit arithmetic identical (`intptr_t` for a
  signed quantity, `uintptr_t` for an unsigned one) is preferred to a pointer
  type that does not.
- **Fix declarations, not call sites.** A field, parameter, return type,
  global or table element that carries a pointer gets a pointer type, or
  `intptr_t`/`uintptr_t` when it also carries offsets or tags. A cast is the
  last resort.
- **No laundering.** A truncating round trip such as `(u32)(uintptr_t)p` or
  `(s32)(intptr_t)p` silences the compiler without fixing anything. A new one
  is allowed only where the true fix belongs to a later milestone (a ROM
  converter, the suspend-point redesign); it carries a comment naming that
  milestone and an entry in the allowlist Task 1 creates.
- **Signedness is chosen per declaration.** A quantity compared, ordered,
  shifted or divided as signed stays signed. The sites in "Behaviour-sensitive
  sites" below are handled exactly as that list says.
- **Upstream-owned files get small mechanical edits**: no reformatting, no
  renaming, no moved code. FORK-089 records each file touched.
- **A declaration whose definition lives in a strict-tier file is changed only
  in Task 9**, together with that definition. Changing the header alone would
  break the 32-bit build with conflicting types. Strict-tier files: 
  `src/game/gunfire.c`, `src/game/model.c`, `src/game/bondview2.c`, everything
  under `port/fast3d/`, and `port/src/port_arm_mesh.c`, `port_native_arms.c`,
  `port_grip_mesh.c`, `port_wardrobe.c`, `ads_hands.c`, `port_knife_ads.c`,
  `port_graphics.c`, `port_shiny.c`, `port_weapon_images.c`,
  `port_hud_weapons.c`, `joy_port.c`, `scripts/port/hand_gallery.py` and its
  template.
- **The recorded count falls with every fix task** (Tasks 3 to 9; Task 10
  removed the count and its baseline). The commit runs
  `lp64_diagnostics.py update` and includes the baseline. A file's count may
  rise only because a declaration fixed in this commit exposed a caller that a
  later task fixes; the commit message names the file and the later task.
- **Every commit updates `FORK_CHANGES.md`** (FORK-089): what changed, the
  count before and after, and what ran.
- No ROM-derived data in Git. Python uses the standard library only. New files
  stay under 500 lines. Shell scripts start with `#!/usr/bin/env bash` and
  `set -euo pipefail`. Build output goes under `build/lp64/`.
- Delivery is direct to `main`, one push per task after its review is clean,
  except Tasks 9 and 10, which share the branch `lp64/types-strict`. Never use
  `git commit --no-verify`.
- Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

### Task check

Every fix task (3 to 9) ended by running the task check of its time, and
recorded the results in FORK-089; until Task 10, step 1 recorded the count with
`lp64_diagnostics.py update` against a baseline file. Task 10 replaced the
count with the compile gate, and from then on the check is:

```bash
# 1. The 64-bit compile gate, in the x86_64 container (the CI runner's
#    architecture); run it in the aarch64 container too for a change to C, C++
#    or the build, wiping the two trees first when switching containers.
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c '
  cmake -S . -B build/lp64/gate -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate --target ge_buildinfo &&
  cmake -S . -B build/lp64/gate-video -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate-video --target ge_buildinfo &&
  python3 scripts/port/lp64_diagnostics.py gate build/lp64/gate build/lp64/gate-video'

# 2. The 32-bit game still builds and plays (i686, headless, with the ROM).
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c 'cmake -S . -B build/lp64/ref-i686-stock -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON && cmake --build build/lp64/ref-i686-stock -j"$(nproc)"'
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ref-i686-stock/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 2 --output build/lp64/ref-i686-stock/smoke.json

# 3. The shipped Windows program is unchanged (before committing; WORKTREE
#    includes new files that are not yet added).
python3 scripts/port/compare_win32.py commits HEAD WORKTREE
```

Both 64-bit trees are configured and only their generated header is built: the
gate compiles syntax-only and needs nothing more. The video tree uses the
container's 64-bit `libsdl2-dev` (CI installs it through the tests profile of
`scripts/port/ci_dependencies.py`).

Expected: step 1 prints `0 failed` for both trees and exits 0; step 2 (which
is also a 32-bit Linux compile and link check) exits 0 with 20 of 20
missions `PASS`; step 3 exits 0. Also run every harness line of the CI tests
step (the `bash scripts/port/test_*.sh` and `python3 scripts/port/test_*.py`
lines of the step `Test gameplay, cache and release tooling without a ROM` in
`.github/workflows/windows-release.yml`), in the i686 container where a line
needs `-m32`, because a header change reaches harnesses that never name the
changed file. The browser build is checked
by CI after the push; a red browser build is the task's to fix.

### Behaviour-sensitive sites

These must not be widened by rote. Each is handled as stated, in the task that
owns it.

- `memaspace.addr` (`src/mema.c:44`) and the signed address ordering at
  `mema.c:199,211`: becomes `intptr_t`, not a pointer or `uintptr_t`, so the
  32-bit comparisons stay signed. (Task 8)
- `globalbank_rdram_offset` (`image_bank.c:21`) and `delta` in `stan.c:3544`:
  signed byte deltas, `intptr_t`. (Tasks 6, 7)
- `prop.c:2167` tests a 32-bit sum of two pointers for non-zero: keep the
  arithmetic in `uintptr_t`, same expression. (Task 7)
- `g_LangBanks`, the text locals and the bg globals become **byte** pointers
  (`u8 *`/`char *`), because their arithmetic is in bytes. (Tasks 4, 7)
- Monitor scripts: when the element type becomes `uintptr_t`, the interpreter's
  cursor at `propobj.c:7444-7480` must step by element, not by 4. (Task 5)
- Unions of a pointer and an integer are left with both members and reviewed:
  `pname` (`src/bondtypes.h:4028`), the `bg.c:4326` union, the
  `initactorpropstuff.c:88` union. Give the integer member `uintptr_t` width
  only if a member is read through the other. (Tasks 5, 7)
- In-place promotions of ROM data (`SETUP_PROMOTE`, `ailist` IDs promoted at
  `prop.c:2550,2572`, `struck_anim` offsets, animation pointer tables,
  `alBnkfNew`): this milestone makes them compile; the converters of LP64-004
  replace them. Make no layout decision here beyond the field's type.
- Sentinel comparisons keep their operands' current signedness on 32-bit:
  texture ID `< MAX_TEXTURES` (`gun.c:1025,1034`, `objecthandler_2.c:411`,
  `image.c:2758`, `front.c:11795-11813,13173`), `tconfig < 100`
  (`propobj.c:7632`), `< PORTMAX` (`bg.c:1212`), `address & 0x80000000`
  (`model.c:5915`), `!= (s32 *)1` (`initanitable.c:239,248`),
  `BG_SEG_IS_OFFSET` (`bg.h:32`), the mask `0x05000000` (`model.c:5975`),
  `== 0x0E000000` (`lightfixture.c:295`), `osScAddClient` with `next = 1`
  (`audi.c:438`). Task 10 asserts in the layout probe that each field involved
  is pointer-sized. Their behaviour at 64 bits is LP64-003's to test.

## Review Focus

1. A type fix silently changes the 32-bit program (a signed comparison of
   addresses becomes unsigned). Expected: the Windows comparison differs and
   the commit is refused or explained. (Task check, step 3; Task 8 for `mema`.)
2. A diagnostic is hidden by a truncating round-trip cast instead of fixed.
   Expected: the cast allowlist test fails. (Task 1, Step 3.)
3. A header declaration is changed on `main` while its definition sits in a
   strict-tier file, breaking the 32-bit build. Expected: the i686 build in the
   task check fails before the commit. (Task check, step 2.)
4. A new prototype reveals hidden diagnostics and the count is "fixed" by
   re-recording a higher number. Expected, until Task 10: `update` refuses a
   higher total without a stated reason (Task 1, Step 2). From Task 10 there
   is no count to re-record: the gate fails on any conversion.
5. The gate passes because `-w` or a `-Wno-error` still switches it off.
   Expected: a canary source with each conversion fails to compile with the
   64-bit flags. (Task 10, Step 1.)

---

### Task 1: Tooling for the milestone

**Files:**
- Modify: `Dockerfile`, `scripts/port/lp64-arm64.Dockerfile` (64-bit
  `libsdl2-dev`)
- Modify: `scripts/port/lp64_diagnostics.py`, `scripts/port/test_lp64_diagnostics.py`,
  `scripts/port/lp64_diagnostics_baseline.json`
- Create: `scripts/port/lp64_casts.py`, `scripts/port/test_lp64_casts.py`,
  `scripts/port/lp64_casts_allowed.txt`
- Modify: `.github/workflows/windows-release.yml` (two lines),
  `FORK_CHANGES.md`, `scripts/port/native_64bit_plan.md` (LP64-002 row)

**Interfaces:**
- Produces, in `lp64_diagnostics.py`:
  - `--compile-commands` may be given more than once. Entries are merged; a
    source file listed by several trees is measured once, with the first
    tree's command.
  - `sites --messages [FILE...]`: one line per place,
    `file:line: kind: message | source text`, the message and the quoted
    source line as GCC printed them. For the four `#line`
    files the line is the file's physical line and the text is read from it.
  - `update` exits 2 and writes nothing when the new total exceeds the
    recorded total, unless `--allow-rise REASON` is given; the reason is
    printed.
- Produces `lp64_casts.py`:
  - `find_casts(text: str) -> list[int]`: line numbers of truncating
    round-trip casts, matching `(u32|s32|int|unsigned|unsigned int|u16|s16)`
    cast applied directly to a `(uintptr_t|intptr_t)` cast.
  - CLI `check` (exit 1 unless every file's count equals its allowlist line:
    more casts, or fewer, or a listed file that is gone, each listing the
    sites and what to change) and `list`. It scans tracked `*.c`, `*.h`, `*.cpp`, `*.inl` under `src/`,
    `port/` and `include/`.
  - `lp64_casts_allowed.txt`: one `path<TAB>count<TAB>reason` line per file.

- [ ] **Step 1: Give both images 64-bit SDL2 and rebuild them**

Add `libsdl2-dev` (the native architecture's package, `--no-install-recommends`)
to both Dockerfiles, then `python3 scripts/port/lp64_env.py build-images`.
Verify in each container: `cmake -S . -B build/lp64/t1-video64 -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DCMAKE_BUILD_TYPE=Debug`
configures, and `port/src/video.c` compiles from its recorded command. The
i686 SDL2 under `/home/dev/sdl2-linux-i686` must still be there.

- [ ] **Step 2: Write the failing tests for the diagnostics tool, then implement**

Tests, in `test_lp64_diagnostics.py`:
1. Two compile-command files naming the same source yield one measurement,
   using the first file's command.
2. `sites --messages` prints the message and source text for a place.
3. `update` with a higher total exits 2 and leaves the baseline file
   byte-identical; with `--allow-rise "reason"` it writes.

- [ ] **Step 3: Write the failing tests for the cast allowlist, then implement**

Tests, in `test_lp64_casts.py`:
1. `find_casts("x = (u32)(uintptr_t)p;")` returns `[1]`;
   `find_casts("x = (uintptr_t)p;")` returns `[]`;
   `(s32) (intptr_t) p` with spaces is found.
2. `check` exits 1 naming a file whose count exceeds its allowlist line, and 0
   when counts are equal or lower.
3. A file absent from the allowlist with one cast fails.

Record today's sites in `lp64_casts_allowed.txt` with the reason
`existing before LP64-002`. Add `python3 scripts/port/test_lp64_casts.py` and
`python3 scripts/port/lp64_casts.py check` to the CI step, after
`python3 scripts/port/test_lp64_diagnostics.py`.

- [ ] **Step 4: Re-record the baseline over both configurations**

Run step 1 of the task check with `update --allow-rise "video configuration added"`.
Expected: about 1,441 places (1,432 plus 8 in `port/src/audio_pc.c` and 1 in
`port/fast3d/glad/glad.c` or wherever `glad.c` lives). Record the actual
figures. CI keeps counting the headless tree only, which can only read lower
than the baseline; say so in FORK-089.

- [ ] **Step 5: Run the rest of the task check, commit**

```bash
git add Dockerfile scripts/port/lp64-arm64.Dockerfile scripts/port/lp64_diagnostics.py scripts/port/test_lp64_diagnostics.py scripts/port/lp64_diagnostics_baseline.json scripts/port/lp64_casts.py scripts/port/test_lp64_casts.py scripts/port/lp64_casts_allowed.txt .github/workflows/windows-release.yml FORK_CHANGES.md scripts/port/native_64bit_plan.md
git commit -m "Count the video configuration and guard against truncating casts"
```

---

### Task 2: Let a clone that cannot render commit strict-tier work on a branch

**Runs on the Windows laptop**, because it changes `scripts/port/hand_gallery.py`,
a strict-tier file. It can run while Tasks 3 to 8 proceed on the Mac. It needs
the owner's agreement first: see "Decision for the owner" at the end.

**Files:**
- Modify: `scripts/port/hand_gallery.py`, `scripts/port/test_hand_gallery.py`,
  `scripts/port/hand_gallery.md`, `AGENTS.md` ("Render review gallery"),
  `FORK_CHANGES.md` (FORK-022), `scripts/port/native_64bit_plan.md`
  ("Gallery rule: two tiers")
- Regenerated: `review/hand-gallery/`

**Interfaces:**
- Produces, in `hand_gallery.py`:
  - `current_branch(root=ROOT) -> str | None`: the checked-out branch's short
    name, `None` on a detached HEAD.
  - `verify(root=ROOT, staged=False, allow_pending=False, allow_stale=False)`:
    with `allow_stale`, a strict mismatch does not raise; every other check
    (schema, determinism flags, coverage, image hashes, HTML) still runs.
  - `pre_commit`: when the staged gallery is stale for strict inputs,
    `defer_enabled(root)` is true and `current_branch(root)` is neither `None`
    nor `main`, it verifies with `allow_stale=True`, prints
    `Hand gallery is STALE on branch <name>: strict inputs changed. Regenerate on a machine that can render before this reaches main.`
    and returns. In every other case it behaves as today.
  - The `verify` command line is unchanged, so CI and `main` still fail on a
    strict mismatch.

- [ ] **Step 1: Write the failing tests**

In temporary repositories, as the existing hook tests do:
1. Defer set, branch `work`, a strict-tier file changed and staged: the hook
   returns 0 and prints the message with the branch name.
2. The same on `main`: the hook fails as today.
3. The same on a detached HEAD: fails.
4. Defer unset, branch `work`: behaves as today (attempts generation).
5. Defer set, branch `work`, strict change and a corrupted image: fails.
6. `verify` from the command line with a strict mismatch still exits 1 on a
   branch.

- [ ] **Step 2: Implement, run `python scripts/port/test_hand_gallery.py`**
- [ ] **Step 3: Update the four documents; regenerate; review; commit; push**

```bash
python scripts/port/hand_gallery.py generate
python scripts/port/hand_gallery.py verify
git add scripts/port/hand_gallery.py scripts/port/test_hand_gallery.py scripts/port/hand_gallery.md AGENTS.md FORK_CHANGES.md scripts/port/native_64bit_plan.md review/hand-gallery
git commit -m "Let a deferring clone commit strict-tier work on a branch"
git pull --rebase && git push origin main
```

Expected: no image changes. This regeneration also clears whatever is pending
on `main` at that moment.

---

### Task 3: Prototypes for pointer-returning functions

**Files:** the non-strict callers and headers `sites --messages` lists for
this cluster; `scripts/port/lp64_diagnostics_baseline.json`; `FORK_CHANGES.md`.

**Interfaces:**
- Fixed behaviour: each function is declared by including the header that
  already declares it. A prototype is written by hand only when no header has
  one, and then in the header that owns the function's file, matching the
  definition exactly. No `extern` copies in `.c` files.
- Out of this task: callers in `bondview2.c` and `gunfire.c` (34 places), and
  `dyn.h` in `bondview2.c`, which must land with the player matrix fields
  (Task 9).

- [ ] **Step 1: List the functions**

`python3 scripts/port/lp64_diagnostics.py ... sites --messages` filtered to
`implicit declaration` and to `makes pointer from integer` on a call's result.
Write the function list and each one's declaring header into FORK-089.

- [ ] **Step 2: Add the includes; recount**

Expected: the 68 non-strict places go, and at least 30 hidden ones appear
(17 in `image_bank.c:238-254`, others in `chrai.c`, `front.c`, `glass.c`,
`initanitable.c`, `ramromreplay.c`, `title.c`). Fix every newly visible place
that is in a non-strict file and belongs to no later task's cluster; leave the
rest to their tasks and name them. The total must end lower than it started.

- [ ] **Step 3: Check the 32-bit program with extra care**

A prototype changes how arguments are passed (a `float` argument is no longer
promoted to `double`; `char` and `short` are no longer promoted). Run the task
check. For any function whose callers' code changes, the commit message states
the parameter that caused it and why the new call is the correct one. If that
cannot be shown for a function, leave it out of the commit and report it as
blocked, with the function, the callers and the difference: an implicit `int`
result is truly truncated at 64 bits, so there is no cast that fixes it.

- [ ] **Step 4: Commit**

```bash
git commit -m "Declare the pointer-returning functions that were called without a prototype"
```

---

### Task 4: Fonts and text carried as integers

**Files:** `src/game/front.c`, `front.h`, `options.c`, `mpmenu.c`,
`language.c`, `language.h`, `bondview.h`; `port/src/port_headshots_menu.c`,
`port_mission_times.c`, `port_reinforcements_menu.c`, `port_startup.c`;
baseline; `FORK_CHANGES.md`.

**Interfaces:**
- `frontPrintText` parameters 5 and 6 and `display_aligned_white_text_to_screen`
  parameters 7 and 8 become `struct fontchar *` and `struct font *` (match the
  types `textRender` already takes). The four `extern` copies in `port/src/`
  are replaced by including the header.
- `g_LangBanks` becomes `u8 *[45]`; text locals become `char *`; the two award
  fields become `char *`.
- Out of this task: `copy_1stfonttable`, `copy_2ndfonttable` and
  `setFontTables` (defined in `bondview2.c`; Task 9). Their callers may show
  new diagnostics in `bondview2.c` until then; name them in the commit.

- [ ] **Step 1: Change the declarations and the locals; recount**

Expected: about 429 places go (252 + 102 + 53 + 6 + 4 + 12).

- [ ] **Step 2: Run the task check; commit**

```bash
git commit -m "Type font and text pointers as pointers"
```

---

### Task 5: Monitor scripts and animation tables

**Files:** `src/game/propobj.c`, `chrai.h`, `chr.h`, `chr.c`,
`initactorpropstuff.c`, `initanitable.c`, `initanitable.h`, the non-strict
users of the animation offset idiom, `assets/animationtable_data.h` only if
the helper macro belongs there; baseline; `FORK_CHANGES.md`.

**Interfaces:**
- Monitor script tables become `uintptr_t` arrays; `MONJUMPTO` and
  `MONJUMPCHANCE` store `(uintptr_t)` of the target; the interpreter reads
  jump operands back with one cast and advances by element.
- One helper macro replaces the idiom
  `PTR_ANIM_x + (s32)&ptr_animation_table->data`. It takes the offset and
  yields `ModelAnimation *`, computed as a byte offset from
  `&ptr_animation_table->data`. Name and header are the builder's choice;
  state them in FORK-089.
- `struck_anim` and `animation_table_ptrs1/2` hold offsets that are rewritten
  to pointers in place. Give the storage a type that holds both without a
  diagnostic (`intptr_t` elements, or a pointer field initialised through the
  helper's offset form) and keep the in-place relocation; LP64-004 replaces
  it.

- [ ] **Step 1: Write a failing test for the monitor script interpreter's stride**

`scripts/port/test_monitor_script.sh` (new, modelled on
`scripts/port/test_states.sh`): compiles a small driver, at 32 and at 64 bits,
that builds a three-entry script with one `MONJUMPTO` whose target is the
third entry, steps the interpreter's read-and-jump logic over it, and asserts
it lands on the third entry. If the interpreter's logic cannot be reached
without pulling in the whole of `propobj.c`, extract only the operand read and
the cursor advance into two small `static inline` functions in `chrai.h` used
by both `propobj.c` and the test, and say so in FORK-089. Add the script to
the CI step.

- [ ] **Step 2: Change the tables and macros; recount**

Expected: about 235 places go (121 + 70 + 33 + 11), including all 59 hard
errors, so `errors_by_file` becomes empty and `src/game/propobj.c` compiles at
64 bits.

- [ ] **Step 3: Run the task check; commit**

```bash
git commit -m "Hold monitor script jumps and animation offsets in pointer-sized storage"
```

---

### Task 6: Image bank, textures and model-file relocation

**Files:** `src/game/image_bank.c`, `image_bank.h`, `image.c`,
`objecthandler_2.c`, `vtxstore.c`, `vtxstore.h`, `blood_decrypt.c`,
`propobj.c` (the `tconfig` test), `src/bondtypes.h`; baseline;
`FORK_CHANGES.md`.

**Interfaces:**
- `globalbank_rdram_offset` becomes `intptr_t`; uses become
  `(u8 *)&sym + globalbank_rdram_offset` or the reverse subtraction, in bytes.
- `sImageTableEntry.index` becomes `uintptr_t`; comparisons with
  `MAX_TEXTURES` stay unsigned.
- `vtxstore_allocate` returns `Vertex *`; `texSetBitstring` takes `u8 *`.
- Out of this task: `modelNodeReplaceGdl` and `sub_GAME_7F075A90`
  (`model.h:111,122`, defined in `model.c`; Task 9), and the 6 image-bank
  places in `gunfire.c`.

- [ ] **Step 1: Change the declarations; recount** (expected: about 128 go)
- [ ] **Step 2: Run the task check; commit**

```bash
git commit -m "Type the image bank delta, texture slots and model relocation"
```

---

### Task 7: Backgrounds, collision, setup and props

**Files:** `src/game/bg.c`, `bg.h`, `lightfixture.c`, `stan.c`, `prop.c`,
`chr.c`, `chr.h`, `chrai.c`, `chraction.c`, `chraction.h`, `chrprop.c`,
`bondview_r.c`, `initpathtablesomething.c`, `cseq.c` (debug traces);
baseline; `FORK_CHANGES.md`; `scripts/port/native_64bit_plan.md` (hazard
inventory).

**Interfaces:**
- `ptr_bg_data`, `ptr_bgdata_offsets`, `gptr_stan` become `u8 *`.
- `BG_SEG_IS_OFFSET` and `BG_SEG_TO_PTR` do their arithmetic in `uintptr_t`.
- Address arithmetic written through `(s32)` or `(u32)` becomes `(u8 *)`
  arithmetic or a pointer difference.
- `(s32)ptr != 0` becomes `ptr != NULL`; debug float traces cast through
  `uintptr_t` (truncation in a debug float is harmless and not laundering: add
  those lines to the allowlist with that reason only if the cast pattern
  matches the checker).
- Record in the spec's hazard inventory, under "Hard-coded sizes":
  `bg_room_data` (`bg.h:152`) is addressed with the literal stride
  `room * 24` and offset `+ 12` at `bg.c:4453,4657,6392,6425`. The literals
  `24` and `+ 12` are not changed in this milestone; the casts around them
  were retyped in Task 8 and the lines carry an `LP64-004` comment.

- [ ] **Step 1: Change the declarations and idioms; recount** (expected: about 139 go)
- [ ] **Step 2: Run the task check; commit**

```bash
git commit -m "Do background, collision and setup address arithmetic in pointers"
```

---

### Task 8: Address plumbing, memory, boot, audio and the port layer

**Files:** `src/ramrom.c`, `ramrom.h`, `memp.c`, `memp.h`, `mema.c`, `mema.h`,
`macro.h`, `bondtypes.h`, `boot.h`, `init.c`, `music.c`, `audi.c`, `sched.c`,
`src/game/rsp.c`, `rsp.h`, `title.c`, `title.h`, `ramromreplay.c`,
`ramromreplay.h`, `indy_commands.c`, `indy_comms.c`, `zlib.c`, `viewport.c`,
`bondview.c`, `front.h`, `lv.c`, `textrelated.c`, `language.c`, the libultra audio files;
`include/PR/os.h`, `R4300.h`, `libaudio.h`; `port/src/libultra_shim.c`,
`n64_stubs.c`, `audio_stubs.c`, `audio_pc.c`, `port_state.c`, `pcsched.c`,
`glad.c`; baseline; allowlist; `FORK_CHANGES.md`.

**Interfaces:**
- `osPiStartDma`/`osPiRawStartDma` `devAddr`, `osVirtualToPhysical`'s return
  and the K0 macros become `uintptr_t`. `romCopyAligned` returns a pointer.
- `typedef u32 romptr_t` becomes `uintptr_t`. `ALIGN64_V2` and `ALIGN16_a`
  work in `uintptr_t`.
- `memaspace.addr`, `memaRealloc`'s `addr` and the locals compared with them
  become `intptr_t` (see "Behaviour-sensitive sites").
- Integers carried through `void *` parameters (`ALSetParam` values, `OSMesg`
  messages) convert through `intptr_t` at the call and back at the use.
- Sites whose true fix is a later milestone get a truncating cast, a comment
  and an allowlist line: `alBnkfNew`'s in-place bank relocation
  (`/* LP64-004: audio bank converter */`), the suspend image's
  `StateHeader`/`StateRegion` addresses
  (`/* LP64-003: suspend points are unavailable on 64-bit */`) and the text
  bank slot reads in `langGet` (`src/game/language.c`, the two places
  Task 4 left; `/* LP64-004: text bank converter */`). Nothing else
  in this task may add to the allowlist.
- This task is large. If its diff passes about 600 changed lines, split it at
  the audio files into two commits with two task checks, and say so in the
  report.

- [ ] **Step 1: Write a failing test that `mema` ordering stays signed**

Extend the nearest existing `-m32` harness that includes `src/mema.c` (find it
with `grep -l mema scripts/port/test_*.sh`; if none does, create
`scripts/port/test_mema_order.sh` modelled on `test_states.sh`): two free
blocks whose addresses differ in sign when read as 32-bit signed must sort in
the order the current code gives. Run it before the change (pass), make it
fail by temporarily typing the field unsigned, restore, then make the real
change and see it pass. If the ordering cannot be exercised with made-up
addresses because the code dereferences them, say so in the report and rely
on the Windows comparison for this site.

- [ ] **Step 2: Change the declarations; recount** (expected: about 237 go)
- [ ] **Step 3: Run the task check; commit**

```bash
git commit -m "Carry addresses in pointer-sized types through ROM, memory and audio"
```

---

### Task 9: The strict batch

**Requires Task 2 on `main`.** Work on a branch: `git switch -c lp64/types-strict`
from current `main`. The hook prints the STALE-on-branch notice and passes.

**Files:** `src/game/bondview2.c`, `model.c`, `gunfire.c`;
`src/game/bondview_internal.h`, `bondview.h`, `bondview.c`,
`initBondDATA.h`, `model.h`, `src/bondtypes.h` (the `Model` fields),
`port/fast3d/system.h`, `port/fast3d/gfx_pc.cpp`; `CMakeLists.txt` only for
the 64-bit include order if `gfx_shiny.cpp` needs it; baseline;
`FORK_CHANGES.md`. Task 8 left these places outside the three strict files,
each depending on a declaration or caller in them (counts after Task 8):
`src/game/bg.c` 3 (706, 747, 837; its line 1833 counts at `gbi.h:2741`), `explosion.c` 2 (959, 1761),
`glass.c` 1 (294), `glass2.c` 1 (702), `sky.c` 1 (1752), `chr.c` 1 (1701),
`chr_b.c` 2 (72, 116), `objecthandler_2.c` 5 (138, 182, 355, 373 twice),
`initanitable.c` 1 (249); `include/PR/os.h` 3 (877, 880 twice) and
`include/PR/gbi.h` 1 (2741), reported at the header line; `port/src/port_sight.c`
2 (26, 56); `src/game/gun.h`, `glass2.c` and `options.c` for the declarations
below. `port/fast3d/glad/glad.c` (its one `strstr` place) counted 0 after
Task 3's prototypes and stays in the 64-bit video configuration's count.

**Interfaces:**
- Font globals `copy_1stfonttable`/`copy_2ndfonttable` and `setFontTables`
  take the font pointer types of Task 4.
- Player matrix fields `field_5C/60/64/68` (`initBondDATA.h:29-32`) and
  `field_10E0` with its accessors become `Mtx *`/`Mtxf *`; `dyn.h` is included
  in `bondview2.c` in the same commit.
- `Model.unk34/38/64/68` become `u8 *`; `Model.animflipfunc` and `unka0`
  become function-pointer types matching their callees; the model render
  helpers take `struct ModelRenderData *`; `loadAnimationFrame` returns a
  pointer; `modelNodeReplaceGdl` and `sub_GAME_7F075A90` take pointer or
  `uintptr_t` parameters; the model loader's base-address arithmetic is in
  `uintptr_t`/`u8 *`, keeping the mask `0x05000000` and the test
  `& 0x80000000` as they are.
- `field_A48` becomes `ALSoundState *`; `start_stan` becomes `StandTile *`.
- `ModelAnimation.address` (`src/bondtypes.h`), written at
  `initanitable.c:249` and read by `loadAnimationFrame` (`model.c:5915-5927`),
  is widened together with that reader.
- `field_10E0` (`bondview.h:1158`, set by `set_BONDdata_field_10E0` from
  `bondview2.c`, read through `get_BONDdata_field_10E0`): its `BONDDATAMTX`
  users are `bg.c` 706, 747, 837, `explosion.c` 959 and 1761, `glass.c:294`,
  `glass2.c:702` and `sky.c:1752`.
- `field_10E4` (`bondview.h:1159`, set by `sub_GAME_7F078464` from
  `bondview2.c`), read through `sub_GAME_7F078474` into `gSPLookAt`, reaches
  `include/PR/gbi.h:2741` from `bg.c:1833` (and from `gunfire.c` and
  `port_shiny.c`).
- `include/PR/os.h:877` (`OS_K0_TO_PHYSICAL`, now `uintptr_t`):
  `bondview2.c` 5014 and 5031 pass it to `sub_GAME_7F0A3B40(Gfx *, s32 *)`
  (`glass2.c:447`, prototype at `options.c:386`); the same header place also
  covers six `options.c` calls and `title.c:156`, cast to the parameter type in
  Task 8, so the place goes only when `bondview2.c`'s calls are fixed.
  `os.h:880` (`OS_PHYSICAL_TO_K0`): `bondview2.c` 12259 and 12270 pass a pointer
  to `buildGaugeBarDL`'s integer parameter (`glass2.c`), and `bondview2.c:12087`
  passes `(u32) perspmtx` (the int-to-pointer place).
- `port/src/port_sight.c` 26 and 56: `portDrawTarget(s32 *gdl)` and
  `portHudGunSight(s32 *gdl)` (`port/include/port_hud.h:40`) keep a `Gfx *` in
  the `s32` that `gunDrawSight(s32 *gdl)` (`gun.h:341`, `gunfire.c`) passes;
  they change together.
- `chr.c:1701` (`sub_GAME_7F06FF5C`), `chr_b.c` 72 and 116
  (`modelAttachHead`), `objecthandler_2.c` 138, 182, 355 and 373
  (`modelNodeReplaceGdl`, `sub_GAME_7F075A90`): parameters of `model.c`
  functions.
- `port/fast3d/system.h:12`: `s64` matches the game's `long long` type, so the
  two headers agree at both widths. `gfx_pc.cpp:8558`: an explicit conversion
  that states the intended width.
- `gfx_shiny.cpp` must compile in a 64-bit video configuration in the
  containers. Today it fails there at both widths, while CI's `linux` job
  compiles it at 32 bits. Find why `<sched.h>` resolves to `src/sched.h` in
  the container and not in CI, and fix it in the 64-bit include order, not in
  the source, unless the source is the only place. The 32-bit compile commands must not
  change (`test_pointer_bits.sh` style check: default `compile_commands.json`
  identical before and after).
- The hand and arm contracts in `scripts/port/arm_rendering.md` apply: no
  change to hand vertices, UVs, colours, materials or render setup. These are
  type changes only.

- [ ] **Step 1: `model.c` and its declarations; task check; commit**
- [ ] **Step 2: `bondview2.c` and its declarations; task check; commit**
- [ ] **Step 3: `gunfire.c`; task check; commit**
- [ ] **Step 4: The three C++ items; task check; commit**

After step 4 the recorded total is 0 with an empty `errors_by_file`, in both
configurations. For each of the four commits the Windows comparison is
expected to exit 0; a difference in a strict-tier file means the gallery
images may change and must be named in the hand-off.

- [ ] **Step 5: Run the arm and wardrobe regressions**

Run the ROM-free wardrobe and ADS regressions `AGENTS.md` names
(`scripts/port/test_wardrobe.sh`, `test_ads.sh`) in the i686 container.
Expected: pass.

Do not push the branch yet; Task 10 continues on it.

---

### Task 10: The gate

On `lp64/types-strict`, after Task 9.

**Files:**
- Modify: `CMakeLists.txt` (the `GE_POINTER_BITS EQUAL 64` branch only)
- Modify: `scripts/port/lp64_diagnostics.py`, `test_lp64_diagnostics.py`;
  delete `scripts/port/lp64_diagnostics_baseline.json`
- Modify: `scripts/port/test_pointer_bits.sh`, `scripts/port/abi_probe/probe.c`
- Modify: `scripts/port/ci_dependencies.py` (tests profile),
  `.github/workflows/windows-release.yml`
- Modify: `AGENTS.md`, `FORK_CHANGES.md`, `scripts/port/native_64bit_plan.md`

**Interfaces:**
- At `GE_POINTER_BITS=64`, C files are compiled without `-w` and with
  `-Werror=int-conversion -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast`,
  plus a `-Wno-` flag for each other warning class the 64-bit build prints, so
  the 64-bit build log has no warnings. The classes and their counts go in a
  comment beside the flags. C++ flags and every 32-bit flag are unchanged.
- `lp64_diagnostics.py gate BUILD_DIR...`: for each directory (a real
  `GE_POINTER_BITS=64` configure whose generated header exists), compiles
  every C and C++ entry of its `compile_commands.json` syntax-only with the
  entry's own flags, in parallel, and exits 1 listing every failing file with
  its first error. `count`, `check`, `update` and the baseline file are
  removed; `sites` stays and runs over the same directories.
- CI: the tests profile gains 64-bit `libsdl2-dev`. The step replaces the
  configure, build and `check` lines with: the headless 32-bit configure and
  build kept as the link check; a 64-bit headless configure and a 64-bit video
  configure, each followed by building only the generated-header target; then
  `python3 scripts/port/lp64_diagnostics.py gate` on both.

- [ ] **Step 1: Write the failing canary test**

In `test_pointer_bits.sh`: configure at 64, take the compile command of any C
entry, and compile three one-line canary sources with it, one per conversion
(`int f(char *p) { return p; }`, `int g(char *p) { return (int)p; }`,
`char *h(int i) { return (char *)i; }`). Each must fail, and the output must
name its `-Werror=` flag. Before the CMake change all three compile (the test
fails); after it they are rejected.

- [ ] **Step 2: Change the 64-bit flags; make the gate command; remove the ratchet**

Tests for `gate`: a directory whose entry fails exits 1 naming the file; a
clean directory exits 0; a directory missing its generated header exits 2
saying to build that target.

- [ ] **Step 3: Assert the sentinel fields in the layout probe**

In `probe.c`, for each field in the sentinel list under "Behaviour-sensitive
sites" that is a struct member, assert `sizeof` equals `sizeof(void *)` at
both widths. Globals and locals in that list are covered by the gate.

- [ ] **Step 4: Run the gate locally at both architectures**

In the aarch64 and x86_64 containers: configure headless and video at 64,
build the generated-header target, run `gate`. Expected: exit 0 in all four.
Then run the full task check. Record durations.

- [ ] **Step 5: Update the record and commit**

`AGENTS.md`'s 64-bit paragraph describes the gate in place of the ratchet.
FORK-089 and the LP64-002 row record every task's commits, counts and runs,
and what is left for later milestones (the allowlisted casts, the hard-coded
sizes, sentinel behaviour, the `-m32` test harnesses at 64 bits, the 527
calls to functions without a prototype that do not return pointers, and the
1,780 incompatible-pointer-type places, neither of which this gate covers).

```bash
git commit -m "Make 64-bit pointer conversions a compile error and gate CI on it"
```

- [ ] **Step 6: Try the gate on the runner before it reaches `main`**

```bash
git push -u origin lp64/types-strict
gh workflow run windows-release.yml --ref lp64/types-strict
```

The publish job runs only on `main`. In the tests job's log, confirm the gate
lines ran and passed. The job itself is expected to stop at
`hand_gallery.py verify --allow-pending`, because the branch is stale for
strict inputs; that is the rule working. A gate failure on the runner is this
task's to fix.

---

### Task 11: Land through the laptop and close

- [ ] **Step 1: Hand the branch to the laptop**

On the Windows laptop:

```bash
git fetch origin && git switch lp64/types-strict
git rebase origin/main
python scripts/port/hand_gallery.py generate
python scripts/port/hand_gallery.py verify
```

Review the images. If every Windows comparison in Task 9 was identical, no
image is expected to change; otherwise review the weapons and wardrobes the
hand-off names, both hands, regular and ADS. Then:

```bash
git add review/hand-gallery
git commit -m "Regenerate the review gallery for the strict-tier type fixes"
git switch main && git pull --ff-only
git merge --ff-only lp64/types-strict && git push origin main
```

- [ ] **Step 2: Confirm on the Mac**

`git switch main && git pull --ff-only`; `hand_gallery.py verify` reports
fresh; the pushed commit's CI passes with the gate lines green.

- [ ] **Step 3: Close the record**

Set LP64-002 to Done in the status table with the CI run, the date of the
regeneration and the open items handed to LP64-003 and LP64-004. This is a
documentation-only commit and leaves the gallery fresh.

---

## What this plan does not do

- It does not make the 64-bit build link or run. LP64-003 does.
- It does not replace any in-place conversion of ROM data, change a hard-coded
  size, or resize a memory budget. Structs that hold pointers grow on 64-bit
  as a result of this milestone, and code that assumes their 32-bit size is
  wrong at 64 bits until LP64-003 and LP64-004.
- It does not gate calls to functions without prototypes that return integers,
  or incompatible pointer types. Both are recorded.
- It does not run the `-m32` test harnesses at 64 bits (verification item 10).

## Decision for the owner

The spec says strict-tier commits are "batched on a branch and merged through
a gallery regeneration on the laptop", but the hook as built refuses a strict
change on the Mac on any branch, so that branch cannot be committed here.
Task 2 resolves it by letting a deferring clone commit strict-tier work on a
branch other than `main`, with `main` and CI as strict as before. The
alternatives are: the laptop's agent makes the strict-tier edits itself from a
brief (no rule change, but about 170 places and the C++ fixes are then written
away from the Mac's tools and review); or the Mac uses a second clone without
the hook (no code change, but it sidesteps a guard rather than changing it).
Task 2 is written for the first; say so if you prefer another.
