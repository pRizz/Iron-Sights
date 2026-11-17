# LP64-003 Addresses and Memory Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-16 and accepted by the owner the same day,
with all nine recommendations under "Decisions for the owner" at the end.
Built and landed on 2026-11-16: the status table of
[native_64bit_plan.md](native_64bit_plan.md) records what was built and
verified, and `FORK_CHANGES.md` (FORK-089) each task's note, the departures
from this text and the hand-over to LP64-004. The task text below is kept as
it was accepted, with dated notes where the work found it wrong.

**Goal:** A 64-bit headless build that keeps every address whole: no game data
below 4 GB and a startup check that proves it, one host-pointer test that
relies on it, no host address carried in 32 bits outside the named ROM
conversions, and a named stop at the first asset class that has no converter
yet, without changing the 32-bit program.

**Architecture:** The measurements below show that a 64-bit executable already
links and boots through the ROM load, then crashes inside the first in-place
conversion of ROM data, and that it does so whether its arenas are pinned low
or not. So this milestone cannot be verified by "how far the game gets": every
later step is behind converters that LP64-004 owns. It is verified by four
things instead: a startup check and a boot check that runs the real 64-bit
executable to a defined stop; ROM-free tests that run at both widths; two
static checks that see what the compile gate cannot (integers narrowed from
pointer width, and calls without a prototype to functions that take
pointers); and the unchanged 32-bit executable. Work in the gallery's
strict-tier files is batched on one branch and reaches `main` through one
regeneration on the Windows laptop.

**Tech Stack:** C and C++ (GCC 13.3, Ubuntu 24.04 containers), CMake, Python 3
standard library, Bash test harnesses, Docker Desktop on Apple Silicon.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md): "Design > Addresses",
"Game code types" (sentinels, hard-coded sizes, unions, `Vertex`), "Memory
budgets", "Suspend points", verification items 1, 2, 3 and 10, milestone
LP64-003 and the hazard inventory. Read it first, then `AGENTS.md`,
`scripts/port/arm_rendering.md`, and the list headed "LP64-002 hand-off" in
`FORK_CHANGES.md` (FORK-089).

## What was measured for this plan (2026-11-16, `main` at 2afe62d)

All runs used the Mac's containers (`lp64_env.py`, GCC 13.3). aarch64 is
native; x86_64 runs under emulation, so nothing seen only there is a finding
yet (the design's rule).

- **Link.** `-DGE_POINTER_BITS=64 -DGE_HEADLESS=ON` and
  `-DGE_POINTER_BITS=64 -DGE_VIDEO=ON` both configure, compile and link, in
  the aarch64 and the x86_64 container (headless on aarch64: 5 s). The
  sanitizer configuration (`-DGE_SANITIZE=ON`, AddressSanitizer and
  UndefinedBehaviorSanitizer) links and runs on aarch64; it was not tried on
  x86_64.
- **Where a 64-bit process puts its memory.** A small probe, in both
  containers: a position-independent executable has its code, data, heap,
  `mmap` regions and stack all above 4 GB (image at `0xaaaa…` on aarch64,
  `0x5555…` on x86_64). The same program linked `-no-pie` has its image at
  `0x400000` and its heap just above it, below 4 GB. A hinted
  `mmap(0x20000000)` is honoured at 64 bits, so today's 64-bit build still
  pins its arenas low: its log says `memp arena: 64 MB at 0x20000000` and
  `portAllocHigh: 64 MB arena at 0x30000000`. The images have no `clang`,
  `gdb` or `valgrind`.
- **Boot.** The headless executable loads the ROM, inflates the compressed
  data, installs the ROM segments and preprocesses the text banks, then dies
  with signal 11 in `modelAnimReadRootMotionValue` (`src/game/model.c:941`),
  reached from `initResolveAnimGroups` in `bossInitMainthreadData`
  (`src/boss.c:284`). The crash is the same on aarch64 and x86_64. The
  sanitizer build stops one call earlier, at its first report:
  `src/game/initanitable.c:241`, a misaligned `struct anim_entry` in
  `expand_ani_table_entries`. Nothing is reported before it. Root cause: the
  animation table's 0x14-byte file headers are read through `ModelAnimation`,
  whose three address fields are 8 bytes at 64 bits. That is the animation
  class of LP64-004.
- **The 20-mission smoke.** `audit_buffers.py --mission-smoke` on the 64-bit
  headless executable: 0 of 20 pass. All 20 logs end in the same boot crash,
  before the stage is chosen.
- **Further, in a scratch copy** (a `git archive` of `main` under
  `build/lp64`, never committed; the diffs are kept with the research files):
  1. *Arenas and ROM image above 4 GB* (no `mmap` hint; the arenas landed at
     `0xffff90e00000` and `0xffff8ce00000`): the same crash. Pinning is not
     what stops the boot.
  2. *Plus the animation header kept in its file layout*, its three readers
     resolving offsets: boot passes the animation code and dies in `alBnkfNew`
     (`port/src/audio_stubs.c:261`) from `musicSeqPlayerInit`
     (`src/music.c:787`). Root cause: the audio bank is relocated in place
     through 8-byte pointer slots over 4-byte file offsets. LP64-004, audio.
  3. *Plus the game's own no-sound switch* (`g_sndBootswitchSound`): boot
     reaches `bossMainloop` and the title's `lvlStageLoad`
     (`src/game/lv.c:781`), then dies in the sound code
     (`src/snd.c:1541`, then `:1558`), which on this port does not tolerate
     that switch. Exploration stopped there: there is no way to run without
     the audio bank, and each further step needs another converter.
- **What LP64-002 already widened.** The declarations the design lists under
  "Addresses" carry whole addresses since LP64-002 Task 8, read back at
  2afe62d: `osPiStartDma` and `osPiRawStartDma` take `uintptr_t`,
  `osVirtualToPhysical` returns it, the K0 macros are identities, `romCopy`
  takes pointers. What is left of that design item is the narrowing below.
  The x86-only code of the hazard inventory does not stop a 64-bit ARM build:
  the aarch64 tree links, and its crash handler printed a backtrace.
- **The video executable** cannot run in the containers (SDL's dummy driver
  has no OpenGL: `could not open SDL window`). Only its build is measured.
- **Narrowing the gate cannot see.** The gate catches conversions between
  pointers and integers, not an integer narrowed from pointer width
  (`u32 timg = cmd->words.w1;`). Compiling both 64-bit trees with
  `-Wconversion` (sign and float conversions off) gives 1,402 warnings; 225 of
  them, in 51 files, narrow a 64-bit integer type. 111 have a `uintptr_t` or
  `intptr_t` source (18 files; 51 places in strict-tier files:
  `port_arm_mesh.c` 20, `gfx_pc.cpp` 16, `port_native_arms.c` 9, `gunfire.c`
  3, `port_grip_mesh.c` 2, `port_knife_ads.c` 1; 15 are debug traces to
  `float` in `propobj.c`). The other 114 start from `long`, `unsigned long`
  or `size_t` (sizes and pointer differences, a few address arithmetic).
  Explicit `(u32)` casts of display-list words are rare (5 places).
- **Command words written by hand.** 41 assignments to `words.w0` in 20
  files (5 in strict-tier files). One written from a signed 32-bit value
  sign-extends into the 8-byte word: the test driver's
  `(u8) G_ENDDL << 24` became `0xffffffffb8000000`, and a reader's
  `w0 >> 24` no longer equalled the opcode. `gbi.h`'s own macros build the
  word unsigned and are not affected.
- **Calls without a prototype.** 401 places, 309 names (10 are C library
  functions or macros). 106 of the names are defined with a pointer or 64-bit
  parameter or result; they are called at 157 places in 73 files (20 places in
  strict-tier files). An `int` argument reaches such a parameter with its
  upper half undefined.
- **Incompatible pointer types.** 1,788 places. 51 pair a pointer to a
  4-byte word with a pointer to a struct or to a pointer (7 in strict-tier
  files; `propobj.c` 12, `tex.c` 8). A struct viewed as words through an
  explicit cast, like `(s32 *) record` in `stan.c`, is not among them: no
  compiler option reports it. `src/` and `port/src/` have 155 such casts,
  not classified.
- **The `-m32` test harnesses at 64 bits** (verification item 10). 30
  `test_*.sh` harnesses; 18 compile with `-m32`, 7 of those also with
  `-msse2 -mfpmath=sse`, which aarch64 GCC rejects. With only those flags
  removed, 16 of the 18 pass at 64 bits and 2 fail to compile, both on the
  driver's own stub `u32 osVirtualToPhysical(void *)`. With the stub
  corrected in a scratch copy, both fail at run time on real defects:
  `test_ads` on the sign-extended command word above, `test_wardrobe` because
  `Vertex` is 24 bytes at 64 bits (`struct Vertex *LinkedTo`,
  `src/bondtypes.h:710`) while the data it indexes is 16-byte `Vtx`.
- **The LP64-002 hand-off list**, every LP64-003 item checked against the
  code: the table under "Hand-off items checked" below.

The per-site tables (`narrow.tsv`, `unproto.tsv`, `incompat.tsv`), the scripts
that made them and the scratch diffs are kept in the ignored folder
`.superpowers/sdd/native_64bit_lp64_003/research/` of the Mac checkout. Task 1
makes the tool able to regenerate the first two.

### What could not be measured

- Anything after the title's stage load at 64 bits: every stage load, the
  smoke beyond boot, the renderer, sound. That needs LP64-004's converters.
- The 64-bit video executable at run time (no display in the containers).
- Whether `loadAnimationFrame`'s "address is in RAM" branch
  (`model.c:5916`) ever runs on PC. Task 11 measures it.
- Which of the 157 calls without a prototype pass a wrong-width argument:
  only declaring the functions shows it (Task 9).
- The effect of each planned edit on the 32-bit executable. Each task
  establishes it with `compare_win32.py`.
- Real x86 hardware, `clang`, GCC 14, and the browser build (CI only).

## Hand-off items checked

"Run time" means the game builds the data in host memory; "file" means it is
read in place from a ROM file or a compiled-in copy of one. File layouts are
LP64-004's by the design, whatever the hand-off list said.

| Item (where, at 2afe62d) | Still true? | At 64 bits | Data | Owner |
|---|---|---|---|---|
| `texLoad(s32 *updateword)` (`image.c:2641,2738`) | Yes | Writes 4 of the 8 bytes of a `Gfx` `w1`, a `ModelFileTextures.TextureID` or an `sImageTableEntry.index` | run time and file | Task 7 |
| `vtxstore.c:111-114`, `tmp = 0x14` | Yes | The record is 32 bytes; arrays sized at 20 overrun | run time | Task 8 |
| `StandTileLocusCallbackRecord` as `s32` words (`stan.c:2723-2743`, `:2776-2779`) | Yes, and worse than recorded | The ladder flag lands in the high half of `rooms`; `stanGetLocusCount` returns 0 and the ladder is never seen | run time | Task 8 |
| `stanGetLocusField0` (`stan.c:2798`) | Yes | Returns the low half; the crouch flag survives | run time | Task 8 |
| `mema7000A2F8` callback takes `u32` (`mema.c:568`) | Yes | Would truncate; the function has no caller | run time | Task 8 |
| `(u32)` texture-ID tests (`othermodemicrocode.c:362`, `port_textures.c:82`) | Yes | A pointer with a small low word passes as a texture number | run time | Task 6 |
| `u32 vaddr` / `u32 timg` (`propobj.c:9167,9292,9476`, `bg.c:4513,4714`) | Yes | High half dropped, then used as an address | run time | Task 6 |
| `((u32 *) gdl)[1]` | Changed: 7 of 8 are N64-only | The live one, `bg.c:4377`, reads the high half of `w0` | run time | Task 7 |
| Sentinel `== 0x0E000000` (`lightfixture.c:295`) | Yes | A host pointer whose bits 24-31 are `0x0E` is rebased | run time | Task 6 |
| Sentinel mask `0x05000000` (`model.c:5976`) | Yes | Same flaw, in the model promotion | file | Task 11 (strict), then LP64-004 |
| Sentinel `address & 0x80000000` (`model.c:5916`) | Yes | Tests bit 31 of a host address: arbitrary | file | Task 11 (strict) |
| Sentinels in `gun.c:1027,1036`, `objecthandler_2.c:411`, `image.c:2758`, `front.c:11799-11817,13177`, `propobj.c:7631`, `initanitable.c:239,248`, `bg.h:32`, `audi.c:439` | Yes | Already full-width unsigned comparisons; correct once `texLoad` stores whole pointers | mixed | Task 6 pins them |
| Sentinel `< PORTMAX` (`bg.c:1214`) | Yes; the slot is `s32`, not pointer-sized | Cannot hold a host pointer | file | LP64-004 (already in its list) |
| `sImageTableEntry` stride (12 / 16) | Yes | Compiled-in arrays, indexed in C: consistent (note from the final review: not everywhere; `glass2.c`'s bullet spark stepped its table by a literal 12 and read its colour at raw byte offsets, fixed in the final review fix wave) | compiled-in | none |
| `ModelFileTextures` stride (12 / 16) | Yes | `romfile_model.c` walks 12, C indexes 16 | file | LP64-004 (already in its list) |
| Op11 union (`bondtypes.h:1083-1094`) | Yes | Fields move from 0x40 to 0x80 | file | LP64-004 (moved; see decision 6) |
| `AnimModelSlot` and `hand.field_B68` viewed as `Model` | Yes | 192 against 264 bytes; also `ModelSlot` (40), `model.c:155` (`0xc0`) and `:85` (`0x20`) | run time | Task 11 (strict) |
| `initobjects.c:9-11`, `&monAnimNN` | Yes | A type mismatch only; same value | compiled-in | Task 8 |
| `Acmd` words holding buffer addresses | Yes | Truncated in nine `abi.h` macros | run time | LP64-004 (the design says so; see decision 6) |
| `audi.c` returning `osVirtualToPhysical` from `s32` | Changed: `amDmaCallback` returns `intptr_t` | Nothing left | | none |
| `ALIGN64_V1` callers | Yes | Correct at 64 bits (`~0x3F` sign-extends) | | none |
| `prop.c:1523` `sizeof` / 4 | Changed: that line is harmless | `:1521` is the defect: `SetupIntroCamera` is 14 words, the compiled-in intro has 10 | compiled-in | LP64-004 (setup class) |
| `stan.c:250` (DEBUG), `s32` against a pointer | Yes | No caller | | Task 8 |
| Suspend image's 32-bit addresses (`port_state.c:49-62`) | Yes | No refusal exists in code | run time | Task 5 |
| `%08x` of `Gfx` words (`gfx_pc.cpp:8942,8352`, `bg.c:4088`, `lightfixture.c:160`) | Yes | Undefined or truncated output | | Tasks 7 and 11 |
| `lp64_casts.py` misses `(u32)((uintptr_t)p)` and `(uint32_t)(uintptr_t)p` | Yes | A widened pattern finds 29 sites against 28; the new one is a pointer difference | | Task 1 |
| `gate` reads only `-Werror=` and `-w` tokens | Yes | | | Task 1 |
| The `-m32` harnesses | Yes | 16 of 18 pass with the flag removed | | Task 2 |

Found while checking, not in the list: `Vertex` is 24 bytes (Task 11);
`port_textures.c:347` stores a truncated pointer in `entry.index` (Task 6);
`objecthandler_2.c:304,306` step a model file's arrays at the wrong stride
and `bg.c:1116` reads `((u32 *) gptr_stan)[1]` (both file layouts, LP64-004);
`seg_addr` (`gfx_pc.cpp:8202`) tests the tagged form before the host-pointer
test, so an odd host address can be taken for a segment word (Task 11);
`struct ModelSlot` viewed as a `Model` (Task 11); four raw offsets from
`g_CurrentPlayer` in `bondview2.c` (Task 11).

## What LP64-003 owns, and what it leaves to LP64-004

LP64-003 owns addresses and memory the game builds at run time: where the
arenas and the ROM image live, how a host pointer is told from a segment word,
integers that carry an address, the layout of structures the game allocates
itself, and the fixed renderer formats.

LP64-004 owns every byte read in place from a ROM file or from a compiled-in
copy of one: the ten asset classes of the design, their file layouts and
converters, the expansion of ROM display lists, and (by the design's own
words) `Acmd`. This plan changes no in-place conversion. It puts a guard in
front of each one (Task 4), so that at 64 bits the game stops with the class's
name instead of running a conversion that cannot be right.

Three items straddle the line and are placed deliberately:

- `texLoad`'s parameter type is fixed here (Task 7), because it is an address
  carried in 32 bits. The 12-byte walk of ROM texture records stays with
  LP64-004.
- `Vertex` becomes 16 bytes at 64 bits here (Task 11). The design fixes its
  shape; LP64-004's model converter fills the slot.
- The two sentinels in `model.c`'s promotion and animation code are made
  full-width here (Task 11), because the hand-off list gave their behaviour to
  this milestone. LP64-004 replaces the code around them.

Boot order of the in-place conversions, for LP64-004's plan. Measured: the
animation segments' byte-order pass (`portAnimDataInit`, `main.c:899`, before
the game's boot), the text banks (`langInit`; seven banks are logged before
the crash), the animation table's relocation (`boss.c:282`, the crash), the
audio banks and the music table (`musicSeqPlayerInit`), then the title's
stage load. By reading: fonts come between the text banks and the animation
table's relocation (`textInit`). The design lists animation tables fourth and
audio ninth; the boot needs them first and fourth, and there is no way to
boot without the audio bank.
(Note from Task 4: measured, fonts load after the animation table's
relocation, not before it; FORK-089's Task 4 note has the measured order.)

## Global Constraints

- **The 32-bit program does not change.** Before every commit,
  `python3 scripts/port/compare_win32.py commits HEAD WORKTREE` exits 0. Where
  it exits 1, the commit is acceptable only if every changed function is one
  whose source the commit edited, or one that differs because a declaration it
  uses changed in the same commit, and the change is explained by name in the
  commit message and FORK-089 (find the functions by disassembling the two
  executables the tool leaves in `build/lp64/compare` with
  `i686-w64-mingw32-objdump -d` and comparing by symbol). The accepted kinds
  of difference are those of the LP64-002 plan, each established by reading
  both versions instruction by instruction: block order and branch direction;
  register and stack-slot choice; an equivalent comparison; a load moved
  across a call or test that cannot affect it; jump threading that duplicates
  code, when every path is shown to make the same calls and stores; a loop
  written with an indexed address instead of a stepped pointer, with the same
  accesses in the same order; two stores to distinct non-volatile objects
  reordered, with no read of either and no call between them; a load moved
  ahead of a store to another, non-overlapping field of the same object.
  Otherwise change the fix.
- **New behaviour is 64-bit only.** A predicate, accessor or layout that
  differs at 64 bits is written so that its 32-bit form is the expression the
  site has today, token for token where possible, including under `GE_XBOX`.
  Code that exists only at 64 bits sits behind `IS_64_BIT`
  (`include/platform_info.h`), never behind the CMake option.
- **No design may depend on low addresses** (the design's measurement on
  macOS). Nothing in this milestone adds a hint, a fixed base or a `-no-pie`
  link at 64 bits, even as a development switch.
- **Renderer formats are fixed.** `Vtx` 16 bytes, `Mtx` 64, `Vp` 16, the light
  structs, and after Task 11 the game's `Vertex` 16, at both widths. The
  layout probe asserts each.
- **No in-place conversion of ROM data is changed here.** A site whose true
  fix is a converter keeps its code, gets the guard of Task 4 in front of it,
  and is named in the hand-over to LP64-004.
- **Fix declarations, not call sites; no laundering.** As in LP64-002. A new
  truncating round-trip cast needs a comment naming the milestone that
  removes it and a line in `scripts/port/lp64_casts_allowed.txt`. A deliberate
  narrowing of a pointer-sized integer is written as an explicit cast or
  through the accessors of Task 7, so the narrowing check stays at zero.
- **Upstream-owned files get small mechanical edits**: no reformatting, no
  renaming, no moved code. New logic goes in `port/`. FORK-089 records each
  file touched.
- **Strict-tier files are changed only in Task 11**, on the branch
  `lp64/addr-strict`: `src/game/gunfire.c`, `src/game/model.c`,
  `src/game/bondview2.c`, everything under `port/fast3d/`, and
  `port/src/port_arm_mesh.c`, `port_native_arms.c`, `port_grip_mesh.c`,
  `port_wardrobe.c`, `ads_hands.c`, `port_knife_ads.c`, `port_graphics.c`,
  `port_shiny.c`, `port_weapon_images.c`, `port_hud_weapons.c`, `joy_port.c`,
  `scripts/port/hand_gallery.py` and its template. A declaration whose
  definition or only user is in one of them changes there too.
- **Hands and arms.** `scripts/port/arm_rendering.md` applies to Task 11: no
  change to hand vertices, UVs, colours, materials or render setup. Its edits
  are type and accessor changes only.
- **Every commit updates `FORK_CHANGES.md`** (FORK-089): what changed, the
  check counts before and after, and what ran.
- No ROM-derived data in Git, including fixtures. Python uses the standard
  library only. New source files stay under 500 lines. Shell scripts start
  with `#!/usr/bin/env bash` and `set -euo pipefail`. Build output goes under
  `build/lp64/` and is deleted before each push.
- Delivery is direct to `main`, one push per task after its review is clean,
  except Task 11. Never use `git commit --no-verify`.
- Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

### Task check

Every task ends by running this and recording the results in FORK-089. Steps
4 and 5 exist from the task that creates them (Tasks 1 and 4).

```bash
# 1. The 64-bit compile gate, in the x86_64 container; also in the aarch64
#    container for a change to C, C++ or the build (wipe both trees first when
#    switching containers).
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c '
  cmake -S . -B build/lp64/gate -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate --target ge_buildinfo &&
  cmake -S . -B build/lp64/gate-video -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate-video --target ge_buildinfo &&
  python3 scripts/port/lp64_diagnostics.py gate build/lp64/gate build/lp64/gate-video'

# 2. The 32-bit game still builds and plays (i686, headless, with the ROM).
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c 'cmake -S . -B build/lp64/ref-i686-stock -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON && cmake --build build/lp64/ref-i686-stock -j"$(nproc)"'
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ref-i686-stock/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 2 --output build/lp64/ref-i686-stock/smoke.json

# 3. The shipped Windows program is unchanged (before committing).
python3 scripts/port/compare_win32.py commits HEAD WORKTREE

# 4. From Task 1: the two static checks, on the trees of step 1.
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c '
  python3 scripts/port/lp64_diagnostics.py narrowing --check build/lp64/gate build/lp64/gate-video &&
  python3 scripts/port/lp64_diagnostics.py unprototyped --check build/lp64/gate build/lp64/gate-video'

# 5. From Task 4: the 64-bit executable boots to its expected stop, plain and
#    under the sanitizers (aarch64, native).
python3 scripts/port/lp64_env.py run --arch aarch64 -- sh -c '
  cmake -S . -B build/lp64/boot -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON && cmake --build build/lp64/boot -j"$(nproc)" &&
  cmake -S . -B build/lp64/boot-san -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_SANITIZE=ON && cmake --build build/lp64/boot-san -j"$(nproc)" &&
  python3 scripts/port/lp64_boot.py --rom /rom/baserom.u.z64 --expect-stop "animation tables" build/lp64/boot/IronSights build/lp64/boot-san/IronSights'
```

Expected: step 1 prints `0 failed` for both trees; step 2 exits 0 with 20 of
20 missions `PASS`; step 3 exits 0 or the commit explains each changed
function; step 4 exits 0 with the counts the task says; step 5 prints
`OK: stopped at "animation tables"` for each executable and exits 0. Also run every
harness line of the CI tests step (`Test gameplay, cache and release tooling
without a ROM` in `.github/workflows/windows-release.yml`), in the i686
container where a line needs `-m32`. Run step 5 once in the x86_64 container
at Task 4 and at the milestone's close. The browser build is checked by CI
after the push; a red browser build is the task's to fix.

### Behaviour-sensitive sites

These must not be changed by rote. Each is handled as stated, in the task that
owns it.

- `PORT_IS_HOST_ADDR` (`port/include/port_hostptr.h`): its 32-bit forms, the
  `GE_XBOX` one included, stay as they are. At 64 bits it is "any of the upper
  32 bits set" and must not cast through `unsigned int`. (Task 3)
- The `>= 0x10000000u` tests on `u32` copies of command words
  (`propobj.c:9169,9294,9478`, `bg.c:4515,4716`): at 32 bits the literal
  threshold stays, not `PORT_IS_HOST_ADDR`, because the Xbox form differs.
  (Task 6)
- `(timg >> 16) != 0xABCDu` beside them is a marker test on a 32-bit value;
  it stays a test of the low word. (Task 6)
  (Note from Task 6's review: the marker test keeps its original uncast
  expression, which reads the low word at 32 bits and the whole word at 64
  bits, where a tag is already rejected by the host test; a `(u32)` cast
  would reject host pointers whose bits 16-31 are `0xABCD`.)
- `BG_SEG_IS_OFFSET` (`bg.h:32`) and `SETUP_PROMOTE` (`prop.c:1455`) are
  already full-width and belong to file layouts. Not changed. (LP64-004)
- `StandTileLocusCallbackRecord`'s first two fields double as flags. The fix
  writes the same fields by name, so the 32-bit stores are the same four
  bytes at the same offsets. (Task 8)
- A command word's upper half is zero. Writers are fixed, readers are not
  taught to mask: a reader that masked would hide a writer that sign-extends
  from the renderer's own `w0 >> 24`. (Tasks 7 and 11)
- `loadAnimationFrame`'s `address & 0x80000000` (`model.c:5916`): measure
  first whether the RAM branch ever runs on PC, then decide. (Task 11)
- `seg_addr` (`gfx_pc.cpp:8202`): the 32-bit order of its two tests stays.
  At 64 bits a host pointer returns before the tagged form is tried.
  (Task 11)
- `Model` views of `AnimModelSlot`, `ModelSlot` and `hand.field_B68`: the
  32-bit sizes and offsets stay. At 64 bits the storage grows to hold a
  `Model`. (Task 11)
- Prototypes change how arguments are passed (a `float` is no longer promoted
  to `double`, `char` and `short` no longer to `int`). The rule of LP64-002
  Task 3 applies to Task 9: each changed caller is explained by the parameter
  that caused it, or the function is left out and reported.

## Review Focus

1. A 64-bit rule leaks into the 32-bit program (a shared macro now tests
   something else at 32 bits). Expected: the Windows comparison differs and
   the commit is refused or explained. (Task check, step 3.)
2. The startup check passes although something sits below 4 GB. Expected: a
   `-no-pie` link of the test driver fails the check, naming the executable
   image. (Task 3, Step 1.)
3. A guard is missing, so the 64-bit game runs an in-place conversion.
   Expected: the boot check stops at a class other than the expected one, or
   crashes, and fails. (Task 4; task check, step 5.)
4. A pointer-sized integer is narrowed implicitly in new code. Expected: the
   narrowing check fails naming the place. (Task 1.)
5. A deliberate narrowing is hidden in a round-trip cast. Expected: the cast
   allowlist test fails. (Task 1.)
6. A harness passes at 64 bits only because its 64-bit half was skipped.
   Expected: the harness prints every width it ran, and the pending list is
   empty at the close. (Tasks 2 and 12.)
7. A strict-tier edit changes the hands. Expected: the Windows comparison
   shows it before the laptop trip, and the hand-off names the functions.
   (Task 11.)

---

### Task 1: Tooling for the milestone

**Files:**
- Modify: `scripts/port/lp64_casts.py`, `scripts/port/test_lp64_casts.py`,
  `scripts/port/lp64_casts_allowed.txt`
- Modify: `scripts/port/lp64_diagnostics.py`,
  `scripts/port/test_lp64_diagnostics.py`
- Create: `scripts/port/lp64_narrowing_allowed.txt`,
  `scripts/port/lp64_unprototyped_allowed.txt`
- Modify: `.github/workflows/windows-release.yml` (two lines after the `gate`
  line), `AGENTS.md` (the 64-bit paragraph), `FORK_CHANGES.md`,
  `scripts/port/native_64bit_plan.md` (LP64-003 row)

`lp64_diagnostics.py` is 343 lines. If the two new commands would take it
past 500, put them in a new module beside it and say so in FORK-089.

**Interfaces:**
- `lp64_casts.py`, `find_casts(text) -> list[int]` also finds a truncating
  cast applied to a parenthesised pointer-width cast, `(u32)((uintptr_t)p)`,
  and the types `uint32_t`, `int32_t`, `uint16_t`, `int16_t`. It does not
  report a cast of a difference, `(s32)((uintptr_t)a - (uintptr_t)b)`
  (`src/init.c:129` is one).
- `lp64_diagnostics.py gate`: an entry is also refused when it carries
  `-Wno-error`, `-Wno-error=KIND` or `-Wno-KIND` for a gated kind, or an
  argument starting with `@` (a response file).
- `lp64_diagnostics.py narrowing [--check] BUILD_DIR...`: compiles every C
  and C++ entry syntax-only with `-Wconversion -Wno-sign-conversion
  -Wno-float-conversion` under `LC_ALL=C`, and lists each implicit conversion
  whose source type is `uintptr_t`, `intptr_t` or `romptr_t` as
  `file:line:column: SOURCE -> TARGET | source text`. `--all` adds the other
  64-bit source types (`long`, `unsigned long`, `size_t`, `ptrdiff_t`,
  `ssize_t`, 64-bit fixed types) as a report only. `--check` exits 1 unless
  each file's count equals its line in `lp64_narrowing_allowed.txt`
  (`path<TAB>count<TAB>reason`, exact match as the cast allowlist is). GCC
  drops the typedef name after arithmetic, so `(uintptr_t) a + b` narrowed to
  `u32` is reported only under `--all`; say so in the command's help.
- `lp64_diagnostics.py unprototyped [--check] BUILD_DIR...`: compiles every C
  entry with `-Wimplicit-function-declaration` restored and `-aux-info`,
  and lists each call without a prototype to a function whose definition in
  the tree takes or returns a pointer or a 64-bit type, as
  `file:line:column: NAME | PROTOTYPE`. `--check` works against
  `lp64_unprototyped_allowed.txt` in the same exact-match form.

- [ ] **Step 1: Write the failing tests for the cast pattern, then implement**

In `test_lp64_casts.py`: `find_casts("x = (u32)((uintptr_t)p);")` returns
`[1]`; `(uint32_t)(uintptr_t)p` is found; `(s32)((uintptr_t)a - (uintptr_t)b)`
is not. Run `python3 scripts/port/lp64_casts.py check`. Expected: exit 0 with
the allowlist unchanged (28 casts in 13 files); if a file's count moves, fix
the pattern, not the allowlist.

- [ ] **Step 2: Write the failing tests for the gate's refusals, then implement**

In `test_lp64_diagnostics.py`, four entries that must be refused, each with
its reason in the output: `-Wno-error`, `-Wno-error=int-conversion`,
`-Wno-int-to-pointer-cast`, `@flags.rsp`. A clean entry still passes.

- [ ] **Step 3: Write the failing tests for `narrowing`, then implement**

Tests, with a fake compiler script as the existing tests use: a line
`conversion from 'uintptr_t' {aka 'long unsigned int'} to 'u32'` is listed;
`from 'long int'` is listed only with `--all`; `--check` exits 1 naming a file
whose count differs from its line, higher or lower, and 0 when all match.

- [ ] **Step 4: Write the failing tests for `unprototyped`, then implement**

Tests: an implicit declaration of `f` with an aux-info line
`/* a.c:1:NF */ extern void f (char *); ` is listed; one for
`extern int g (int);` is not; a name with no definition is not.

- [ ] **Step 5: Record today's sites and wire CI**

Run both commands on the trees of the task check's step 1 and write the two
allowlists with the reason `existing before LP64-003`. Expected: 111 places
in 18 files for `narrowing`, 157 places in 73 files for `unprototyped`;
record the actual figures, and explain any difference from these in FORK-089.
Add to the CI step, after the `gate` line:

```yaml
          python3 scripts/port/lp64_diagnostics.py narrowing --check build/lp64/gate build/lp64/gate-video
          python3 scripts/port/lp64_diagnostics.py unprototyped --check build/lp64/gate build/lp64/gate-video
```

- [ ] **Step 6: Run the task check, commit**

```bash
git commit -m "See narrowed pointer-sized integers and unprototyped pointer calls at 64 bits"
```

---

### Task 2: The `-m32` harnesses also run at 64 bits

**Files:**
- Create: `scripts/port/harness_widths.sh`,
  `scripts/port/test_harness_widths.sh`
- Modify: the 18 harnesses that compile with `-m32` (`test_ads.sh`,
  `test_buffers.sh`, `test_build_time.sh`, `test_dyn.sh`, `test_hit_sound.sh`,
  `test_hitmap.sh`, `test_hud.sh`, `test_image_bank.sh`,
  `test_mission_cheats.sh`, `test_mission_hints.sh`, `test_mission_times.sh`,
  `test_save_transfer.sh`, `test_stan_callbacks.sh`, `test_start_input.sh`,
  `test_states.sh`, `test_upstream_merge.sh`, `test_wardrobe.sh`,
  `test_web_logo.sh`), `scripts/port/test_ads.c`, `test_wardrobe.c`,
  `test_knife_ads.c`
- Modify: `.github/workflows/windows-release.yml` only if a line's
  environment changes, `FORK_CHANGES.md`

**Interfaces:**
- `harness_widths.sh`, sourced by a harness, provides:
  - `harness_widths`: prints the widths to run, one per line: `32` when the
    compiler accepts `-m32`, `64` when the host compiler's pointers are 8
    bytes. `MIDAS_HARNESS_WIDTHS="32"` or `"64"` restricts it.
  - `harness_flags WIDTH [EXTRA...]`: prints `-m32` and the harness's own
    x86 flags (`-msse2 -mfpmath=sse`, where it had them) for 32, and nothing
    for 64: x86_64 and aarch64 use SSE or NEON arithmetic by default.
  - `harness_pending NAME WIDTH`: true when `NAME WIDTH` is in the list
    `HARNESS_PENDING` at the top of the file; the harness then prints
    `NAME: 64-bit half pending (LP64-003 task 11)` and skips that half.
- Each harness runs its body once per width and prints `NAME: PASS (32-bit)`
  and `NAME: PASS (64-bit)`. Its 32-bit commands are unchanged.
- `HARNESS_PENDING` starts as `test_ads 64` and `test_wardrobe 64`; Task 11
  empties it. `test_states.sh` is 32-bit only by design from Task 5 and says
  so in one line.
- The two driver stubs become
  `uintptr_t osVirtualToPhysical(void *)`, matching `include/PR/os.h`.
  `test_knife_ads.c` builds its command words with the `gbi.h` macros or an
  unsigned shift.

- [ ] **Step 1: Write the failing test for the helper**

`test_harness_widths.sh`: with a fake `gcc` first on `PATH` that rejects
`-m32`, `harness_widths` prints `64` only; with one that accepts it on a
64-bit host, `32` then `64`; `MIDAS_HARNESS_WIDTHS=32` prints `32`;
`harness_pending test_ads 64` is true and `harness_pending test_dyn 64`
false. Add it to the CI step.

- [ ] **Step 2: Convert the 16 harnesses that pass today**

Expected in the x86_64 container: every one prints both PASS lines. In the
aarch64 container: the 64-bit line only. In the i686 container run of the CI
lines: both.

- [ ] **Step 3: Convert `test_ads.sh` and `test_wardrobe.sh` with their 64-bit halves pending**

Fix the stubs and the driver's command words. Expected: both compile at 64
bits; `test_ads` may then pass at 64 bits, in which case take it off the
pending list now; `test_wardrobe` fails at 64 bits on the 24-byte `Vertex`
(`ASSERTION FAULT - paNativeFit(...)`) and stays pending.

- [ ] **Step 4: Run the task check, commit**

```bash
git commit -m "Run the ROM-free test harnesses at both pointer widths"
```

---

### Task 3: Arenas above 4 GB, the host-pointer test and the startup check

**Files:**
- Create: `port/include/port_lp64.h`, `port/src/port_lp64.c`,
  `scripts/port/test_lp64_address.sh`, `scripts/port/test_lp64_address.c`
- Modify: `port/include/port_hostptr.h`, `port/src/pcsched.c`,
  `port/src/main.c`, `port/src/romdata.c`, `port/src/port_memp.c`,
  `CMakeLists.txt` (the new source), `scripts/port/abi_probe/probe.c`,
  `.github/workflows/windows-release.yml` (one line), `FORK_CHANGES.md`

**Interfaces:**
- `port_hostptr.h`: at 64 bits
  `PORT_IS_HOST_ADDR(v)` is `((((uintptr_t) (v)) >> 32) != 0)`. The 32-bit
  definitions, `GE_XBOX` included, are untouched. The header's comment states
  the 64-bit rule and its condition (no game data below 4 GB).
- `port_lp64.h`, everything a no-op macro at 32 bits:
  - `int portLp64RegionIsHigh(const void *start, size_t size)`: pure; true
    when the whole region is at or above `0x100000000`.
  - `void portLp64CheckRegion(const char *name, const void *start, size_t size)`:
    on a low region prints
    `ge: lp64: NAME at ADDRESS is below 4 GB; a 64-bit build needs every game address above 4 GB (scripts/port/native_64bit_plan.md, "Addresses")`
    and exits with status 4. Otherwise remembers the region.
  - `void portLp64CheckAddressSpace(void)`: checks the main executable's
    loaded segments (on Linux from `dl_iterate_phdr`), a `malloc` block and a
    stack address, then prints `ge: lp64: address space OK` with each
    remembered region's name and start.
- `pcsched.c` at 64 bits: `portAllocHighRaw` passes no hint and treats a
  result below 4 GB as a failure; `portGetMempArena`, `portAllocHighReserve`,
  `portAllocHigh` and `portAllocHighDedicated` call `portLp64CheckRegion`
  with the names `memp arena`, `high arena` and `high allocation`; a failed
  allocation is fatal, naming what was asked for, not a `NULL` return. The
  32-bit paths, hints and scan are untouched.
- `romdata.c`: the ROM image is passed to
  `portLp64CheckRegion("ROM image", ...)` once it is in place; the fallback
  that keeps the heap copy is 32-bit only.
- `main.c`: `portLp64CheckAddressSpace()` is called right after
  `romdataInit` returns (`main.c:884`): the arenas are reserved and the ROM
  image is in place, and no ROM data has been converted yet
  (`portAnimDataInit` is the next call).
- `port_memp.c`: the exhaustion message names the bank
  (`permanent`, `stage`, ...) as well as its number, at both widths. This
  changes one 32-bit string; say so in the commit.
- The other pools the design names (room display lists, the graphics buffer,
  model and setup allocations, texture pools, the music heap) are not resized
  or changed here. Read what each does when it runs out and record it as a
  table in FORK-089, for LP64-005.
- The sanitizers do not trip the check (their executables are
  position-independent too; the boot check of Task 4 runs under them).
  Valgrind would; the design accepts that.

- [ ] **Step 1: Write the failing address tests**

`test_lp64_address.sh` (uses `harness_widths.sh`) builds
`test_lp64_address.c` with `port_lp64.c`:
1. Truth table of `PORT_IS_HOST_ADDR` at each width. 32 bits: `0x0FFFFFFF`
   false, `0x10000000` true. 64 bits: `0x0E000000` false, `0xFFFFFFFF` false,
   `0x100000000` true, `0x0000AAAA03123456` true (its low word looks like a
   segment-3 word, which the old macro got wrong).
2. `portLp64RegionIsHigh`: a region ending at 4 GB is low; one starting at
   4 GB is high; a zero-size region at 4 GB is high.
3. Linked as a position-independent executable, the driver's
   `portLp64CheckAddressSpace()` prints `ge: lp64: address space OK` and
   exits 0.
4. The same driver linked `-no-pie` exits 4 and names
   `executable image` in its message.

Run it before the change: it fails to compile (no `port_lp64.h`). Add it to
the CI step.

- [ ] **Step 2: Implement; run the 64-bit executable**

Expected, in the aarch64 container, from
`build/lp64/boot/IronSights` with `GE_FRAMES=120`: the log has
`memp arena: 64 MB at 0xffff…`, the high arena likewise, and
`ge: lp64: address space OK`; the run then crashes in the animation code as
measured (Task 4 turns that into a stop). In the x86_64 container the
addresses start `0x7fff…`.

- [ ] **Step 3: Probe; task check; commit**

`probe.c` includes `port_hostptr.h` and asserts the macro on constants:
`!PORT_IS_HOST_ADDR(0x0E000000u)` at both widths, and at 64 bits
`!PORT_IS_HOST_ADDR(0xFFFFFFFFu)` and `PORT_IS_HOST_ADDR(0x100000000ull)`.
Run the task check. Expected: step 3 exits 0 apart from the one message
string in `port_memp.c`, which the commit names.

```bash
git commit -m "Keep 64-bit game memory above 4 GB and check it at startup"
```

---

### Task 4: A named stop at each unconverted asset class, and the boot check

**Files:**
- Modify: `port/include/port_lp64.h`, `port/src/port_lp64.c`
- Modify (one call each): `port/src/romdata.c` (`romdataTextPreprocess`),
  the place the texture directory and image bank are installed (start from
  the log line `romdata: texture table`, `romdata.c:1114`, and from
  `texLoad`'s callers in `src/game/image_bank.c`), `port/src/segments.c`
  (`portFontBuildSegments`), `port/src/animdata.c` (`portAnimDataInit`),
  `port/src/romfile_model.c` (`romfileModelConvert`),
  `port/src/romfile_stan.c` (`romfileStanConvert`),
  `port/src/romfile_setup.c` (`romfileSetupConvert`),
  `port/src/romfile_bg.c` (`romfileBgConvert`), `port/src/audio_stubs.c`
  (`alBnkfNew`), `src/music.c` (before the sequence table is read)
- Create: `scripts/port/lp64_boot.py`, `scripts/port/test_lp64_boot.py`
- Modify: `scripts/port/test_lp64_address.c` (guard cases),
  `.github/workflows/windows-release.yml` (one line), `AGENTS.md` (the
  64-bit paragraph), `FORK_CHANGES.md`

**Interfaces:**
- `port_lp64.h`:
  - `enum PortLp64Asset`: `PORT_LP64_TEXT_BANKS`, `_FONTS`, `_TEXTURES`,
    `_ANIMATIONS`, `_MODELS`, `_COLLISION`, `_SETUPS`, `_BACKGROUNDS`,
    `_AUDIO_BANKS`, `_MUSIC`, then `PORT_LP64_ASSET_COUNT`: the design's ten
    classes.
  - `const char *portLp64AssetName(enum PortLp64Asset)`: `text banks`,
    `fonts`, `textures`, `animation tables`, `models`, `collision tiles`,
    `setup files`, `backgrounds`, `audio banks`, `music sequences`.
  - `int portLp64AssetConverted(enum PortLp64Asset)`: reads one table in
    `port_lp64.c`, all zero in this milestone. LP64-004 sets an entry in the
    commit that lands that class's converter.
  - `void portLp64RequireConverter(enum PortLp64Asset)`: when the class is
    not converted, prints
    `ge: lp64: stopping: asset class "NAME" has no 64-bit converter yet (LP64-004)`
    and exits with status 5. At 32 bits it is `((void) 0)`.
- Each in-place conversion calls `portLp64RequireConverter` as its first
  statement. The animation guard is in `portAnimDataInit`, the first of them
  the boot reaches, so the expected stop of this milestone is
  `animation tables`. A conversion with a compiled-in path (the 19 background and
  collision files this ROM lacks) is guarded on both paths.
- `lp64_boot.py`:
  - `parse_log(text: str) -> BootResult` with fields `address_ok: bool`,
    `stop_class: str | None`, `fatal_signal: bool`,
    `sanitizer_errors: list[str]`.
  - CLI `lp64_boot.py --rom ROM --expect-stop NAME EXECUTABLE...`: runs each
    executable in its own temporary directory with `GE_FRAMES=120`,
    `GE_SEED=1`, `GE_NOINISAVE=1` and a 120 s timeout. Exit 0 only if every
    run exited with status 5, logged the address line, stopped at `NAME`,
    and logged no fatal signal and no sanitizer error. Prints
    `OK: stopped at "NAME"` or what differed, with the last 20 log lines.
    `--stage NAME` adds `GE_STAGE`.

- [ ] **Step 1: Write the failing tests**

`test_lp64_boot.py` (ROM-free): `parse_log` on four sample logs: a clean stop
at `animation tables`; a stop at another class; a fatal signal; a sanitizer report
before the stop. The CLI with a fake executable (a shell script that prints a
log and exits 5) passes; one that exits 139 fails. In `test_lp64_address.c`:
`portLp64AssetName` returns the ten names in order, and at 64 bits a child
process calling `portLp64RequireConverter(PORT_LP64_ANIMATIONS)` exits 5 with
the message. Add both to the CI step.

- [ ] **Step 2: Implement the guard and place the ten calls**

Verify the placement by reading, then by run: temporarily mark
`animation tables` converted in a scratch build and confirm the stop moves to
the next class the boot reaches (expected `text banks`, loaded by `langInit`,
from the boot order below); restore the table.

- [ ] **Step 3: Run the boot check**

Task check step 5, in the aarch64 container, then once in the x86_64
container. Expected: `OK: stopped at "animation tables"` for the plain and the
sanitizer executable, exit 0. Then the 20-mission form, for the record:
`lp64_boot.py --stage` for each mission of `audit_buffers.py`'s list stops at
the same class.

- [ ] **Step 4: Task check; commit**

Expected: step 3 exits 0 (the guards expand to nothing at 32 bits).

```bash
git commit -m "Stop a 64-bit boot at the first asset class without a converter"
```

---

### Task 5: Suspend points are unavailable on 64-bit builds

**Files:**
- Create: `port/src/port_state_unavailable.c`
- Modify: `CMakeLists.txt` (the 64-bit branch: swap the source, skip
  `embed_state_layout.py`), `scripts/port/lp64_casts_allowed.txt` (the
  `port_state.c` reason), `scripts/port/test_states.sh`,
  `scripts/port/save_states.md`, `scripts/port/test_pointer_bits.sh`,
  `FORK_CHANGES.md`

**Interfaces:**
- `port_state_unavailable.c` implements `port/include/port_state.h` as
  `port/web/state.c` does: `portStateMessage()` returns
  `"Suspend points are unavailable in 64-bit builds."`;
  `portStateRequest` and `portStateMenuOpen` print it;
  `portStateInfo` reports no slot; `portStateFrame` still calls
  `portGraphicsFrame()`.
- At `GE_POINTER_BITS=64`, `port/src/port_state.c` is not compiled and the
  post-build layout step does not run. The 32-bit source list and link are
  unchanged.
- The player-facing notice in the menus and the manual is LP64-007's. The
  existing watch row (`Suspend Points`) reaches `portStateMenuOpen`, which
  prints the message; no row is added, moved or removed, so no input idiom
  changes.

- [ ] **Step 1: Write the failing tests**

`test_pointer_bits.sh`: the 64-bit `compile_commands.json` has
`port_state_unavailable.c` and not `port_state.c`; the 32-bit one is
byte-identical to before this task. `test_states.sh`: at 64 bits, a driver
linked with `port_state_unavailable.c` gets 0 from `portStateInfo` and the
message from `portStateMessage`.

- [ ] **Step 2: Implement; task check; commit**

Expected: the 64-bit build no longer prints `State layout: … regions`; the
boot check still passes.

```bash
git commit -m "Report suspend points as unavailable in 64-bit builds"
```

---

### Task 6: Tell host pointers from numbers and segment words at full width

**Files:**
- Modify: `port/include/port_hostptr.h`, `src/game/propobj.c`,
  `src/game/bg.c`, `src/game/lightfixture.c`,
  `src/game/othermodemicrocode.c`, `port/src/port_textures.c`,
  `scripts/port/lp64_casts_allowed.txt`, `scripts/port/abi_probe/probe.c`,
  `scripts/port/test_lp64_address.c`, `FORK_CHANGES.md`

**Interfaces:**
- `port_hostptr.h` gains three predicates, each with a 32-bit form equal to
  the expression its sites have today:
  - `PORT_WORD_IS_HOST(w)`: 32 bits `((u32) (w) >= 0x10000000u)`; 64 bits
    `PORT_IS_HOST_ADDR(w)`.
  - `PORT_WORD_HAS_SEGMENT(w, seg)`: 32 bits
    `(((uintptr_t) (w) & 0xFF000000) == ((uintptr_t) (seg) << 24))`; 64 bits
    `(((uintptr_t) (w) >> 24) == (uintptr_t) (seg))`.
  - `PORT_WORD_IS_INDEX(w, limit)`: `((uintptr_t) (w) < (uintptr_t) (limit))`
    at both widths.
- Sites:
  - `propobj.c:9167-9169`, `:9292-9294`, `:9476-9478` and `bg.c:4513-4515`,
    `:4714-4716`: the `u32` copy becomes `uintptr_t`, the test becomes
    `PORT_WORD_IS_HOST`, and the pointer is rebuilt from the whole word. The
    `0xABCD` marker test reads the low word.
    (Note from Task 6's review: the marker test keeps its original uncast
    expression, which reads the low word at 32 bits and the whole word at 64
    bits, where a tag is already rejected by the host test; a `(u32)` cast
    would reject host pointers whose bits 16-31 are `0xABCD`.)
  - `lightfixture.c:295`: `PORT_WORD_HAS_SEGMENT(ret, 0x0E)`.
  - `othermodemicrocode.c:362` and `port_textures.c:82`:
    `PORT_WORD_IS_INDEX`, no `(u32)`.
  - `port_textures.c:347`: `entry.index` receives the whole pointer; its
    allowlist line goes. With the slot whole, the two refusals of external
    textures at 64 bits (`if (sizeof(void *) > 4`, `:368` and `:779`) are
    removed, as the design says. No harness links `port_textures.c` today;
    if its slot loader can be driven ROM-free, add a 64-bit case to
    `test_lp64_address.sh`, otherwise say so in FORK-089 and hand its first
    run to LP64-005.
- The full-width sentinels the check found correct are not edited. The probe
  pins them: for `ModelFileTextures.TextureID`, `sImageTableEntry.index` and
  `Gfx.words.w1`, assert the field is unsigned as well as pointer-sized.

- [ ] **Step 1: Write the failing predicate tests**

In `test_lp64_address.c`, truth tables at each width. `PORT_WORD_IS_HOST`:
32 bits as `PORT_IS_HOST_ADDR` non-Xbox; 64 bits `0x0E001234` false,
`0x0000AAAA0E001234` true. `PORT_WORD_HAS_SEGMENT(w, 0x0E)`: `0x0E001234`
true at both; `0x0000AAAA0E001234` false at 64 bits.
`PORT_WORD_IS_INDEX(w, 0x1000)`: `0xFFF` true; `0x0000AAAA00000123` false at
64 bits (the old `(u32)` test said true).

- [ ] **Step 2: Change the sites; task check; commit**

Expected: step 3 exits 0; step 4's narrowing count falls by 5 (the `u32`
copies) and the cast allowlist by 1.

```bash
git commit -m "Classify command words and texture slots at pointer width"
```

---

### Task 7: Display-list words and other pointer-sized integers

**Files:**
- Create: `port/include/port_gfxwords.h`
- Modify: `src/game/image.c`, `image.h`, `image_bank.c`,
  `othermodemicrocode.c`, `bg.c`, `propobj.c`, `tex.c`, `front.c`,
  `mpmenu.c`, `lightfixture.c`, `src/mema.c`, `src/init.c`, `src/macro.h`,
  `port/src/port_baked_light.c`, `audio_pc.c`, `port_memp.c`; the non-strict
  files among the 20 that assign `words.w0` (`unk_092E50.c`, `bgapply.c`,
  `tex.c`, `title.c`, `radar.c`, `options.h`, `options.c`, `bg.c`, `lv.c`,
  `glass2.c`, `explosion.c`, `chrprop.c`, `blood_animation.c`,
  `port/src/port_hud.c`, `roc_hooks.c`, `port_weapon_export.c`)
- Modify: `scripts/port/lp64_narrowing_allowed.txt`,
  `scripts/port/test_lp64_address.c`, `FORK_CHANGES.md`

**Interfaces:**
- `port_gfxwords.h`: `GFX_W0(g)` is `((u32) (g)->words.w0)`, the N64 command
  word; `GFX_OP(g)` is `(GFX_W0(g) >> 24)`; `GFX_W1_DATA(g)` is
  `((u32) (g)->words.w1)` for a second word that holds data, not an address;
  `GFX_W1_ADDR(g)` is `((g)->words.w1)`, pointer-sized. At 32 bits each is
  the plain member access with a cast that changes nothing.
- `texLoad(uintptr_t *updateword, ...)`. Its callers pass the real object:
  `&cmd->words.w1` (`image.c:2471`), `&textures[i].TextureID`
  (`image.c:2761`), `&entry.index` (`image_bank.c:256,260,265`,
  `othermodemicrocode.c:364`); the local at `image.c:2788` becomes
  `uintptr_t`.
- `bg.c:4377` reads `gdl->words.w1`, not `((u32 *) gdl)[1]`.
- Every assignment to `words.w0` stores a value whose upper half is zero: an
  unsigned expression, or the `gbi.h` macro. Record each of the 41 sites and
  its verdict in FORK-089; fix the ones that can sign-extend.
- Traces print a pointer-sized word with `PRIxPTR` (`bg.c:4088`, `:2959`,
  `lightfixture.c:160`).
- The non-strict narrowing places (60 before Task 6 took 5) go to zero:
  accessors for command words,
  an explicit cast where a pointer-sized value holds a number
  (`front.c:11801-11819,13179`, `mpmenu.c`, the monitor script's `time`), a
  pointer-sized type where it holds an address (`mema.c:232,571`,
  `init.c:117`, `macro.h:15`'s users). The 15 `float` traces in `propobj.c`
  get `(f32) (uintptr_t)` as LP64-002 did elsewhere.
- Every line of `lp64_casts_allowed.txt` that still reads
  `existing before LP64-002` gets its owner in the reason: the text bank
  (`romdata.c` 2), setup, collision and model converters (`romfile_setup.c`
  7, `romfile_stan.c` 4, `romfile_model.c` 1) and the bg file offsets
  (`bg.c` 3) are LP64-004's; `propobj.c:14719` hashes an address and keeps
  its low bits on purpose; the arm files' four are Task 11's.
- Review the 114 places of `narrowing --all` once. Fix the ones that carry an
  address; record the rest by kind in FORK-089. They are not gated.

- [ ] **Step 1: Write the failing tests**

In `test_lp64_address.c`: `GFX_OP` of a command built with
`gsSPEndDisplayList()` equals `(u8) G_ENDDL` at both widths, and its
`words.w0 >> 32` is 0 at 64 bits; a `uintptr_t` slot passed to a stub with
`texLoad`'s new parameter type keeps all 8 bytes of
`0x0000AAAA12345678`.

- [ ] **Step 2: `texLoad` and its callers; task check; commit**

```bash
git commit -m "Store whole texture pointers through texLoad"
```

- [ ] **Step 3: Command words and the remaining narrowings; task check; commit**

Expected: `narrowing --check` passes with only strict-tier files left in the
allowlist (51 places in 6 files).

```bash
git commit -m "Read and write display-list words at their own width"
```

---

### Task 8: Structures the game builds at run time

**Files:**
- Modify: `src/game/vtxstore.c`, `src/game/stan.c`, `src/mema.c`,
  `src/game/initobjects.c`, `src/game/propobj.c` (`:7208`),
  `scripts/port/test_stan_callbacks.c`, `scripts/port/abi_probe/probe.c`,
  `FORK_CHANGES.md`, `scripts/port/native_64bit_plan.md` (hazard inventory)

**Interfaces:**
- `vtxstore.c:111-114` allocates `count * sizeof(struct unk_09B7A0_struct_parent)`.
  The probe asserts that struct is 0x14 bytes at 32 bits.
- `stan.c`: the callback writes `record->rooms` and `record->count` by name
  (`rooms` holds the flag as `(s32 *) (intptr_t) 1`), and the clear sets the
  four fields by name. `stanGetLocusField0` keeps its `s32` result.
- `mema7000A2F8`'s callback takes `intptr_t`.
- `initobjects.c:9-11` and `propobj.c:7208` name the script's first element,
  so the types agree.
- `stan.c:250-256` (DEBUG, no caller) compares pointers with pointers.
- Unions of a pointer and an integer (the design asks for each to be
  reviewed). A scan found 13 beside `Vertex`, Op11 and `Gfx`: the room union
  at `bg.c:4328`, which is run-time and reviewed here; and twelve file
  records, recorded for LP64-004 (seven setup link records at
  `bondtypes.h:3321-3683`, the intro's `lang_ptr` pair at `:3878,3883`,
  `pname` at `:4030`, and `struck_anim` at `chr.h:62` and
  `initactorpropstuff.c:88`).
- The 51 incompatible-pointer places that pair a 4-byte word pointer with a
  struct or pointer-to-pointer (`incompat.tsv` in the research folder): read
  each one in a non-strict file (44). A run-time structure viewed as words is
  fixed here by typed access. A file layout is recorded for LP64-004 in
  FORK-089. Add to the hazard inventory what the review finds.

- [ ] **Step 1: Write the failing tests**

`test_stan_callbacks.c` (runs at both widths since Task 2): after the ladder
callback, `stanGetLocusCount` reports the ladder; today that fails at 64
bits. In `probe.c`: the `vtxstore` record's 32-bit size.

- [ ] **Step 2: Fix; task check; commit**

Expected: step 3 exits 0 (the same bytes are stored at 32 bits).

```bash
git commit -m "Size and access run-time structures by their types"
```

---

### Task 9: Prototypes for functions that take pointers

**Files:** the non-strict callers `unprototyped` lists (137 places in about
70 files) and the headers that declare the functions;
`scripts/port/lp64_unprototyped_allowed.txt`; `FORK_CHANGES.md`.

**Interfaces:**
- Each function is declared by including the header that already declares it.
  A prototype is written by hand only when no header has one, in the header
  that owns the function's file, matching the definition exactly. No `extern`
  copies in `.c` files.
- Out of this task: the 20 places in strict-tier files (Task 11), and the
  about 200 names whose definitions take and return only 32-bit values.

- [ ] **Step 1: List the functions and their headers** from
  `unprototyped`, into FORK-089.

- [ ] **Step 2: Add the includes; fix what they reveal**

A prototype turns a silent mismatch into a diagnostic. Fix every newly
visible place in a non-strict file; a wrong-width argument found this way is
a defect at 64 bits and is named in FORK-089.

- [ ] **Step 3: Check the 32-bit program with extra care**

Run the task check. For any function whose callers' code changes, the commit
message states the parameter that caused it and why the new call is the
correct one. If that cannot be shown, leave the function out, keep its
allowlist line, and report it as blocked.

- [ ] **Step 4: Commit**

Expected: the allowlist holds only strict-tier files and any blocked name.

```bash
git commit -m "Declare the pointer-taking functions that were called without a prototype"
```

---

### Task 10: `long` in data layouts

**Files:** `include/PR/gbi.h` (`Hilite`, `Gsetcolor`, `TexRect`), and each
`long` in `src/` and `include/PR/` that lays out data
(`src/bondtypes.h`, `src/aicommands2.h`, `src/bondaicommands.h`,
`src/game/stan.c`, `src/game/image.c`, `include/PR/primage.h`,
`include/PR/ram_ORIGINAL_rom.h`, `include/PR/gu.h`'s `FTOFIX32`, `src/str.c`'s
`ULONG_MAX`); `scripts/port/abi_probe/probe.c`; `FORK_CHANGES.md`.

**Interfaces:**
- A `long` that lays out data the renderer or a file reads becomes a 32-bit
  type at 64 bits and stays `long` at 32 bits (one typedef in the header that
  owns it), so its size is the N64's at both widths. The probe asserts
  `sizeof(Hilite) == 16`, `sizeof(Gsetcolor) == 8` and `sizeof(TexRect) == 16`
  at both widths (today 32, 16 and 32 at 64 bits).
- A `long` used for arithmetic is left alone unless its 64-bit width changes
  a result; list each one read and its verdict in FORK-089.
- Out of this task: the `long` in `model.c` and `bondview2.c` (Task 11), and
  the C library headers under `include/`.

- [ ] **Step 1: Write the failing probe assertions** (they fail at 64 bits today)
- [ ] **Step 2: Fix; task check; commit**

```bash
git commit -m "Keep long-typed data layouts at their N64 size on 64-bit builds"
```

---

### Task 11: The strict batch

Work on a branch: `git switch -c lp64/addr-strict` from current `main`. The
hook prints the STALE-on-branch notice and passes.

**Files (every one a strict-tier gallery input unless marked):**
- `src/game/model.c`, `src/game/gunfire.c`, `src/game/bondview2.c`
- `port/fast3d/gfx_pc.cpp`, `port/fast3d/glad/glad.c`
- `port/src/port_arm_mesh.c`, `port_native_arms.c`, `port_grip_mesh.c`,
  `port_knife_ads.c`
- Not strict, changed with them: `src/bondtypes.h` (`Vertex`),
  `src/game/chr.c` (`:3409`), `src/game/objecthandler.h`,
  `src/game/initunk_005520.c`, `src/game/bondview.h`,
  `scripts/port/harness_widths.sh`, the three allowlists,
  `scripts/port/abi_probe/probe.c`, `FORK_CHANGES.md`

**Interfaces:**
- **`Vertex` is 16 bytes at both widths.** At 64 bits the union at offset 8
  holds a 32-bit slot in place of `LinkedTo` and `CollisionRelatedNode`; a
  macro in `bondtypes.h` resolves it against the model file's base
  (`rodata->BaseAddr`) at its three readers (`model.c:6060-6080`, `:6280`,
  `chr.c:3409`). At 32 bits the struct and the three expressions are as
  today. The probe asserts `sizeof(Vertex) == 16`. What the slot holds after
  conversion is LP64-004's; until then the models guard stops the load.
- **Sentinels.** `model.c:5976`'s promotion test becomes
  `PORT_WORD_HAS_SEGMENT(var, 0x05)`. For `model.c:5916`, first run the i686
  reference smoke with a temporary trace in a scratch build and record
  whether the RAM branch ever runs on PC; then give the test a 64-bit form
  that keeps the measured behaviour, with its reason in a comment.
- **`Model` storage.** At 64 bits `AnimModelSlot`, `ModelSlot` and the
  `hand.field_B68` run hold at least `sizeof(Model)`; `model.c:85` and `:155`
  allocate from `sizeof`, and the read-write data at `model.c:90`
  (`numRecords * 4`) from its element's size. The 32-bit sizes and every 32-bit field offset
  stay; the probe asserts them. The four raw offsets from `g_CurrentPlayer`
  in `bondview2.c` become member addresses at 64 bits.
- **Command words.** The 51 narrowing places use the Task 7 accessors;
  `seg_addr` returns a host pointer before trying the tagged form, at 64 bits
  only; the 5 strict `words.w0` assignments are reviewed; `gfx_pc.cpp:8942`
  and `:8352` print with `PRIxPTR`; at 64 bits a debug-build assertion in the
  display-list walker stops on a command whose `w0` has a non-zero upper
  half.
- **Casts.** The four allowlisted casts in `port_arm_mesh.c` (3) and
  `port_grip_mesh.c` (1) are removed or get a reason naming their milestone.
- **Prototypes.** The 20 strict call places; `port_native_arms.c` includes
  `<string.h>`.
- **`long`.** The one `long` each in `model.c` and `bondview2.c` is handled
  as Task 10 handles the others.
- **Harnesses.** `HARNESS_PENDING` is emptied: `test_ads` and `test_wardrobe`
  pass at 64 bits.

- [ ] **Step 1: `Vertex`; task check; commit**

Write the failing probe assertion first. Expected after the fix:
`test_wardrobe` passes at 64 bits.

- [ ] **Step 2: `model.c` (sentinels, `Model` storage) and `bondview2.c`; task check; commit**
- [ ] **Step 3: `gunfire.c` and the arm files; task check; commit**
- [ ] **Step 4: `port/fast3d`; task check; commit**

After step 4: `narrowing --check` and `unprototyped --check` pass with empty
allowlists apart from any blocked name; delete an allowlist file that is
empty and make its `--check` treat a missing file as empty. For each of the
four commits the Windows comparison is expected to exit 0; a difference means
gallery images may change and must be named in the hand-off.

- [ ] **Step 5: Run the arm and wardrobe regressions**

`scripts/port/test_wardrobe.sh` and `test_ads.sh`, in the i686 container
(both widths run there). Expected: pass at 32 and 64 bits.

- [ ] **Step 6: Try the branch on the runner**

```bash
git push -u origin lp64/addr-strict
gh workflow run windows-release.yml --ref lp64/addr-strict
```

In the tests job's log, confirm the gate, the two static checks and the
harnesses ran and passed. The job is expected to stop at
`hand_gallery.py verify --allow-pending`, because the branch is stale for
strict inputs; that is the rule working.

---

### Task 12: Land through the laptop and close

As run on 2026-11-16 (FORK-089, "LP64-003 task 12"): `main` had not moved, so
no rebase was needed; the laptop's one commit, 9abbe58, is the gallery
regeneration (0 changed image cells); the hand-over lists were assembled in
the final review fix wave before the laptop trip, so Step 3 only set the
status row and corrected one sentence. Steps 2 and 3 ran on the Mac as written.

- [ ] **Step 1: Hand the branch to the laptop**

On the Windows laptop:

```bash
git fetch origin && git switch lp64/addr-strict
git rebase origin/main
python scripts/port/hand_gallery.py generate
python scripts/port/hand_gallery.py verify
```

Review the images. If every Windows comparison in Task 11 was identical, no
image is expected to change; otherwise review the weapons and wardrobes the
hand-off names, both hands, regular and ADS. Then:

```bash
git add review/hand-gallery
git commit -m "Regenerate the review gallery for the strict-tier address fixes"
git switch main && git pull --ff-only
git merge --ff-only lp64/addr-strict && git push origin main
```

This regeneration also clears whatever Tasks 1 to 10 left pending.

- [ ] **Step 2: Confirm on the Mac**

`git switch main && git pull --ff-only`; `hand_gallery.py verify` reports
fresh; the pushed commit's CI passes with the gate, the two static checks,
the harnesses at both widths and the address tests green. Run the task
check's step 5 once more in both containers.

- [ ] **Step 3: Close the record**

Set LP64-003 to Done in the status table with the CI run, the date of the
regeneration, the boot check's output and the hand-over to LP64-004: the ten
guards and how to mark a class converted, the measured boot order, the file
layouts this plan found (`Op11`, `ModelFileTextures`, `SetupIntroCamera`, the
bg env slot, `objecthandler_2.c:304,306`, `bg.c:1116`, `Acmd`), the 19 files
this ROM lacks, and the pool budgets. A documentation-only commit.

---

## What this plan does not do

- It does not make a stage load at 64 bits. The 64-bit game stops at
  `animation tables` when this milestone is done, by design.
- It does not run most of its own fixes at 64 bits inside the game: nothing
  after the first guard executes. Those fixes are covered by ROM-free tests at
  both widths, the static checks and the 32-bit comparison, and get their
  first real run in LP64-004 and LP64-005. A defect that only a running stage
  shows will be found there, not here.
- It does not convert any ROM data, expand a ROM display list or widen
  `Acmd`.
- It does not size memory budgets. Pools name themselves when they run out;
  their 64-bit sizes come with the first stage loads (LP64-005) and the soak
  (LP64-006).
- It does not gate the about 200 unprototyped names that take only 32-bit
  values, the incompatible pointer types, or structs viewed as words through
  explicit casts. The first two are recorded with counts; the third has no
  tool.
- It does not add the player-facing suspend-point notice, crash reporting or
  packaging (LP64-007).

## Decisions for the owner

**The owner accepted all nine recommendations on 2026-11-16.** They are
kept here as put, each with the alternative that was not taken; the plan is
written for the recommendations. Decision 1 changes the design's done
condition for this milestone, and the design records it.

1. **What "done" means.** The design says LP64-003 is done "when a headless
   64-bit build starts and reaches its first asset load". Measured, today's
   build already does that and then crashes in it. *Recommended:* done means
   the 64-bit headless executable passes the startup check with nothing below
   4 GB and exits with status 5 at a guard naming `animation tables` (the
   first conversion the boot reaches), plain and
   under the sanitizers, on aarch64 and x86_64 (Tasks 3 and 4). The
   alternatives: no guards (the build crashes wherever the first conversion
   misreads memory, and LP64-004 starts from a crash); or pull the first
   converter into this milestone to show one real load (it blurs the line the
   design draws, and the next class stops the boot anyway).
2. **Two new CI checks** (Task 1): no implicit narrowing of a
   `uintptr_t`/`intptr_t` value, and no call without a prototype to a
   function that takes or returns a pointer. Both end the milestone at zero.
   They cost about half a minute of CI and make some future code state a
   narrowing with a cast or an accessor. *Recommended:* add both. The
   alternative for the second is `-Werror=implicit-function-declaration` at
   64 bits, which means declaring all 309 names now and risks more 32-bit
   differences.
3. **`Vertex` in this milestone** (Task 11). The design fixes it at 16 bytes
   but no milestone had it, and it is not in the hand-off list. Measured, it
   is 24 bytes at 64 bits and the wardrobe test fails on it. *Recommended:*
   do it here, in the strict batch.
4. **One strict batch, one laptop trip** (Tasks 11 and 12): `model.c`,
   `gunfire.c`, `bondview2.c`, two files under `port/fast3d` and four arm
   files, on `lp64/addr-strict`. The arm files are the fork's hand code; the
   edits are accessor and type changes and the Windows comparison is expected
   to be identical, so no image should change. *Recommended:* as planned. If
   you would rather not touch the arm files in this milestone, their 32
   narrowing places stay in the allowlist until LP64-004's display-list work
   reaches them.
5. **No low-address development mode.** A 64-bit build with its image and
   arenas below 4 GB would let truncated pointers work by accident and might
   seem a way to run more of the game sooner. Measured, the first crash is a
   layout, not a truncation, and is the same with the arenas low or high.
   *Recommended:* none; the design also forbids depending on low addresses.
6. **Items moved to LP64-004 against the hand-off list:** `Acmd` (the design
   says "when the audio class is converted"), the Op11 union,
   `SetupIntroCamera`'s walk and the bg env slot (all read from files).
   *Recommended:* follow the design.
7. **Suspend points:** the mechanism now (Task 5), the notice in the menus
   and the manual in LP64-007. *Recommended:* as planned.
8. **Budgets:** only names on exhaustion now; sizes with LP64-005 and
   LP64-006. *Recommended:* as planned.
9. **For LP64-004's plan, not this one:** the boot needs animation tables,
   text banks, fonts, then audio banks and music, before any stage load. The
   design's class order has animation tables fourth and audio ninth. Noted
   by the owner; LP64-004's plan settles the order.
   (Note from Task 4: measured, fonts load after the animation table's
   relocation; FORK-089's Task 4 note has the measured order.)
