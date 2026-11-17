# LP64-004 Asset Converters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Planned on 2026-11-16 from measurements on `main` at f73382a and
accepted by the owner the same day, with all ten recommendations under
"Decisions for the owner" at the end. The status table of
[native_64bit_plan.md](native_64bit_plan.md) records what is built and
verified as the tasks land. The tasks are written for the
recommendations.

**Goal:** A 64-bit headless build that converts every class of ROM data it
loads, so that it boots through the title and the menus and loads and runs
each of the 20 solo missions, without changing the 32-bit program.

**Architecture:** The ten asset classes are converted one at a time in the
order the boot reaches them, which the measurements below establish and
which differs from the design's list. Each class gets a file layout (structs
of fixed-width fields that match the ROM bytes), a converter in `port/src/`
that is compiled only into 64-bit builds and into a 32-bit test build, a
ROM-free test on input built in the test, and a run of the real 64-bit
executable showing its stop move to the next class. Where a file's records
cannot grow in place the converter writes the run-time layout into new
memory; where the measurements show the bytes can stay (text banks, tile
data, animation streams) they stay, and that is said per class. On a 32-bit
build every in-place path is untouched, and the converters run only in an
opt-in oracle build that compares their output with the in-place result for
every file. Work in the gallery's strict-tier files is not planned; whatever
the first 64-bit runs make necessary there is batched on one branch and
lands through one regeneration on the Windows laptop.

**Tech Stack:** C (GCC 13.3, Ubuntu 24.04 containers), CMake, Python 3
standard library, Bash test harnesses, Docker Desktop on Apple Silicon.

**Spec:** [native_64bit_plan.md](native_64bit_plan.md): "Design > ROM data:
file layouts and converters", "Display lists", "Memory budgets", "Addresses"
(the `Acmd` item), verification items 1, 2, 3, 4, 8 and 10, milestone
LP64-004 and the hazard inventory ("In-place conversions to replace",
"Display-list stride and layout", "Hard-coded sizes"). Read it first, then
`AGENTS.md`, [the LP64-003 plan](native_64bit_lp64_003.md) for the task
check and the rules this plan continues, and in `FORK_CHANGES.md` (FORK-089)
the list headed "LP64-003 hand-over to LP64-004".

## What was measured for this plan (2026-11-16, `main` at f73382a)

All runs used the Mac's containers (`lp64_env.py`, GCC 13.3). aarch64 is
native; x86_64 and i686 run under emulation, so nothing seen only there is a
finding yet (the design's rule). The scratch copies were `git archive`s of
`main` under `build/lp64`, never committed and deleted afterwards.

- **Where `main` stands.** The 64-bit headless executable logs
  `ge: lp64: address space OK` and exits with status 5 at
  `asset class "animation tables"`. The ten guards are where the hand-over
  list says, at the same lines.
- **Boot order of the ten classes.** In a scratch copy each class was
  treated as converted in turn, with the stand-in described below, and the
  executable run to its next stop (with a backtrace at the stop). The order,
  the same in the aarch64 and the x86_64 container:

  | # | Class | Reached from | Design's place |
  |---|---|---|---|
  | 1 | animation tables | `portAnimDataInit`, before the game's boot; the relocation runs at `boss.c:282` | 4 |
  | 2 | text banks | `langInit` (seven banks) | 1 |
  | 3 | audio banks | `musicSeqPlayerInit`, `alBnkfNew` twice | 9 |
  | 4 | music sequences | the same function | 10 |
  | 5 | textures | the title's `lvlStageLoad`: `texReset`, `texLoadFromDisplayList` (`image_bank.c:240`) | 3 |
  | 6 | fonts | the same stage load: `load_font_tables` (`lv.c:802`) | 2 |
  | 7 | models | the legal screen: `init_menu00_legalscreen`, `load_object_fill_header` (`front.c:1742`) | 5 |
  | 8 | backgrounds | a mission's `load_bg_file` (`bg.c:1100`) | 8 |
  | 9 | collision tiles | the same function (`bg.c:1111`) | 6 |
  | 10 | setup files | `proplvreset2` (`prop.c:1950`) | 7 |

  Stops 1 to 7 were run. Stops 8 to 10 are by reading `lv.c:835-931`: no
  stand-in for models was small enough to write (see below). `GE_STAGE`
  does not skip the title: its stage load and the legal screen's model load
  always run first.
- **Stand-ins, and what each showed.** These are scratch code, not the
  converters; they are recorded because each fixed what the class needs.
  1. *Animation tables.* A file header is 0x14 bytes at
     `data + PTR_ANIM_x`; 83 sites in six files form that address from a
     constant, so headers cannot move. The 173 headers are at least 0x30
     apart, the last at 0xE7C0 of 0xE7E0 bytes, and 102 of them sit at
     4 mod 8. A run-time `ModelAnimation` written over each header in the
     permanent copy works when it is declared packed and 4-byte aligned
     at 64 bits (its read fields then end at 0x20), with the descriptor and
     stream pointers into the static staging image, which keeps the file
     layout. No reader in `model.c` changed. Without the packing the
     sanitizer build stops at a misaligned member access.
  2. *Text banks.* The table kept as native 4-byte offsets and `langGet`
     adding the bank's address, which is the N64 branch of that function.
     The three relabelled strings fit in their own strings' bytes.
  3. *Audio banks.* New records for the five kinds that hold pointers; the
     other five stay in the file image. 448 records and 16,448 new bytes
     for the effects bank, 312 and 11,216 for the instruments.
  4. *Music sequences.* The 63 file entries of 8 bytes widened to 16-byte
     `RareALSeqData` (the table grows from 508 to 1,016 bytes).
  5. *Textures.* Treated as converted with no change: `texReset` ran
     clean, also under the sanitizers.
  6. *Fonts.* `struct font` built at its 64-bit size (2,932 to 3,688
     bytes) and the font list widened (0xC0 to 0x180 bytes).

  With all six the plain and the sanitizer executable reach the legal
  screen's model load and stop at `models` with status 5 and no sanitizer
  report. With `models` also treated as converted and no converter, the
  boot dies in `modelPromoteNodeOffsetsToPointers` (`model.c:6006`).
  One lesson cost three runs: a staging array read through the game's own
  `extern` of it (declared as one `u8` or one pointer) trips the
  sanitizer's object-size check, so converters read staging data through
  `port/src` declarations or after `romCopy`.
- **Sizes** (32 to 64 bits, by a probe compiled at both widths): `ModelNode`
  24 to 48, `ModelFileTextures` 12 to 16, the records Header 16 to 24,
  Group 28 to 40, DL 20 to 40, DLCollision 32 to 64, DLPrimary 16 to 32,
  LOD 16 to 24, BSP 36 to 48, Gunfire 40 to 48, Shadow 32 to 48, Switch
  8 to 16, Op11 76 to 144, Op05 424 to 448, Op07 432 to 464, Op06 24 to 32,
  Op17 32 to 40 (BBox 28, GroupSimple 20 and Interlink 28 unchanged);
  `bg_room_data` 24 to 40, `bg_portal_data_entry` 8 to 16; `stagesetup`
  40 to 80, `PadRecord` 44 to 56, `BoundPadRecord` 68 to 80, `waypoint`
  16 to 24, `waygroup` 12 to 24, `PathRecord` and `AIListRecord` 8 to 16,
  `pname` 4 to 8, `ObjectRecord` 128 to 144; `fontchar` 24 to 32;
  `ALBankFile` 8 to 16, `ALBank` 16 to 24, `ALInstrument` 20 to 24,
  `ALSound` 16 to 32, `ALWaveTable` 20 to 32; `RareALSeqData` 8 to 16;
  `Gfx` 8 to 16; `Acmd` 8 at both (its words are 32-bit).
- **What a converted class costs.** A 32-bit headless build with counters
  in the three file walkers ran the 20-mission smoke (20 of 20 `PASS`), and
  the growth was computed from the record counts and the sizes above:

  | Class | 32-bit bytes | 64-bit growth | Where it comes from |
  |---|---|---|---|
  | models, per mission (22 to 55 files in 600 frames) | 178 to 397 KB | 34 to 82 KB (1.16 to 1.26 times) | stage bank, or the caller's buffer |
  | models, the 280 distinct files of the smoke | 1,921,952 | 396,168 (1.21 times; worst file 1.55) | |
  | setup file, per mission | 24 to 103 KB | 4 to 17 KB (1.16 to 1.19 times) | stage bank |
  | bg tables (at most 137 rooms, 194 portals) | 3.3 and 1.6 KB | 2.2 and 1.6 KB | stage bank |
  | room display lists, per stage (at most 114,992 bytes, Silo) | up to 115 KB | the same again | the room block (`mema`) |
  | fonts | 2 x 2,932 | 2 x 756, and 0xC0 for the list | stage bank, permanent bank |
  | audio banks | 40,800 | 27,664 | music heap |
  | music table | 508 | 508 | music heap |
  | animation tables, text banks, tile data | | 0 | |

  Over all 512 model files of the ROM the estimate is 3.29 MB to 4.01 MB
  (58,041 list commands). Three callers load a model into a fixed buffer:
  the largest converted files are estimated at 42,648 of 45,008 bytes
  (`GtriggerZ`, `gun.c`'s 0xAFD0), 46,052 of 48,496 (`Csuit_lf_handZ`,
  0xBD70) and 26,744 of 30,000 (`GautoshotZ`, 0x7530), padding not counted.
- **The pools those bytes come from**, at 32 bits. The stage bank on PC is
  66,805,760 bytes and between 62.99 and 63.51 MB of it is free at frame 60
  of every mission. The permanent bank is 303,104 bytes with 189,280 free.
  The music heap is a static 753,664 bytes with 219,120 used. The graphics
  buffer's frame-end use peaked at 30,872 bytes (35,960 in Surface 2) of
  122,880 to 204,800, in a headless smoke; its unit is display-list
  commands, which double.
- **Room lists** (a read-only scan of the 25 bg files with data): every
  inflated list is a multiple of 8 bytes; the only address word is `G_VTX`
  with segment 0x0E, which the renderer and the walkers resolve by the
  segment, so it stays a segment word; no list calls another. The list's
  length is the return value of `bgDecompress`; there is no size field.
- **Model lists** (a scan of the 512 files): no list calls another in the
  stock ROM; address words are `G_MTX` (segment 3), `G_VTX` (segment 5 in
  DL nodes, always inside that node's vertex array but 2,194 of 2,805 not
  at its start; segment 4 in the other two kinds) and 30 `G_SETTIMG` to an
  inline texture. Opcodes 3, 5, 6, 7, 11, 14, 16, 17, 19 and 20 never
  occur, so their conversion cannot be checked against the ROM.
- **The hand-over list**, every item checked against the code: the table
  under "Hand-over items checked" below.

### What could not be measured

- Any stage load at 64 bits, and so the order of stops 8 to 10, the first
  run of the model, background, collision and setup paths, and every
  64-bit fix of LP64-003 that sits behind them.
- Stand-ins for models, backgrounds, collision tiles and setup files: each
  needs a real converter, so there is no figure for "how far the boot gets"
  past the legal screen.
- The 64-bit video executable at run time (no display in the containers),
  and so the mixer: a headless build drops `audio_pc.c` and never emits an
  audio command. `Acmd` can be tested only through its interpreter test.
- Graphics buffer and room memory use in real play (the smoke is 600
  frames standing at the start), and the stage bank's use in multiplayer.
- Whether the 32-bit game's second load of a stage sees what play wrote
  into the bg tables on the first (Task 12 measures it).
- Real x86 hardware, `clang`, GCC 14, and the browser build (CI only).

## The ten classes

"In place" means the bytes stay where the loader put them. "Reader" is game
code that reads the converted data. Every 64-bit branch below sits behind
`IS_64_BIT`; the 32-bit build keeps the code it has, token for token.

| Class | File has | 64-bit run time needs | Converter produces | When |
|---|---|---|---|---|
| animation tables | 0x14-byte headers with three 4-byte offsets, inside a 0xE7E0-byte segment; streams elsewhere | `ModelAnimation` with three 8-byte fields at the header's own address | each header rewritten in place in the permanent copy (packed layout, 0x20 bytes of the 0x30 available), pointing into the staging image | boot |
| text banks | a table of 4-byte offsets, then strings | nothing pointer-sized if the reader adds the base | the table in native byte order, offsets kept; `langGet` adds the bank address | each bank load (boot, stage load, briefing) |
| audio banks | five record kinds with 4-byte offsets, five without | the five as C structs with pointers | a new graph of the five from the music heap; the rest stay in the file image | boot |
| music sequences | 4-byte header, 8-byte entries | 8-byte header, 16-byte entries | the table at its run-time size, offsets in pointer slots for `musicSeqFileNew` | boot |
| textures | nothing read through a pointer-sized struct | host `Gfx` in and out of `texLoadFromGdl`; `struct tex` by its type | the display-list expander and two stride fixes in `tex.c` | load and first use, as today |
| fonts | 169 words, 94 records of 6 words, glyph bytes; a 24-command list | `struct font` 756 bytes larger; 16-byte commands | the staging images built at the 64-bit layout, pixel offsets in pointer slots for the game's own fix-up | once, at the first stage load |
| models | 4-byte switch slots, 12-byte texture entries, 24-byte nodes, records, 8-byte commands, all cross-referenced by segment-5 offsets | every one of those at its C size, references remapped | a new image in the same buffer, references as `0x05000000` plus new offset for the game's own promotion | every model load, including during play, as today |
| backgrounds | 24-byte room records and 8-byte portal records inside the ROM image; 8-byte commands in compressed room data | 40- and 16-byte records; host `Gfx` | the two tables in the stage bank; each room list expanded as it is inflated | stage load; each room load, as today |
| collision tiles | a two-word header, room offset slots, tiles | a 16-byte header of two pointers | the header written over the first 16 bytes; tiles in place | stage load |
| setup files | a 10-offset header, eight tables, prop and intro records used in place as live objects | every record at its C size (most prop record types grow), tables at theirs | a new image, references remapped, sizes from one table `sizepropdef` also reads | stage load |

Three formats carry no pointers and are only confirmed (Task 1): briefings
(24 half-words), AI command lists (bytes; the table that points at them is
the setup class's), and fog tables (read field by field). A fourth is not
what the design assumed: the demo header `ramromfilestructure` has no
pointer but is 228 bytes at 32 bits and 232 at 64 (two `u64` fields raise
its alignment), and the game steps past it by `sizeof`. Task 1 fixes its
size. *Note (built on 2026-11-16):* Task 1 found that the cart's demo header is 0xE8 bytes, the struct's size at 64 bits and with MinGW (i686 Linux's 228 is the exception), and that playback already steps past it by `RAMROM_HEADER_ROMSIZE` (0xE8, `ramromreplay.c`), so nothing was changed in `ramromreplay`, and `RomDemoHeader` is 0xE8 bytes.

## Hand-over items checked

Against `main` at f73382a. "True" means the item still describes the code.

| Item of the hand-over list | Still true? | What this plan does with it |
|---|---|---|
| The ten guards and their places | True, every line as listed | Each class's task marks it converted; Task 17 says what remains of the guards |
| Boot order: byte-order pass, text banks, the animation relocation, "fonts somewhere after it", audio and music, the title's stage load | Corrected: fonts load inside the title's stage load, after the textures; models follow at the legal screen | The order of the tasks |
| Op11 record, fields move from 0x40 to 0x80 | True (76 to 144 bytes); opcode 11 never occurs in the ROM | Task 10, covered by a fixture only |
| `ModelFileTextures` 12 in the file, 16 in C | True (`romfile_model.c:414,417,438`) | Tasks 10 and 11 |
| `objecthandler_2.c:302,305` place `Switches` and `Textures` by `s32` strides | True, now `:302-307` | Task 11 |
| `bg.c:1124` prints `((u32 *) gptr_stan)[1]` | True; a trace | Task 13 |
| `prop.c:1523`: `SetupIntroCamera` is 14 words, the intro has 10 | True (56 bytes at 64 bits; `kSetupIntroKinds[6].words` is 10) | Task 14 |
| The bg env slot "cannot hold a host pointer" | True, and it need not: it is a file slot that holds an offset, then a portal index (`bg.c:1222-1227`) | Stays in the file image (Task 12) |
| `Acmd`: nine `abi.h` macros truncate | True; eight have callers | Task 7 |
| The twelve file-record unions | True (`bondtypes.h:3340-3349`, `3604-3613`, `3694-3705`, the intro camera's two, `pname`, the two firing tables') | Tasks 14 and 15; the firing tables are run-time data and stay |
| `Switches[n]->Data` read as `f32 *` (`chraction.c` two, `propobj.c` one) | True, and wider: `propobj.c` has two (`:6084`, `:6126`), and `bondview2.c`, `gunfire.c`, `prop.c:759` and `ads_hands.c:550` do the same | The records read (Group, GroupSimple, Interlink, BBox) keep their floats at offset 0 at 64 bits; pinned by the layout probe (Task 10) |
| The setup prop walk by `sizepropdef`; setup records returned as `ObjectRecord *` | True (`cleanup_objects.c:21,83`, `loadobjectmodel.c:274,303,331,587`; three returns) | Task 15 |
| `expand_ani_table_entries` relocates in place | True (`initanitable.c:268-269`) | Task 3 |
| The 19 files this ROM lacks; "their compiled-in copies are not linked" | Half true: the eight bg files are zero-length on the cart (cut stages), three more alias one of them, and all eleven resolve to 64 KB zero arrays that are linked at 64 bits (`asset_bin_stubs.c`). The eight stans are not linked | The guard in `load_bg_file` keeps stopping them; decision 7 |
| The pools and what each does when full | True, with one correction: on PC the music heap is a static array, not the permanent bank | Task 2 makes the silent ones speak at 64 bits |
| Collision vertices: unswap, `LinkOffset`, index | True, with two precisions: the 32-bit code matches only links below 0x10000, and the target is a node, so the offset is the node's new one | Task 10 |
| The unprototyped allowlist (`texSelect`, `strcmp`) | True, two lines | Carried unchanged |
| What no tool sees (`-Wno-incompatible-pointer-types`) | True (`CMakeLists.txt:60`) | The converter sources compile with that warning as an error (Task 1) |
| Unexecuted checks (`GE_LP64_DL_CHECK`, `seg_addr`) | True, and still so after this milestone: a headless build has no renderer | Hand-over to LP64-005 |
| Untested until a guard lifts | True | `portTexReplaceRom`: Task 8. The crouch branch: Task 13. The arm mesh's alias cases: LP64-005 (video) |
| `ModelAnimation.address` must be zero-extended | True; file values are at most 0xFFFAC | Task 3 |
| `FTOFIX32`, `watchRenderController`, the macOS notes, the two `atof` reads | Unchanged | Carried to LP64-005, LP64-006 and the macOS project |

Found while checking, not in the list:

- `ALInstrumentAlt_s` (`snd.h:160-178`), through which `sndPlaySfx` indexes
  the effects bank, has its array at 12 at 32 bits and at 16 at 64. At 32
  bits its index k is the standard record's k - 1; at 64 bits it is k, so
  every effect would play its neighbour. (Task 5)
- `ramromfilestructure` is 228 and 232 bytes. (Task 1) *Note (built on 2026-11-16):* Task 1 found that the cart's demo header is 0xE8 bytes, the struct's size at 64 bits and with MinGW (i686 Linux's 228 is the exception), and that playback already steps past it by `RAMROM_HEADER_ROMSIZE` (0xE8, `ramromreplay.c`), so nothing was changed in `ramromreplay`, and `RomDemoHeader` is 0xE8 bytes.
- `initSetLevelLoadPropSafeItem` (`initobjects.c:191`) writes `prop` of an
  `ObjectRecord` where the list's reader expects `SafeObjectRecord.next`;
  the two share an offset at 32 bits only. (Task 15)
- The intro credits record keeps a pointer in an `s32`
  (`bondview_r.c:380`). (Task 14)
- `texLoadFromGdl` counts commands as bytes `>> 3` (`tex.c:846`) while its
  model caller passes `n * sizeof(Gfx)`; `texCopyGdls` (`tex.c:1177`)
  copies two `u32` a command into `Gfx` steps. (Task 8)
- One instrument sample is 16-bit raw, and `alBnkfNew` swaps its samples
  in place inside the ROM image; `audio_pc.c:31-33` says both banks are all
  ADPCM. Behaviour kept; the comment is corrected. (Task 5)
- `lvInit` copies the font list before `portFontBuildSegments` has filled
  it, at both widths, so the copy is zeros; nothing on PC reads it (the
  three lists are called by symbol). Kept as it is. (Task 9)
- A missing stan is read by `stanDetermineEOF` before any guard.
  (Task 13)

## What LP64-004 owns, and what it leaves to LP64-005

LP64-004 owns the ten converters, their file layouts, the display-list
expander, `Acmd`, the oracle build, and the first stage loads: the title,
the menus as far as the boot walks them, and the 20 solo missions for 600
frames each, headless.

LP64-005 owns everything played beyond that: the attract demos, every
stage including multiplayer and the cut ones, the video executable, the
mixer at run time, pool sizes from longer runs, the renderer's first walk
of a 64-bit display list, and the late-load audit of a 64-bit build.

Two items straddle the line and are placed deliberately:

- A pool whose unit is the display-list command is scaled here (the
  graphics buffer, Task 2, decision 4), because at 64 bits it holds half as
  many commands and nothing checks its end. Every other pool size stays
  LP64-005's and LP64-006's, as the owner decided for LP64-003.
- Defects the first stage loads expose are fixed here when they stop a
  mission from loading or running its 600 frames (Task 16). A difference
  from the 32-bit game that does not stop it is recorded for LP64-006.

## Global Constraints

- **The 32-bit program does not change.** Before every commit,
  `python3 scripts/port/compare_win32.py commits HEAD WORKTREE` exits 0.
  Where it exits 1, the rule and the accepted kinds of difference are those
  of [the LP64-003 plan](native_64bit_lp64_003.md#global-constraints),
  unchanged.
- **Converters are not in a shipped 32-bit build.** `port/src/romconv_*.c`
  and `port/src/port_lp64_oracle.c` are compiled at `GE_POINTER_BITS=64`,
  and at 32 bits only with `-DGE_LP64_ORACLE=ON`, which is refused outside
  a headless build. Every call of a converter in shared code is inside
  `#if IS_64_BIT` or `#ifdef GE_LP64_ORACLE`, with the existing code in the
  other branch, unedited.
- **Converters are correct at either width** (the design). A converter
  writes the run-time layout through the C types and `sizeof`, so at 32
  bits its output is the in-place result. Where the 64-bit representation
  is deliberately different (the animation header's packing, `Vertex`'s
  `LinkOffset`, text offsets) the converter's 32-bit branch produces what
  the in-place path produces, and the plan says the 64-bit branch is
  covered by fixtures only.
- **Destination sizes are never guessed silently** (the design). A
  converter that writes new memory measures first: its first pass returns
  the output size, the caller checks it against the room it has, and only
  then is anything written. Too little room is reported with the file's
  name and both sizes.
- **A converter never reads what it is writing.** One that rewrites a
  buffer in place reads from a copy (`portConvScratch`, Task 1).
- **Renderer formats are fixed**: `Vtx`, the game's `Vertex`, `Mtx`, `Vp`
  and the light structs keep their sizes; the layout probe asserts them.
- **Display lists** (the design): every list a walker or the renderer
  reads is host `Gfx`; a command's upper half is zero; code steps by
  `sizeof(Gfx)` and reads words through `port/include/port_gfxwords.h`.
  Segment words stay segment words where the reader resolves the segment
  (0x0E in rooms, 3, 4 and 5 in models).
- **No design may depend on low addresses**, and nothing here adds a hint,
  a fixed base or a `-no-pie` link at 64 bits.
- **Fix declarations, not call sites; no laundering.** A new truncating
  round-trip cast needs its line in `scripts/port/lp64_casts_allowed.txt`
  with the milestone that removes it. The narrowing and unprototyped
  checks stay at their counts (0 and 2).
- **Upstream-owned files get small mechanical edits**: one branch on
  `IS_64_BIT` around a call, a `sizeof` for a literal, a type for a cast.
  New logic goes in `port/`. FORK-089 records each file touched.
- **Strict-tier files** (`src/game/gunfire.c`, `model.c`, `bondview2.c`,
  everything under `port/fast3d/`, `port/src/port_arm_mesh.c`,
  `port_native_arms.c`, `port_grip_mesh.c`, `port_wardrobe.c`,
  `ads_hands.c`, `port_knife_ads.c`, `port_graphics.c`, `port_shiny.c`,
  `port_weapon_images.c`, `port_hud_weapons.c`, `joy_port.c`,
  `scripts/port/hand_gallery.py` and its template) are changed only on the
  branch `lp64/conv-strict` (see "The strict batch"). No task plans an
  edit to one.
- **Load before play** (`AGENTS.md`). No converter moves work to first use:
  each runs where the in-place conversion it replaces runs. Models and
  rooms are already converted at their loads during play at 32 bits; the
  64-bit conversions do more work at the same moments, and their cost is
  measured in LP64-005, when a 64-bit build can run the late-load audit.
  `portConvScratch` is sized at boot so that no model load of the stock
  ROM allocates during play.
- **No ROM-derived data in Git**, fixtures included: every test input is
  built in the test from numbers written there. Python uses the standard
  library only. New source files stay under 500 lines. Shell scripts start
  with `#!/usr/bin/env bash` and `set -euo pipefail`. Build output goes
  under `build/lp64/` and is deleted before each push.
- **Every commit updates `FORK_CHANGES.md`** (FORK-089): what changed, the
  check counts, what ran, and for a converter the oracle's line.
- Delivery is direct to `main`, one push per task after its review is
  clean, except commits on `lp64/conv-strict`. Never use
  `git commit --no-verify`. Commit messages end with
  `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`.

### The strict batch

No task plans a strict-tier edit: the two classes whose readers are in
`model.c` (animation tables and models) are converted so that its code runs
unchanged, which the animation stand-in showed for the first. The first
64-bit runs of the model and gun code may still find defects there.

- A commit that touches a strict-tier file goes to `lp64/conv-strict`,
  never to `main`. From its first commit the branch is rebased on `main`
  after every push to `main` (`git switch lp64/conv-strict && git rebase
  main`), and every 64-bit run of the task check is made on the branch.
- A class is marked converted on `main` only if `main`'s own executable
  then passes that task's boot check. If it needs a strict-tier fix to do
  so, the line in `port/src/port_lp64.c` is changed in the same branch
  commit as the fix, and `main` keeps stopping at that class until the
  branch lands.
- Each strict-tier commit leaves the Windows executable identical, or
  names every changed function for the laptop, as in LP64-003.
- The branch lands once, in Task 17, through one regeneration on the
  Windows laptop.

### Task check

Every task ends by running this and recording the results in FORK-089.
Steps 5 to 7 exist from the task that creates them; `STOP` is the class the
task says the boot now stops at, or `--expect-run` once it runs.

```bash
# 1. The 64-bit compile gate and the two static checks, in the x86_64
#    container; also in the aarch64 container for a change to C or the build.
python3 scripts/port/lp64_env.py run --arch x86_64 -- sh -c '
  cmake -S . -B build/lp64/gate -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate --target ge_buildinfo &&
  cmake -S . -B build/lp64/gate-video -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_VIDEO=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON &&
  cmake --build build/lp64/gate-video --target ge_buildinfo &&
  python3 scripts/port/lp64_diagnostics.py gate build/lp64/gate build/lp64/gate-video &&
  python3 scripts/port/lp64_diagnostics.py narrowing --check build/lp64/gate build/lp64/gate-video &&
  python3 scripts/port/lp64_diagnostics.py unprototyped --check build/lp64/gate build/lp64/gate-video'

# 2. The 32-bit game still builds and plays (i686, headless, with the ROM).
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c 'cmake -S . -B build/lp64/ref-i686 -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON && cmake --build build/lp64/ref-i686 -j"$(nproc)"'
python3 scripts/port/lp64_env.py run --arch i686 -- python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/ref-i686/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 2 --output build/lp64/ref-i686/smoke.json

# 3. The shipped Windows program is unchanged (before committing).
python3 scripts/port/compare_win32.py commits HEAD WORKTREE

# 4. The 64-bit executable boots to its expected stop, plain and under the
#    sanitizers (aarch64, native).
python3 scripts/port/lp64_env.py run --arch aarch64 -- sh -c '
  cmake -S . -B build/lp64/boot -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_HEADLESS=ON && cmake --build build/lp64/boot -j"$(nproc)" &&
  cmake -S . -B build/lp64/boot-san -DCMAKE_BUILD_TYPE=Debug -DGE_POINTER_BITS=64 -DGE_SANITIZE=ON && cmake --build build/lp64/boot-san -j"$(nproc)" &&
  python3 scripts/port/lp64_boot.py --rom /rom/baserom.u.z64 --expect-stop "STOP" build/lp64/boot/IronSights build/lp64/boot-san/IronSights'

# 5. From Task 1: the converter oracle, 32-bit, with the ROM.
python3 scripts/port/lp64_env.py run --arch i686 -- sh -c '
  cmake -S . -B build/lp64/oracle -DCMAKE_BUILD_TYPE=Debug -DGE_HEADLESS=ON -DGE_LP64_ORACLE=ON && cmake --build build/lp64/oracle -j"$(nproc)" &&
  python3 scripts/port/lp64_oracle.py --rom /rom/baserom.u.z64 --classes "CLASSES SO FAR" build/lp64/oracle/IronSights'

# 6. From Task 11: the 64-bit smoke (aarch64), from the mission the task names.
python3 scripts/port/lp64_env.py run --arch aarch64 -- python3 scripts/port/audit_buffers.py --mission-smoke build/lp64/boot/IronSights --rom /rom/baserom.u.z64 --frames 600 --jobs 4 --output build/lp64/boot/smoke.json

# 7. From Task 16: the load census, 32 against 64 bits.
python3 scripts/port/lp64_census.py compare build/lp64/ref-i686 build/lp64/boot
```

Expected: step 1 prints `0 failed` for both trees, `OK: 0 narrowings in 0
files` and `OK: 2 unprototyped calls in 2 files`; step 2 exits 0 with 20 of
20 `PASS`; step 3 exits 0 or the commit explains each changed function;
step 4 prints `OK: stopped at "STOP"` (or `OK: ran N frames`) for each
executable; step 5 prints, for each class, `N files, 0 differ` with N above
zero, and exits 0; steps 6 and 7 as their tasks say. Also run every harness
line of the CI tests step (`Test gameplay, cache and release tooling
without a ROM` in `.github/workflows/windows-release.yml`), in the i686
container where a line needs `-m32`. Run step 4 once in the x86_64
container at each task that moves the stop. The browser build is checked by
CI after the push; a red browser build is the task's to fix.

*Note (2026-11-17, Task 16 review):* from Task 16 on, steps 2, 5 and 6 run
their missions with the smoke's `--fixed-tick` and `--frames 1800`
(`audit_buffers.py ... --frames 1800 --fixed-tick`, `lp64_oracle.py ...
--frames 1800 --fixed-tick`), and step 7 compares those step 2 and step 6
folders. The census compares two widths that must play the same game fields,
and the emulated 32-bit run passes more fields a frame on the game clock; one
field a frame for 600 frames covered less than the game-clock runs (no weapon
load in three missions, fewer oracle textures), so 1800 was measured as the
frame by which every mission has loaded its first-person weapon (Dam, the
last, by frame 1200) with a margin. FORK-089, "LP64-004 task 16", has the
measurement and the commands verbatim. The commands above are unchanged.

### Behaviour-sensitive sites

- `langGet` (`language.c:413`): at 64 bits it adds the bank's address to a
  non-zero slot, the expression its `TARGET_N64` branch has. The 32-bit
  return stays the slot's value. A slot of 0 stays "not loaded". (Task 4)
- The three relabelled strings (`sTextPatches`, `romdata.c:877`) are
  outside the bank. At 64 bits each is written over the string it
  replaces, which must be at least as long and referenced by no other
  slot; otherwise the bank's load fails naming the slot. (Task 4)
- `ALInstrumentAlt_s`: its 32-bit layout stays. At 64 bits its array must
  start one pointer before `ALInstrument.soundArray`, so that index k is
  still record k - 1. (Task 5)
- `alBnkfNew` swaps a raw sample's bytes inside the ROM image, once. The
  64-bit converter does the same, once, guarded as today. (Task 5)
- `texLoadFromGdl`'s `srcsize >> 3` becomes a count of host commands. At
  32 bits `srcsize / sizeof(Gfx)` is the same instruction; the Windows
  comparison shows it. (Task 8)
- The texture pool's "nearly full" thresholds (`image.c:2632`, 0x10CC and
  0xA28) are literals, and each record grows by 8 bytes at 64 bits. Read
  before changed: a threshold is raised only if the margin it leaves is
  shown to be too small. (Task 8)
- The model promotion (`modelPromoteNodeOffsetsToPointers`, strict tier)
  is not edited: the converter gives it `0x05000000 | offset` in
  pointer-sized slots. Its 32-bit loop over collision vertices stays; at
  64 bits the converter has already written `LinkOffset` and the index.
  (Task 10)
- `VERTEX_COLLISION_NODE` reads `LinkOffset` 0 as "no link". The converter
  refuses a link whose target lands at offset 0 and reports the file; the
  oracle sweep shows none in the ROM. (Task 10)
- `port_arm_mesh.c:217` (strict tier, not edited) accepts a vertex word
  only with segment 5. Converted lists keep segment words; a converter
  that wrote host addresses there would remove the hands. (Task 10)
- The bg tables: at 32 bits they live in the ROM image and keep what play
  wrote into them when the stage loads again; at 64 bits they are built
  fresh at each load, as on the N64. (Task 12, decision 6)
- `BG_SEG_IS_OFFSET`, `< PORTMAX` and `SETUP_PROMOTE` are full-width and
  stay as they are; the converters give them offsets in pointer-sized
  slots. (Tasks 12, 14)
- A prop record never set up has `prop == NULL`, which
  `cleanup_objects.c:51` and `loadobjectmodel.c:562` rely on: every
  pointer slot the converter adds is zero unless the file's word is not.
  (Task 15)
- `sizepropdef` returns words. At 64 bits it returns the converted
  record's size in words from the converter's own table, for every type,
  padded to a multiple of two. Its 32-bit body stays. (Task 15)

## Review Focus

1. A converter leaks into the 32-bit program (a shared file calls it
   outside the two guards, or CMake lists it). Expected: the Windows
   comparison differs and the commit is refused. (Task check, step 3;
   Task 1's CMake test.)
2. A converter writes past the room it was given. Expected: its measuring
   pass reports the size first and the caller refuses, naming the file;
   the fixture with a one-byte-short buffer fails cleanly, and the
   sanitizer test build sees no overrun. (Every converter task.)
3. A converter is right for the stock ROM and wrong for a record the ROM
   does not have (model opcodes 3, 5, 6, 7, 11, 17 and 20; a list that
   calls another; a text relabel that does not fit). Expected: a fixture
   builds each such input and asserts the output, or asserts a named
   refusal. (Tasks 4, 10.)
4. A reference is left pointing at the old layout (an offset the remap
   missed). Expected: every reference field goes through one lookup that
   fails on a miss, and the fixture has one record of every kind that
   holds a reference. (Tasks 10, 14, 15.)
5. A class is marked converted while a reader still assumes the file
   layout. Expected: the boot check crashes or the sanitizer build
   reports, and the task does not close. A reader the boot does not reach
   (a record type no mission of the smoke loads) is the residual risk; the
   census of Task 16 lists which record types the smoke exercised.
6. The 64-bit branch of a converter differs from the 32-bit one and only
   the 32-bit one is checked by the oracle. Expected: each such branch is
   named in its task and has its own fixture assertions at 64 bits.
7. A pool is too small at 64 bits and fails quietly. Expected: Task 2's
   line in the log, which the boot check and the smoke treat as a failure.

---

### Task 1: The converter interface, the oracle build and the boot check's run mode

**Files:**
- Create: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `port/src/romconv_common.c`, `port/include/port_lp64_oracle.h`,
  `port/src/port_lp64_oracle.c`, `scripts/port/lp64_oracle.py`,
  `scripts/port/test_lp64_oracle.py`, `scripts/port/test_romconv.sh`,
  `scripts/port/test_romconv_common.c`
- Modify: `CMakeLists.txt`, `scripts/port/lp64_boot.py`,
  `scripts/port/test_lp64_boot.py`, `scripts/port/test_pointer_bits.sh`,
  `scripts/port/abi_probe/probe.c`, `src/game/ramromreplay.h`,
  `src/game/ramromreplay.c`, `.github/workflows/windows-release.yml`,
  `AGENTS.md`, `FORK_CHANGES.md`

**Interfaces:**
- `port_romconv.h` (every converter task adds its prototypes here):
  - `u16 portConvBe16(const u8 *p); u32 portConvBe32(const u8 *p);`
  - `u8 *portConvScratch(u32 bytes);` a grow-only buffer; the returned
    bytes are valid until the next call. `void portConvScratchReserve(u32
    bytes);` is called once from `romdataInit` with 262,144 (the largest
    model file is 43,120 bytes and the largest setup 103,424).
  - `struct PortConvMap`, with `int portConvMapInit(struct PortConvMap *,
    u32 filesize)`, `void portConvMapPut(struct PortConvMap *, u32 old, u32
    new)`, `int portConvMapGet(const struct PortConvMap *, u32 old, u32
    *new)` (0 when `old` was never put) and `void portConvMapFree`. One
    slot for each 4 bytes of the file; an odd `old` is a miss.
  - `void portConvRefuse(enum PortLp64Asset asset, const char *file, const
    char *why, ...);` logs `ge: lp64: cannot convert CLASS "FILE": WHY`.
- `port_romlayout.h`: the file layouts, one section a class, each struct
  named `Rom...` with `u8`, `u16`, `u32` and `s16` fields only and a
  `_Static_assert` on its size beside it. Task 1 adds `RomDemoHeader`
  (228 bytes) and `RomBrief` (48). *Note (built on 2026-11-16):* Task 1 found that the cart's demo header is 0xE8 bytes, the struct's size at 64 bits and with MinGW (i686 Linux's 228 is the exception), and that playback already steps past it by `RAMROM_HEADER_ROMSIZE` (0xE8, `ramromreplay.c`), so nothing was changed in `ramromreplay`, and `RomDemoHeader` is 0xE8 bytes.
- CMake: `port/src/romconv_*.c` leave `PORT_SRC` unless
  `GE_POINTER_BITS EQUAL 64 OR GE_LP64_ORACLE`; `port_lp64_oracle.c` unless
  `GE_LP64_ORACLE`. `option(GE_LP64_ORACLE ...)` is a configure error
  unless `GE_POINTER_BITS` is 32 and `GE_HEADLESS` is on, and defines
  `GE_LP64_ORACLE`. The converter sources get
  `-Werror=incompatible-pointer-types` at both widths.
- `port_lp64_oracle.h`, all `((void) 0)` without `GE_LP64_ORACLE`:
  - `void portLp64OracleBytes(enum PortLp64Asset, const char *file, const
    void *inplace, const void *converted, u32 size);` logs
    `ge: lp64: oracle: CLASS FILE identical (N bytes)` or
    `... DIFFERS at +0xOFF (in-place XX, converter YY)`.
  - `void portLp64OracleResult(enum PortLp64Asset, const char *file, int
    identical, const char *detail);` for a comparison that is not of bytes.
  - At exit: one line a class, `ge: lp64: oracle: CLASS: N files, M differ`.
- `lp64_oracle.py --rom ROM --classes "A,B" EXECUTABLE`: one boot with
  `GE_ROMPROBE=1` and `GE_LP64_ORACLE_SWEEP=1`, then the 20 missions of
  `audit_buffers.py`'s list for 600 frames each; sums the exit lines; exit
  0 only if each named class has N above zero and M zero in total.
- `lp64_boot.py --expect-run [--frames N]` (default 600), exclusive with
  `--expect-stop`: exit 0 only if every run exits with status 0, logs the
  address line, and logs no asset-class stop, no `ge: fatal signal`, no
  sanitizer line and no `ge: lp64: pool` or `ge: lp64: cannot convert`
  line; prints `OK: ran N frames (EXECUTABLE)`. `--expect-stop` gains the
  last two refusals as well.
  *Note (built on 2026-11-16):* by the review of Task 1, `--expect-run` also requires `pcsched.c`'s `ge: frame limit reached: frames=N` with N the expected count, so a run that quits cleanly early fails.
- `ramromreplay.h`: `#define RAMROM_FILE_HEADER_SIZE 228`, used in place of
  `sizeof(struct ramromfilestructure)` at `ramromreplay.c:287,708,761`.
  *Note (built on 2026-11-16):* Task 1 found that the cart's demo header is 0xE8 bytes, the struct's size at 64 bits and with MinGW (i686 Linux's 228 is the exception), and that playback already steps past it by `RAMROM_HEADER_ROMSIZE` (0xE8, `ramromreplay.c`), so nothing was changed in `ramromreplay`, and `RomDemoHeader` is 0xE8 bytes.

- [ ] **Step 1: Write the failing tests**

`test_romconv_common.c` (built by `test_romconv.sh` at both widths through
`harness_widths.sh`; every later converter test is added to this script):
`portConvBe32` of `{0x12,0x34,0x56,0x78}` is `0x12345678`; a map of a
64-byte file returns what was put, misses an offset never put, and misses
an odd offset; `portConvScratch(100)` then `(200000)` return writable
memory; after `portConvScratchReserve(262144)` a request of 262,144 returns
the same address twice. *Note (built on 2026-11-16):* that check passed against a reserve stubbed out (the freed block's address came back), so it also writes bytes through the reserved block and requires them intact. `test_lp64_oracle.py`: the summing on three sample
logs (all identical; one `differ`; a class with no file) and the command
line against stand-in scripts. `test_lp64_boot.py`: `--expect-run` passes a
stand-in that prints the address line and exits 0, and fails one that
stops at a class, one that prints `ge: lp64: pool "music heap" exhausted`
and one that exits 5. `test_pointer_bits.sh`: `-DGE_LP64_ORACLE=ON` with
`-DGE_POINTER_BITS=64`, and without `-DGE_HEADLESS=ON`, are configure
errors; a 32-bit configure without it lists no `romconv_` source in
`compile_commands.json`. `probe.c`: `sizeof(RomDemoHeader) == 228`, and at
32 bits `sizeof(struct ramromfilestructure) == 228`.

Run: `python3 scripts/port/lp64_env.py run --arch x86_64 -- bash scripts/port/test_romconv.sh`
Expected: fails to compile (`port_romconv.h: No such file`).

- [ ] **Step 2: Implement the interface, the CMake rules and the two tools**

Add `bash scripts/port/test_romconv.sh` and `python3
scripts/port/test_lp64_oracle.py` to the CI tests step: the fixtures are
the converters' check in CI, which has no ROM (the design's item 4).

- [ ] **Step 3: Confirm the pointer-free formats**

Read and record in FORK-089, with the line of each reader: briefings
(`romdata.c`'s `CLASS_BRIEF`), fog tables (`portFogServeFromRom`), AI
command lists (no struct of `src/aicommands2.h` has a pointer member; lists
are found by ID). Replace the three `sizeof` in `ramromreplay.c`; at 32
bits the constant equals the `sizeof`, so the Windows comparison is
identical.

- [ ] **Step 4: Run**

Run: `test_romconv.sh` in the x86_64 and aarch64 containers.
Expected: `test_romconv: PASS (32-bit)` and `PASS (64-bit)`; `PASS
(64-bit)` alone on aarch64.
Run: task check steps 1 to 5, step 5 with `--classes ""`.
Expected: step 4 still `OK: stopped at "animation tables"`; step 5 builds,
runs 20 missions and exits 0.

- [ ] **Step 5: Commit**

```bash
git commit -m "Add the converter interface, the oracle build and the boot check's run mode"
```

---

### Task 2: Pools that fail quietly say so at 64 bits

**Files:**
- Modify: `port/include/port_lp64.h`, `port/src/port_lp64.c`,
  `src/libultra/audio/heapalloc.c` (`alHeapDBAlloc`, the one
  `CMakeLists.txt:188` compiles), `src/game/bg.c` (`bgLoadRoomModelData`'s
  two failures, `:3617` and `:3762-3769`), `src/game/dyn.c`
  (`portDynAuditFrameEnd`, `dynInitMemory`), `src/game/image.c` (beside
  `ge: TEXOOM`), `scripts/port/test_lp64_address.c`, `FORK_CHANGES.md`

**Interfaces:**
- `void portLp64PoolExhausted(const char *pool, u32 request);` at 64 bits
  logs `ge: lp64: pool "POOL" exhausted (request N bytes)`, at most 8 lines
  a pool a stage; `void portLp64PoolFatal(const char *pool, u32 request);`
  logs the same and exits with status 6. Both are `((void) 0)` at 32 bits.
  Pool names: `music heap`, `room memory`, `graphics buffer`,
  `texture pool`.
- Not covered: a model slot request that returns `NULL` does so in
  `model.c` (`modelmgrInstantiateModel`, strict tier). It is left, and
  named in the hand-over; a run that shows it puts the call on
  `lp64/conv-strict`.
- Fatal: the music heap (its callers copy into the result unchecked) and
  the graphics buffer when the frame-end position is past its end. The
  others log and go on, as the 32-bit game does.
- At 64 bits `dynInitMemory` multiplies `g_GfxSizesByPlayerCount[...]` by
  `sizeof(Gfx) / 8` after the PC doubling (decision 4).

- [ ] **Step 1: Write the failing test**

In `test_lp64_address.c`, 64-bit half: a child process calling
`portLp64PoolFatal("music heap", 4096)` exits 6 with
`ge: lp64: pool "music heap" exhausted (request 4096 bytes)`; nine calls of
`portLp64PoolExhausted("room memory", 64)` print eight lines.

Run: `python3 scripts/port/lp64_env.py run --arch aarch64 -- bash scripts/port/test_lp64_address.sh`
Expected: FAIL, `implicit declaration of function 'portLp64PoolFatal'`.

- [ ] **Step 2: Implement, and place the five calls**

The frame-end check runs every frame of a 64-bit build: one comparison of
two pointers already in registers, no driver call. Record in FORK-089 that
the render-performance rule's benchmark is owed by LP64-005, the first
milestone with a 64-bit build that draws.

- [ ] **Step 3: Task check; commit**

Expected: step 3 exits 0 (the calls expand to nothing at 32 bits); step 4
still stops at `animation tables`.

```bash
git commit -m "Name the pools that fail quietly on a 64-bit build"
```

---

### Task 3: Animation tables

**Files:**
- Create: `port/src/romconv_anim.c`, `scripts/port/test_romconv_anim.c`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `src/bondtypes.h` (`ModelAnimation`), `src/game/initanitable.c`
  (`alloc_load_expand_ani_table`), `port/src/animdata.c` (two accessors,
  the guard's comment), `port/src/port_lp64.c` (the table),
  `scripts/port/abi_probe/probe.c`, `scripts/port/test_romconv.sh`,
  `FORK_CHANGES.md`

**Interfaces:**
- `RomAnimHeader` (0x14 bytes): `u32 entries_offset; u16 frames; u8 unk06;
  u8 unk07; u32 descriptors_offset; u16 unk0c; u16 frame_bits; u32
  stream_offset;`
- `ModelAnimation` at 64 bits: the same members, declared
  `__attribute__((packed, aligned(4)))`, so `address` is at 0, `unk04` at
  8, `bitDescriptors` at 12, `unk0C` at 20, `unk0E` at 22 and `bitStream`
  at 24. Its 32-bit declaration is unchanged.
- `int portAnimHeadersConvert(u8 *table, u32 tablesize, const u8 *staged,
  u32 stagedsize, const u8 *entries, intptr_t *const *tables, int
  ntables);` For each entry of each 0-terminated table that is not 1: reads
  the native-order `RomAnimHeader` at `staged + offset`, writes the
  run-time header at `table + offset` (`address = entries + entries_offset`
  zero-extended, `bitDescriptors = staged + descriptors_offset`,
  `bitStream = staged + stream_offset`), and replaces the table entry with
  the header's address. Returns 0 and calls `portConvRefuse` if an offset
  or `offset + 0x20` is outside the table, or if two distinct headers are
  less than 0x20 apart; converts nothing in that case. A header listed
  twice is written once.
- `const u8 *portAnimStagedData(void); const u8 *portAnimStagedEntries(void);`
  in `animdata.c` (the staging arrays through their real declarations).
- `initanitable.c`: at 64 bits the two `expand_ani_table_entries` calls
  are one `portAnimHeadersConvert` call; a 0 result stops the game with
  `sysFatalError`. At 32 bits the two calls stay.
- `portAnimDataInit`'s byte-order pass works on the file layout at any
  width and is not changed; its guard stays as the class's.

- [ ] **Step 1: Write the failing test**

`test_romconv_anim.c`, on a 0x100-byte staged image built in the test with
three headers at 0x1C, 0x60 and 0xA4 (two of them at 4 mod 8), a
descriptor table of two 6-byte entries at 0x30 and a stream at 0x40, a
table `{0x1C, 1, 0x60, 0xA4, 0x1C, 0}` and a copy of the image as `table`:

```c
assert(portAnimHeadersConvert(table, 0x100, staged, 0x100, entries, lists, 1) == 1);
a = (ModelAnimation *) (table + 0x1C);
assert((u8 *) list[0] == table + 0x1C && list[1] == 1 && list[4] == list[0]);
assert(a->address == (intptr_t) (entries + 0x2B4C) && a->unk04 == 77 && a->unk0E == 0x260);
assert((u8 *) a->bitDescriptors == staged + 0x30 && a->bitStream == staged + 0x40);
assert(a->bitDescriptors[1].bitCount == 7);            /* read through the pointer */
assert(table[0x1C + 0x20] == staged[0x1C + 0x20]);     /* nothing written past 0x20 */
```

and refusals: a header at 0xF0 (0x20 past it is outside), and headers at
0x1C and 0x30 (0x14 apart), each leaving `table` and the list unchanged.
At 64 bits also `offsetof(ModelAnimation, bitStream) == 24` and
`_Alignof(ModelAnimation) == 4`; at 32 bits `sizeof(ModelAnimation) ==
0x40`. Compile the 64-bit half with `-fsanitize=address,undefined
-fno-sanitize-recover=all`.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portAnimHeadersConvert'`.

- [ ] **Step 2: Implement; mark the class converted**

`port_lp64.c`: the `animation tables` entry becomes 1.

- [ ] **Step 3: Oracle**

In the oracle build, after the two `expand_ani_table_entries` calls: copy
the staged image, run the converter on the copy with a copy of the two
tables, and compare each header's 0x14 bytes and each table entry (less the
copy's address) with the in-place ones through `portLp64OracleBytes`, one
"file" a header.
Expected in step 5: `animation tables: 173 files, 0 differ`.

- [ ] **Step 4: Run the boot check**

Run: task check step 4 with `STOP` = `text banks`, aarch64, then x86_64.
Expected: `OK: stopped at "text banks"` for both executables.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Convert the animation table's headers on a 64-bit build"
```

---

### Task 4: Text banks

**Files:**
- Create: `port/src/romconv_text.c`, `scripts/port/test_romconv_text.c`
- Modify: `port/include/port_romconv.h`, `port/src/romdata.c`
  (`romdataTextPreprocess`), `src/game/language.c` (`langGet`),
  `port/src/port_lp64.c`, `scripts/port/test_romconv.sh`, `FORK_CHANGES.md`

**Interfaces:**
- `struct PortTextPatch { const char *bank; u32 slot; const char *text; };`
- `s32 portTextBankConvert(u8 *data, u32 size, uintptr_t base, const char
  *bank, const struct PortTextPatch *patches, u32 npatches);` Returns the
  slot count, 0 for an empty bank (all zero bytes), -1 after
  `portConvRefuse`. The table's size is the smallest non-zero offset, which
  must be a multiple of 4 and inside the bank, as today. At 64 bits each
  slot becomes the native-order offset (0 stays 0; an offset at or past
  `size` becomes 0 with today's log line) and a patch is copied over the
  string its slot names; a patch longer than that string, or a slot whose
  string another slot also names, refuses the bank. At 32 bits each slot
  becomes `(u32) (base + offset)` and a patched slot the patch's address,
  as today.
- `romdataTextPreprocess` at 64 bits calls it with `base = (uintptr_t)
  data` and keeps its log lines; at 32 bits it is unchanged.
- `langGet` at 64 bits returns `(u8 *) textbank_ptr + textslot_offset` for
  a non-zero slot.

- [ ] **Step 1: Write the failing test**

A bank built in the test: three big-endian slots `{0x0C, 0, 0x12}`, then
`"ratio\n\0"` at 0x0C... written out as bytes, 0x18 long.

```c
assert(portTextBankConvert(bank, 0x18, (uintptr_t) bank, "LtestE", NULL, 0) == 3);
#if IS_64_BIT
assert(slot(bank, 0) == 0x0C && slot(bank, 1) == 0 && slot(bank, 2) == 0x12);
#else
assert(slot(bank, 0) == (u32) (uintptr_t) (bank + 0x0C));
#endif
```

with a patch `{"LtestE", 0, "hud\n"}`: at 64 bits `strcmp((char *) bank +
0x0C, "hud\n") == 0` and slot 2's string is untouched; a patch
`"much longer\n"` returns -1 and leaves the bank's strings as they were; a
bank of 32 zero bytes returns 0; a first offset of 0x0D returns -1.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portTextBankConvert'`.

- [ ] **Step 2: Implement; mark the class converted**

- [ ] **Step 3: Oracle**

In the oracle build `romdataTextPreprocess` converts a copy with `base` set
to the real bank's address and compares the whole bank.
Expected: `text banks: N files, 0 differ`, N the banks of the sweep.

- [ ] **Step 4: Run the boot check**

`STOP` = `audio banks`. The log must still say
`text LoptionsE  64 slots from the ROM (port relabels applied)`.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Keep text bank slots as offsets on a 64-bit build"
```

---

### Task 5: Audio banks

**Files:**
- Create: `port/src/romconv_bank.c`, `scripts/port/test_romconv_bank.c`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `port/src/audio_stubs.c`, `src/music.c` (the two `alBnkfNew` calls),
  `src/snd.h` (`ALInstrumentAlt_s`), `port/src/audio_pc.c` (the comment at
  `:31-33`), `port/src/port_lp64.c`, `scripts/port/abi_probe/probe.c`,
  `scripts/port/test_romconv.sh`, `FORK_CHANGES.md`

**Interfaces:**
- File layouts: `RomBankFile` (4 bytes, then `u32` offsets), `RomBank`
  (12, then offsets), `RomInstrument` (16, then offsets), `RomSound` (16),
  `RomWaveTable` (20).
- `u32 portBankConvertedSize(const u8 *file, u32 size);` the bytes of new
  records, each rounded up to 8; 0 after `portConvRefuse` (wrong revision,
  an offset or a count outside the file).
- `ALBankFile *portBankConvert(u8 *file, u32 size, u8 *table, u8 *out, u32
  outsize);` writes one `ALBankFile`, and one record for each distinct
  `ALBank`, `ALInstrument`, `ALSound` and `ALWaveTable` of the file, into
  `out`; a record reached twice is written once. Envelopes, key maps,
  ADPCM books and loops stay in `file` and are byte-swapped there, once
  each, exactly as `portBnkfPatchSound` does; a wave's `base` is `table +
  offset`; a raw wave's samples are swapped once. Every `flags` byte it
  writes is 1.
- `music.c` at 64 bits: `out = alHeapAlloc(&g_musicHeap, 1,
  portBankConvertedSize(...))`, then `sfxBank = portBankConvert(...)`; the
  same for the instruments. At 32 bits `alBnkfNew` as today.
- `ALInstrumentAlt_s` at 64 bits: `s32 unk0; s32 unk4; ALSound
  *soundArray[1];`.

- [ ] **Step 1: Write the failing test**

A bank file built in the test: one bank (rate 22050) with two instruments;
the first has two sounds that share one wave table and one envelope, the
second one sound with a 16-bit raw wave and a raw loop; one ADPCM book of
order 2 with 1 predictor.

```c
n = portBankConvertedSize(file, sizeof(file));
f = portBankConvert(file, sizeof(file), table, out, n);
b = f->bankArray[0];
assert(f->bankCount == 1 && b->instCount == 2 && b->sampleRate == 22050);
assert(b->instArray[0]->soundCount == 2);
assert(b->instArray[0]->soundArray[0]->wavetable == b->instArray[0]->soundArray[1]->wavetable);
assert(b->instArray[0]->soundArray[0]->wavetable->base == table + 0x40);
assert(b->instArray[0]->soundArray[0]->envelope->attackTime == 5000);
assert(b->instArray[0]->soundArray[0]->wavetable->waveInfo.adpcmWave.book->book[3] == -1234);
assert(b->instArray[1]->soundArray[0]->wavetable->type == AL_RAW16_WAVE);
assert(((s16 *) (table + 0x80))[0] == 0x0102);      /* swapped once */
```

Converting a second copy with `outsize = n - 8` returns `NULL` and writes
nothing past `out + n - 8`; a file whose instrument count runs past its end
gives size 0. `probe.c`, both widths: `offsetof(struct ALInstrumentAlt_s,
soundArray) + sizeof(void *) == offsetof(ALInstrument, soundArray)`.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL (the probe's assertion fails at 64 bits before the `snd.h`
change; the test does not link).

- [ ] **Step 2: Implement; mark the class converted**

- [ ] **Step 3: Oracle**

The two graphs are not the same bytes. `portLp64OracleResult` gets the
result of walking both from the bank file down and comparing every scalar
field and, for each pointer, that both point at the same file offset or at
records that compare equal.
Expected: `audio banks: 2 files, 0 differ`.

- [ ] **Step 4: Run the boot check**

`STOP` = `music sequences`. Record the music heap's use from `GE_SNDDBG=1`
at both widths (32 bits today: 219,120 of 753,664).

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Build the audio banks' run-time records on a 64-bit build"
```

---

### Task 6: Music sequences

**Files:**
- Create: `port/src/romconv_seq.c`, `scripts/port/test_romconv_seq.c`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `src/music.c` (`musicSeqPlayerInit`, `:798-831`), `port/src/port_lp64.c`,
  `scripts/port/test_romconv.sh`, `FORK_CHANGES.md`

**Interfaces:**
- `RomSeqTable`: `u16 count; u16 unk;` then `RomSeqEntry { u32 offset; u16
  uncompressed_len; u16 len; }`.
- `u32 portSeqTableSize(const u8 *file);`
  `offsetof(RareALSeqBankFile, seqArray) + count * sizeof(RareALSeqData)`.
- `int portSeqTableConvert(const u8 *file, u32 filesize, RareALSeqBankFile
  *out, u32 outsize);` writes `seqCount`, and for each entry the offset
  zero-extended in `address` (the game's `musicSeqFileNew` then adds the
  segment's address, unchanged) and the two lengths. Returns 0 after
  `portConvRefuse` when `filesize` or `outsize` is too small.
- `music.c` at 64 bits: the table is allocated at `portSeqTableSize` and
  filled by the converter from the ROM segment; the existing probe copy,
  `tblSegmentSize` arithmetic and swap loop stay in the 32-bit branch.

- [ ] **Step 1: Write the failing test**

A table of three entries built in the test (`offset` 0x1FC, 0x300,
0x80000010; lengths 6109/3948, 10/8, 1/1):

```c
assert(portSeqTableSize(file) == offsetof(RareALSeqBankFile, seqArray) + 3 * sizeof(RareALSeqData));
assert(portSeqTableConvert(file, sizeof(file), out, portSeqTableSize(file)) == 1);
assert(out->seqCount == 3 && (uintptr_t) out->seqArray[0].address == 0x1FC);
assert((uintptr_t) out->seqArray[2].address == 0x80000010u);   /* not sign-extended */
assert(out->seqArray[0].uncompressed_len == 6109 && out->seqArray[0].len == 3948);
```

and a refusal for `outsize` one byte short, with `out` untouched.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portSeqTableSize'`.

- [ ] **Step 2: Implement; mark the class converted**

- [ ] **Step 3: Oracle**

Compare the converter's table, before `musicSeqFileNew`, with the in-place
one at the same point. Expected: `music sequences: 1 files, 0 differ`.

- [ ] **Step 4: Run the boot check**

`STOP` = `textures`: the boot now reaches the title's stage load.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Build the music sequence table at its run-time size on a 64-bit build"
```

---

### Task 7: Audio command words

**Files:**
- Modify: `include/PR/abi.h` (`Awords`, the nine address casts),
  `port/src/audio_pc.c` (`portAcmdValidate`'s `u32 w1`, `:421`),
  `port/include/port_audio_bounds.h` (`:16`), `src/audi.c` (`:550`),
  `scripts/port/abi_probe/probe.c`, `scripts/port/test_audio_buffers.c`,
  `FORK_CHANGES.md`

**Interfaces:**
- `abi.h`: `typedef` `Aword`, `uintptr_t` at 64 bits and `unsigned int`
  otherwise (chosen by `__SIZEOF_POINTER__`, as `gbi.h` chose `gbi_long`:
  `abi.h` is read before anything defines `IS_64_BIT`). `Awords` is
  `{ Aword w0, w1; }`. The casts of an address in `aADPCMdec`,
  `aPoleFilter`, `aEnvMixer`, `aLoadBuffer`, `aPan`, `aResample`,
  `aSaveBuffer`, `aSetLoop` and `aLoadADPCM` become `(Aword)`. `Acmd` is 16
  bytes at 64 bits and 8 at 32. The bit-field views no longer overlay the
  words at 64 bits; nothing compiled on PC reads them (confirm by search,
  and record it).
- The interpreter already takes `uintptr_t w1 = list[c].words.w1`.

- [ ] **Step 1: Write the failing test**

`test_audio_buffers.c`, 64-bit half: build one `aLoadBuffer` command whose
address is a `malloc`ed block's (above 4 GB in a position-independent
test), run it through the interpreter, and assert the bytes loaded are the
block's; assert `portAcmdValidate` accepts that list. `probe.c`:
`sizeof(Acmd) == 2 * sizeof(void *)`.

Run: `bash scripts/port/test_buffers.sh`
Expected: FAIL at 64 bits (the address is cut to 32 bits; the load reads
elsewhere or the validator rejects it).

- [ ] **Step 2: Implement**

- [ ] **Step 3: Task check; commit**

Expected: step 1's video tree compiles (the mixer is only there); step 3
exits 0 (`Aword` is `unsigned int` at 32 bits); step 4 unchanged, since a
headless build emits no audio command. The browser build keeps an 8-byte
`Acmd` (`scripts/port/web_probe/probe.c` is not changed).

```bash
git commit -m "Carry whole buffer addresses in audio commands on a 64-bit build"
```

---

### Task 8: Textures and the display-list expander

**Files:**
- Create: `port/src/romconv_gfx.c`, `scripts/port/test_romconv_gfx.c`
- Modify: `port/include/port_romconv.h`, `src/game/tex.c`
  (`texLoadFromGdl` `:846`, `texCopyGdls` `:1177-1207`),
  `src/game/image.c` (only if Step 3 shows a threshold too small),
  `scripts/port/test_lp64_textures.c`, `port/src/port_lp64.c`,
  `scripts/port/test_romconv.sh`, `FORK_CHANGES.md`

**Interfaces:**
- `void portGfxFromRom(Gfx *dst, const u8 *src, u32 commands);` each
  8-byte big-endian command becomes one `Gfx` with both words
  zero-extended. `dst` and `src` may be the same buffer only at 32 bits;
  at 64 bits it copes with `dst` starting at or before `src` when it is
  told to work backwards: `void portGfxFromRomBackward(Gfx *dst, const u8
  *src, u32 commands);` for a destination that overlaps the source's
  start.
- `void portGfxFromWords(Gfx *dst, const u32 *words, u32 commands);` the
  same for commands already in native 4-byte words.
- `texLoadFromGdl`: `count = srcsize / sizeof(Gfx)`; its input is host
  `Gfx`.
- `texCopyGdls(dst, src, bytes)`: at 64 bits `portGfxFromRom(dst, src,
  bytes >> 3)`; its 32-bit body stays. Its comment gains its second
  caller's fate (`objecthandler_2.c:162` is dead on PC).

- [ ] **Step 1: Write the failing test**

```c
static const u8 rom[] = { 0x04,0x30,0x00,0x40, 0x0E,0x00,0x01,0x20,
                          0xB8,0x00,0x00,0x00, 0x00,0x00,0x00,0x00 };
portGfxFromRom(out, rom, 2);
assert(GFX_OP(&out[0]) == 0x04 && out[0].words.w0 == 0x04300040u);
assert(out[0].words.w1 == 0x0E000120u && out[1].words.w0 == 0xB8000000u);
assert(sizeof(out[0].words.w0) == sizeof(void *));
```

a command `0xFF,0xFF,0xFF,0xFF` gives `w0 == 0xFFFFFFFFu` (not
sign-extended); `portGfxFromRomBackward` on a buffer holding 3 commands at
its end and room for 3 `Gfx` from its start gives the same three as the
forward call on separate buffers. In `test_lp64_textures.c`:
`portTexReplaceRom` with a replacement registered for a texture number
stores the replacement's whole address in the `struct tex` and leaves a
texture without one alone, at both widths.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portGfxFromRom'`.

- [ ] **Step 2: Implement the expander and the two `tex.c` changes**

- [ ] **Step 3: Read `texLoad` at 64 bits, then mark the class converted**

`image.c:2528-2791`, `texInflateZlib`, `texInflateNonZlib`, the pool
arithmetic and the thresholds at `:2632`. The planning read found no
address held in 32 bits and no literal record size; record what this read
finds, line by line, in FORK-089. Nothing is changed that the read does
not show wrong.

- [ ] **Step 4: Run the boot check**

`STOP` = `fonts`, plain and sanitizer. The Windows comparison must be
identical (`srcsize / sizeof(Gfx)` is `>> 3` at 32 bits).

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Expand ROM display lists to host commands and count them by size"
```

---

### Task 9: Fonts

**Files:**
- Create: `port/src/romconv_font.c`, `scripts/port/test_romconv_font.c`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `port/src/segments.c` (the three staging arrays' sizes and their
  `.space` labels, `portFontBuildSegments`), `port/src/port_lp64.c`,
  `scripts/port/abi_probe/probe.c`, `scripts/port/test_romconv.sh`,
  `FORK_CHANGES.md`

**Interfaces:**
- `RomFont`: `u32 kerning[169]; RomFontChar chars[94];` with `RomFontChar`
  six `u32` (`index`, `baseline`, `height`, `width`, `kerningindex`,
  `pixel_offset`), 0xB74 bytes; glyph bytes follow.
- `u32 portFontConvertedSize(u32 romsize);` `romsize - sizeof(RomFont) +
  sizeof(struct font)`.
- `int portFontConvert(const u8 *rom, u32 romsize, u8 *out, u32 outsize);`
  writes a `struct font` with native fields, each `pixeldata` holding
  `pixel_offset - sizeof(RomFont) + sizeof(struct font)` zero-extended (0
  stays 0), then the glyph bytes. The game's own fix-up in
  `load_font_tables` then adds the copy's address, unchanged. Refuses a
  `pixel_offset` outside the glyph area.
- `segments.c` at 64 bits: the two font staging arrays and their `End`
  labels are `0x24B0 + 0x2F4` and `0x3540 + 0x2F4` bytes, and the font
  list's 0x180; `portFontBuildSegments` fills them with `portFontConvert`
  and `portGfxFromRom` (24 commands). `load_font_tables` and `lvInit` are
  not edited: their sizes come from the labels.

- [ ] **Step 1: Write the failing test**

A font built in the test with glyph 0 (`width` 5, `height` 7,
`kerningindex` 3, `pixel_offset` 0xB80, eight glyph bytes there) and glyph
93 with `pixel_offset` 0:

```c
n = portFontConvertedSize(sizeof(rom));
assert(n == sizeof(rom) - 0xB74 + sizeof(struct font));
assert(portFontConvert(rom, sizeof(rom), out, n) == 1);
f = (struct font *) out;
assert(f->kerning[168] == 0x55 && f->chars[0].width == 5 && f->chars[0].kerningindex == 3);
assert((uintptr_t) f->chars[0].pixeldata == 0xB80 - 0xB74 + sizeof(struct font));
assert(out[(uintptr_t) f->chars[0].pixeldata] == 0xA1);     /* the glyph's first byte */
assert(f->chars[93].pixeldata == NULL);
```

and a refusal for `pixel_offset` 0xFFFFFF. `probe.c`:
`sizeof(struct font) - sizeof(RomFont)` is 0 at 32 bits and 0x2F4 at 64.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portFontConvert'`.

- [ ] **Step 2: Implement; mark the class converted**

- [ ] **Step 3: Oracle**

Compare each converted staging image with the one `portFontFromRom`
builds. Expected: `fonts: 3 files, 0 differ` (two fonts and the list).

- [ ] **Step 4: Run the boot check**

`STOP` = `models`, plain and sanitizer, aarch64 and x86_64. This is as far
as the planning stand-ins went.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Build the font tables at their run-time layout on a 64-bit build"
```

---

### Task 10: Models, the converter

**Files:**
- Create: `port/src/romconv_model.c`, `port/src/romconv_model_records.c`,
  `scripts/port/test_romconv_model.c`,
  `scripts/port/test_romconv_model_build.h` (the fixture builder)
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `scripts/port/abi_probe/probe.c`, `scripts/port/test_romconv.sh`,
  `FORK_CHANGES.md`

**Interfaces:**
- File layouts: `RomModelNode` (24), `RomModelTexture` (12), and one
  struct for each record kind with its size from `modelRecordSize`
  (`romfile_model.c:326`): Header 16, Group 28, DL 20, DLCollision 32,
  DLPrimary 16, LOD 16, BSP 36, BBox 28, Gunfire 40, Shadow 32, Switch 8,
  Op06 24, GroupSimple 20, Head 4, Interlink 28, Op11 76, Op05 424, Op07
  432.
- `u32 portModelConvertedSize(const u8 *file, u32 size, s32 numSwitches,
  s32 numtextures, const char *name);` walks the file as
  `romfile_model.c`'s `walk` does (the same reachability: `Next`, `Child`,
  LOD's `Affects`, Switch's `Controls`, BSP's two children) and returns
  the output's size, or 0 after `portConvRefuse` for anything `walk`
  counts as bad today.
- `s32 portModelConvert(const u8 *file, u32 size, u8 *out, u32 outsize,
  s32 numSwitches, s32 numtextures, const char *name);` returns 1, or 0
  after `portConvRefuse`. `file` and `out` must not overlap. Output, in
  this order: the switch slots (`sizeof(ModelNode *)` each), the texture
  entries (`sizeof(ModelFileTextures)` each), the nodes, the records, the
  vertex, point-usage and inline-texel blocks, the display lists as host
  `Gfx`; every block 8-aligned.
- What it writes:
  - Every reference to a node, a record, a texture entry, a list or a data
    block is `0x05000000 | new offset`, zero-extended into the field's own
    width, found through `portConvMapGet`; a miss refuses the file. A zero
    reference stays zero. `BaseAddr` fields are written as the file has
    them (the game's promotion sets them).
  - Scalars are byte-swapped into the C field of the same meaning, by the
    field maps `romfile_model.c` has (`:47-65`).
  - Vertices keep 16 bytes, six half-words swapped. A collision vertex
    with a link: at 64 bits `LinkOffset` is the target node's new offset
    and `CollisionRelatedIndex` is byte-swapped; a target at new offset 0
    refuses the file. At 32 bits the two are left as the in-place path
    leaves them.
  - Lists through `portGfxFromRom`, then: a `G_VTX` (0x04) word with
    segment 5 becomes the vertex block's new offset plus its distance into
    the block; a `G_SETTIMG` (0xFD) word with segment 5 the texel block's
    new offset; a list call (0x06, 0xDE) with segment 5 the called list's
    new offset. Words with segment 3 or 4, and `G_SETTEX` (0xC0), are
    copied.
  - `Gunfire.Image` and `Shadow.image` are the new offset of the texture
    entry they name.

- [ ] **Step 1: Write the fixture builder and the failing test**

`test_romconv_model_build.h` builds a big-endian file from C calls
(`mfNode`, `mfRecord`, `mfVertices`, `mfList`, `mfTexture`), returning each
item's offset, so the test names things by what they are. The main fixture:
two switch slots, three textures (one inline), a Header whose `FirstGroup`
is a Group; under it a DL node (4 vertices; a list of `G_VTX` segment 5 at
vertex 2, `G_SETTEX`, `G_SETTIMG` to the inline texels, end), a
DLCollision node (2 vertices, 2 collision vertices, the second linked to
that node; point usage), a DLPrimary, an LOD whose `Affects` is a
Switch, a BSP with two BBox children, a Gunfire and a Shadow naming
texture 1, and a GroupSimple, Head and Interlink.

```c
n = portModelConvertedSize(file, size, 2, 3, "Ptest");
assert(n % 8 == 0 && portModelConvert(file, size, out, n, 2, 3, "Ptest") == 1);
sw = (uintptr_t *) out;
tex = (ModelFileTextures *) (out + 2 * sizeof(void *));
root = (ModelNode *) (tex + 3);
assert(sw[0] == (0x05000000u | off_of(out, node_dl)) && sw[1] == 0);
assert(tex[1].TextureID == 0x123 && tex[1].Width == 32 && tex[1].Height == 64);
assert((tex[2].TextureID >> 24) == 0x05);
assert((root->Opcode & 0xff) == MODELNODE_OPCODE_HEADER);
dl = record_of(out, node_dl);                    /* follows the 0x05 word */
assert(dl->numVertices == 4);
cmd = list_of(out, dl->Primary);
assert(GFX_OP(&cmd[0]) == 0x04);
assert(cmd[0].words.w1 == (0x05000000u | (off_of(out, vertices_of(out, dl)) + 2 * 16)));
assert(GFX_OP(&cmd[3]) == 0xB8 && cmd[3].words.w1 == 0);
```

and, for each record kind in the fixture, one assertion on a scalar and one
on each reference; `gunfire->Image` and `shadow->image` equal `0x05000000 |
(2 * sizeof(void *) + 1 * sizeof(ModelFileTextures))`. At 64 bits the
linked collision vertex has `LinkOffset == off_of(out, node_dlcollision)`
and index 0x0102; at 32 bits its bytes equal the in-place path's. A second
file with one node of each opcode the ROM lacks (3, 5, 6, 7, 11, 17, 20),
each record filled with a counting pattern, converts with every scalar in
the field the pattern predicts. A third with a list that calls another:
the call's word is the called list's new offset. Refusals, each leaving
`out` unwritten: a node offset below the root; a record that runs past
the file; a reference to an offset nothing owns; `outsize` 8 short; a
collision link to offset 0. `probe.c`: Group, GroupSimple, Interlink and
BBox keep their first float at offset 0 at both widths.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portModelConvertedSize'`.

- [ ] **Step 2: Implement the two passes**

`romconv_model.c` holds the walk, the map and the lists;
`romconv_model_records.c` one writer a record kind. The existing
`test_model_bounds.c` inputs (truncated records, count wraparound, 10,000
random files) run through the new functions too: no crash, no write
outside `out`, under `-fsanitize=address,undefined`.

- [ ] **Step 3: Run**

Run: `test_romconv.sh` in the x86_64 and aarch64 containers; task check
steps 1 to 5. The class is not marked converted in this task.
Expected: `PASS` at both widths; step 4 still stops at `models`.

- [ ] **Step 4: Commit**

```bash
git commit -m "Add the model file converter"
```

---

### Task 11: Models, wired into the loader

**Files:**
- Modify: `src/game/objecthandler_2.c` (`load_object_fill_header`
  `:294-360`, `portConvertModelGdl` `:37-67`), `src/game/ob.c` (a
  capacity accessor), `src/game/gun.c` (the three caps at `:962-972` and
  `D_80032464`, 64-bit values), `port/src/romfile_model.c` (the oracle
  call), `port/src/port_lp64_oracle.c` (the model sweep),
  `port/src/port_lp64.c`, `FORK_CHANGES.md`

**Interfaces:**
- `u32 fileGetCapacity(s32 index);` (`ob.c`): the bytes of the block a
  bank load holds before it is trimmed.
- `load_object_fill_header` at 64 bits, for a served file: `size =
  portModelConvertedSize(filedata, poolsz, ...)`; refuse if `size` is 0 or
  exceeds the room (`fileGetCapacity` for a bank load, the caller's `size`
  for an address load), taking the existing "rendering nothing" branch
  with its log line; copy the file to `portConvScratch(poolsz)`;
  `portModelConvert(scratch, poolsz, filedata, size, ...)`; commit `size`
  with `fileSetSize`. `Switches`, `Textures` and `RootNode` are placed by
  typed pointer arithmetic (`(struct ModelNode **) filedata +
  numSwitches`), which is the same address as today at 32 bits. The 32-bit
  branch keeps `romfileModelConvert`.
- `gun.c`: at 64 bits each fixed model size is the 32-bit one times 13/8,
  rounded up to 16, with the hand buffer and the texture pool that starts
  after it moved to match. A file that still does not fit is refused by
  name (decision 5).
- The oracle sweep converts every model file a compiled header table
  names (bodies, heads, guns and props; the counts come from the header),
  by both paths.

- [ ] **Step 1: Wire the converter; mark the class converted**

No new unit test: the converter's are Task 10's. The checks are the runs
below.

- [ ] **Step 2: Oracle**

Run: task check step 5 with models in `--classes`.
Expected: `models: N files, 0 differ`, with N the number of model files
the header tables name (512 in the stock ROM by the planning scan; record
the number found). Each comparison is of the whole converted image against
the in-place one, both before the game's promotion.

- [ ] **Step 3: Run the boot checks**

Run: `lp64_boot.py --expect-run --frames 600` (the title, the legal screen
and the menus), plain and sanitizer, aarch64 then x86_64.
Expected: `OK: ran 600 frames` for each.
Run: `lp64_boot.py --stage dam --expect-stop "backgrounds"`.
Expected: `OK: stopped at "backgrounds"`.

A fault in `model.c`, `gunfire.c` or `bondview2.c` here is a strict-tier
defect: write a ROM-free test that reproduces it where one can be written
(`test_wardrobe.c` builds model trees in memory), fix it on
`lp64/conv-strict`, and follow "The strict batch".

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Convert model files at load on a 64-bit build"
```

---

### Task 12: Backgrounds

**Files:**
- Create: `port/src/romconv_bg.c`, `scripts/port/test_romconv_bg.c`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `src/game/bg.c` (`load_bg_file` `:1100-1108`, `:1149`, `:1161`; the
  literal strides at `:383`, `:4463`, `:4667`, `:6402`, `:6435`;
  `bgLoadRoomPrimaryGdl` `:3099-3175` and the secondary `:3189-3240`),
  `port/src/romfile_bg.c` (the oracle call), `port/src/port_lp64.c`,
  `scripts/port/test_romconv.sh`, `FORK_CHANGES.md`

**Interfaces:**
- `RomBgHeader` (five `u32`), `RomBgRoom` (three `u32` offsets and three
  big-endian floats, 24 bytes), `RomBgPortal` (`u32 offset; u8 room1,
  room2, cb1, cb2;`, 8 bytes).
- `u32 portBgTablesSize(const u8 *file, u32 size, const char *name);` the
  bytes of both run-time tables, 8-aligned; 0 after `portConvRefuse`.
- `int portBgTablesConvert(const u8 *file, u32 size, u8 *out, u32 outsize,
  bg_room_data **rooms, bg_portal_data_entry **portals);` writes
  `bg_room_data` records up to and including the zero record after the end
  marker, and `bg_portal_data_entry` records up to and including the zero
  one. Offsets are kept as the file's `0x0F` words, zero-extended, so
  `BG_SEG_TO_PTR`, the file-position arithmetic (`:3033`, `:3128`,
  `:3218`) and the portal promotion (`:1195-1198`) run unchanged. It reads
  the file after `romfileBgConvert`'s byte-order pass, which stays (it is
  width-independent and idempotent by its registry).
- `load_bg_file` at 64 bits: the two tables are allocated from
  `MEMPOOL_STAGE` at each load and `ptr_bgdata_room_fileposition_list` and
  `g_BgPortals` point at them. The vis commands and the portal points stay
  in the ROM image.
- The literal strides become `sizeof(bg_room_data)`, `offsetof(bg_room_data,
  pos)` and field access to `controlbytes1`; identical code at 32 bits.
- Room lists at 64 bits: after `bgDecompress` returns E bytes of 8-byte
  commands at the block's end, the list is expanded with
  `portGfxFromRomBackward` where `texCopyGdls` is called today, needs 2E
  bytes of the block, and reports `room memory` exhausted (Task 2) when
  they are not there; `texLoadFromGdl` is given host bytes, and
  `usize_*_DL_binary` holds host bytes.

- [ ] **Step 1: Write the failing test**

A front matter built in the test: header `{0, 0x0F000014, 0x0F00008C,
0x0F00007C, 0}`, four room records (a zero one, two rooms, the end marker)
and the closing zero, two vis commands, two portals and a zero one, the
portal points.

```c
n = portBgTablesSize(fm, sizeof(fm), "test");
assert(portBgTablesConvert(fm, sizeof(fm), out, n, &rooms, &portals) == 1);
assert((uintptr_t) rooms[1].pPointTableBin == 0x0F000200u && rooms[1].pos.y == 12.5f);
assert((uintptr_t) rooms[3].pPointTableBin == 0x0F000900u);        /* the end marker */
assert(portals[0].connectedRoom1 == 3 && portals[0].connectedRoom2 == 4 && portals[1].controlbytes1 == 0x80);
assert((uintptr_t) portals[0].offset_portal == 0x0F0000A4u);
assert(BG_SEG_IS_OFFSET((uintptr_t) portals[0].offset_portal));
```

(with the field names of `bg.h:143-158`), and refusals: a room table that
runs into the vis commands without a zero record; `outsize` 8 short.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portBgTablesSize'`.

- [ ] **Step 2: Implement; change the strides; mark the class converted**

- [ ] **Step 3: Measure what a second load sees at 32 bits**

In the oracle build: at each `load_bg_file`, compare the two tables in the
ROM image with a fresh conversion of a pristine copy of the file, field by
field, before the load's own passes run. Load Dam, play 600 frames, load
it again (`GE_STAGE=dam` then the soak's second lap). Record every field
that differs at the second load. Expected: unknown; this is the
measurement decision 6 asks for. A difference is recorded for LP64-006 and
changes nothing here.

- [ ] **Step 4: Oracle and boot check**

Oracle: the converted tables against the in-place ones, at the first load
of each bg file, and in the sweep for every bg file `levelinfotable` names
that the ROM serves. Expected: `backgrounds: N files, 0 differ` (25 files
have data in the stock ROM; the smoke alone loads 20).
Boot: `--stage dam --expect-stop "collision tiles"`.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Build the background's room and portal tables and expand room lists on a 64-bit build"
```

---

### Task 13: Collision tiles

**Files:**
- Create: `port/src/romconv_stan.c`, `scripts/port/test_romconv_stan.c`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `port/src/romfile_stan.c`, `src/game/bg.c` (the trace at `:1122-1124`
  prints the pointer; a `NULL` stan is refused by name before
  `stanDetermineEOF`), `scripts/port/test_stan_callbacks.c` (the crouch
  branch), `port/src/port_lp64.c`, `scripts/port/test_romconv.sh`,
  `FORK_CHANGES.md`

**Interfaces:**
- `RomStanHeader`: `u32 unk; u32 first_tile;` then `u32` room offsets to a
  zero.
- `int portStanConvert(u8 *data, u32 size, uintptr_t base, const char
  *name);` checks `first_tile == 8 + (rooms + 1) * 4` and `rooms >= 1`,
  converts every tile in place exactly as `romfileStanConvert` does
  (`:90-115`), then writes `struct StanPrefixRecord { NULL, base +
  first_tile }` over the file's first `sizeof` bytes. At 64 bits that
  covers the two header words and the first two room slots, which nothing
  on PC reads outside `DEBUG`; at 32 bits it is the two words, and the
  room slots are rebased as today. Refuses a file with no room slot.
- `romfileStanConvert` at 64 bits calls it; `portStanPack` keeps its guard
  (no compiled-in stan is linked at 64 bits).

- [ ] **Step 1: Write the failing tests**

A stan built in the test: header, three room slots and the zero, two tiles
of three points with one edge link.

```c
assert(portStanConvert(stan, sizeof(stan), (uintptr_t) stan, "test") == 1);
p = (struct StanPrefixRecord *) stan;
assert(p->stanfile == NULL && (u8 *) p->ptr_firstroom == stan + 0x18);
assert(p->ptr_firstroom->room == 5 && p->ptr_firstroom->points[1].link == 0x0033);
```

and a refusal for `first_tile` 0x14 with three room slots. In
`test_stan_callbacks.c`: the crouch tile (special 1) sets the locus
record's first field and `stanGetLocusField0` returns non-zero, at both
widths (the hand-over list's untested branch).

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portStanConvert'`.

- [ ] **Step 2: Implement; mark the class converted**

- [ ] **Step 3: Oracle and boot check**

Oracle: the whole file. Expected: `collision tiles: N files, 0 differ`.
Boot: `--stage dam --expect-stop "setup files"`. The rooms'
first loads (`bgRoomCalcBB`) now run for the first time: any `ge: lp64:
pool "room memory"` line fails the check and is Task 12's defect.

- [ ] **Step 4: Task check; commit**

```bash
git commit -m "Write the collision file's header as pointers on a 64-bit build"
```

---

### Task 14: Setup files, the tables and the intro

**Files:**
- Create: `port/src/romconv_setup.c`, `scripts/port/test_romconv_setup.c`,
  `scripts/port/test_romconv_setup_build.h`
- Modify: `port/include/port_romconv.h`, `port/include/port_romlayout.h`,
  `src/bondtypes.h` (the credits record's table field becomes
  pointer-sized at 64 bits), `src/game/bondview_r.c` (`:380`),
  `scripts/port/abi_probe/probe.c`, `scripts/port/test_romconv.sh`,
  `FORK_CHANGES.md`

**Interfaces:**
- File layouts: `RomSetupHeader` (ten `u32`), `RomPad` (44), `RomBoundPad`
  (68), `RomWaypoint` (16), `RomWaygroup` (12), `RomPath` (8), `RomAiList`
  (8), and the intro records by their word counts (`kSetupIntroKinds`).
- `u32 portSetupConvertedSize(const u8 *file, u32 size, const char
  *name);` and `s32 portSetupConvert(const u8 *file, u32 size, u8 *out,
  u32 outsize, const char *name);` (`file` and `out` not overlapping).
  This task implements everything but the prop records, which it copies
  through `portSetupPropsSize` and `portSetupPropsConvert`, stubs here
  that refuse a file with any prop record but the end marker.
- Output: a `struct stagesetup` whose ten fields hold offsets from the
  output's start, zero-extended (`SETUP_PROMOTE` adds the base,
  unchanged); then each table at its run-time record size with its
  sub-lists; the AI byte code and the name strings copied; the intro
  records, the camera at 56 bytes with its two text IDs in the low words
  of their unions; the credits table, the credits record pointing at it by
  an offset the 64-bit reader adds the base to. Every reference inside
  (`plink`, the waypoint and waygroup lists, a path's list, an AI list's
  code, a name) is remapped through `portConvMapGet`; a miss refuses the
  file.

- [ ] **Step 1: Write the fixture builder and the failing test**

`test_romconv_setup_build.h` builds a big-endian setup from C calls. The
fixture: two pads and the terminator, one bound pad, three waypoints with
neighbour lists, one waygroup, one path, two AI lists with byte code, two
pad names, an intro of a spawn, a camera (text IDs 0x1234 and 0x5678) and
credits with two entries, and only the end marker for props.

```c
n = portSetupConvertedSize(file, size, "UsetuptestZ");
assert(portSetupConvert(file, size, out, n, "UsetuptestZ") == 1);
s = (struct stagesetup *) out;
pads = (PadRecord *) (out + (uintptr_t) s->pads);
assert(pads[1].pos.z == -40.0f && (uintptr_t) pads[1].plink != 0 && pads[2].plink == NULL);
assert(strcmp((char *) out + (uintptr_t) pads[1].plink, "p1") == 0);
ai = (AIListRecord *) (out + (uintptr_t) s->ailists);
assert(ai[1].ID == 0x0401 && out[(uintptr_t) ai[1].ailist] == 0xAD);   /* first code byte */
```

and one assertion for each table and each reference kind; the camera's
`lang1c` and `lang20` hold their IDs; the walk over the intro by `sizeof`
(as `bondview_r.c:343` does) reaches the end marker. Refusals: a header
offset outside the file; a pad table with no terminator; `outsize` 8
short. `probe.c` gains the 64-bit sizes this plan measured (`stagesetup`
80, `PadRecord` 56, `SetupIntroCamera` 56) beside the 32-bit ones.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portSetupConvertedSize'`.

- [ ] **Step 2: Implement**

- [ ] **Step 3: Run; commit**

The class is not marked converted in this task; task check step 4 still
stops at `setup files`.

```bash
git commit -m "Add the setup file converter for the tables and the intro"
```

---

### Task 15: Setup files, the prop records and the wiring

**Files:**
- Create: `port/src/romconv_setup_props.c`,
  `scripts/port/test_romconv_setup_props.c`
- Modify: `port/include/port_romconv.h`, `port/src/romfile_setup.c` (the
  oracle call), `port/src/romdata.c` (`romdataFilePreprocess`'s setup
  case), `src/game/ob.c` (`fileIndexLoadToBank` `:339-348`,
  `fileIndexLoadToAddr` `:441`), `src/game/loadobjectmodel.c`
  (`sizepropdef`), `src/game/prop.c` (the fix-up loop `:1992-2059`),
  `src/game/initobjects.c` (`:191`), `port/src/port_lp64.c`,
  `scripts/port/test_romconv.sh`, `FORK_CHANGES.md`

**Interfaces:**
- `struct PortSetupWide { u16 filewords; u16 widewords; u8 npointers; u8
  pointerword[16]; };` and `const struct PortSetupWide
  *portSetupWide(u32 type);` one entry for each of the 49 prop types: the
  file's size in words (`kSetupPropdefKinds`), the converted size in
  4-byte words (a multiple of 2), and the file word indexes that are
  pointer-sized in the C record.
- `u32 portSetupPropsSize(const u8 *props, u32 size);` and `s32
  portSetupPropsConvert(const u8 *props, u32 size, u8 *out, u32
  outsize);` each record is byte-swapped by its kinds, then written word
  by word: a pointer word is 8-aligned and zero-extended to 8 bytes, any
  other is copied; the record is padded to `widewords`.
- `sizepropdef` at 64 bits returns `portSetupWide(type)->widewords`.
- The fix-up loop at 64 bits reads and writes the same fields by member
  (`GuardRecord`, `ObjectRecord.damage`, the objective and watch records);
  its 32-bit body of word indexes stays.
- `initSetLevelLoadPropSafeItem` writes `((SafeObjectRecord *)
  arg0)->next` at 64 bits.
- `u32 romdataFileConvert(s32 index, u8 *data, u32 size, u32 capacity);`
  (`romdata.c`, 64-bit only): for a setup, measures, refuses when the
  result exceeds `capacity`, copies `data` to the scratch, converts into
  `data` and returns the new size; for every other class calls
  `romdataFilePreprocess` and returns `size`. `ob.c` at 64 bits calls it
  with the block's capacity and commits what it returns.

- [ ] **Step 1: Write the failing test**

First the table against the compiler, for every type that has a C record
(`ObjectRecord` and each record that begins with one, `GuardRecord`,
`LinkRecord`, `LockDoorRecord`, `SafeObjectRecord`, the tag, rename,
vehicle and aircraft records and the rest named in the planning table):

```c
#define CHECK_WIDE(type, T) do { \
    const struct PortSetupWide *w = portSetupWide(type); \
    assert(layout_from_pointer_words(w) == (size_t) w->widewords * 4); \
    assert((size_t) w->widewords * 4 == ((sizeof(T) + 7) & ~(size_t) 7)); \
} while (0)
CHECK_WIDE(PROPDEF_PROP, ObjectRecord);   /* 144 at 64 bits, 128 at 32 */
```

with `layout_from_pointer_words` the rule the converter applies (a pointer
word 8-aligned and 8 bytes, any other 4, the record padded to 8), and for
each pointer word an assertion that the offset the rule gives it is the
`offsetof` of the C member it becomes (`prop` at 16 and `model` at 24 of
`ObjectRecord` at 64 bits). The second assertion of the macro is left out,
by name and with the reason beside it, for the types whose C record is not
the file's record (the CCTV record's struct is four words longer than its
file record; the tinted-glass and link types use another record's size
today): for those the first assertion and the member offsets are the check.
At 32 bits `widewords == filewords` for every type. Then a prop list built in
the test (a guard, a door, a collectable, a link, a tag, a safe item, a
rename, the end marker): after conversion the walk by
`portSetupWide(...)->widewords` visits the same eight types in order,
`((ObjectRecord *) door)->prop == NULL`, `damage` holds the file's value, the
link's two unions hold their indexes, and the door's `linkedDoorOffset` its
index.

Run: `bash scripts/port/test_romconv.sh`
Expected: FAIL, `undefined reference to 'portSetupWide'`.

- [ ] **Step 2: Implement; wire the loader; mark the class converted**

- [ ] **Step 3: Oracle**

Expected: `setup files: N files, 0 differ` (34 retail setups in the
sweep). At 32 bits the converted image equals the in-place one byte for
byte.

- [ ] **Step 4: Run the boot check and the first mission**

Run: `lp64_boot.py --stage dam --expect-run --frames 600`, plain and
sanitizer, aarch64.
Expected: `OK: ran 600 frames`. This is the first 64-bit stage load. What
it finds is fixed here if it is in the setup path, and otherwise carried
to Task 16 with the log.

- [ ] **Step 5: Task check; commit**

```bash
git commit -m "Convert setup files at load on a 64-bit build"
```

---

### Task 16: The 20 missions at 64 bits

**Files:**
- Create: `scripts/port/lp64_census.py`, `scripts/port/test_lp64_census.py`
- Modify: whatever the runs show, each defect in the file that has it;
  `port/src/romconv_*.c` log lines (so both widths report the same counts);
  `.github/workflows/windows-release.yml` (the census test);
  `FORK_CHANGES.md`

**Interfaces:**
- `lp64_census.py compare DIR32 DIR64`: for each mission log of the two
  smoke folders, the multiset of `ge: romdata:` lines naming a model, a
  setup, a stan, a bg or a text bank, with the counts they print; exit 0
  only if every mission's sets are equal; prints the first differing line
  of each mission otherwise. `lp64_census.py types DIR` lists the prop
  record types and model opcodes the missions loaded (from lines the
  converters print when `GE_LP64_CENSUS=1`).
- Each 64-bit converter prints the line its in-place counterpart prints,
  with the same numbers (`model NAME converted (N nodes, V verts, D DLs)`,
  `setup NAME converted (...)`): counts of the file's records, not of
  converted bytes.

- [ ] **Step 1: Write the census tool and its test** (sample logs written
  in the test; equal sets pass, a missing model fails, a count that
  differs fails).

- [ ] **Step 2: Run the smoke and fix what stops a mission**

Run: task check step 6 (plain, aarch64).
For each mission that does not `PASS`: find the first fault, reproduce it
in a ROM-free test where one can be written, fix it at the declaration or
in the converter, and run again. A fix in a strict-tier file goes to
`lp64/conv-strict`. A fault that needs a design change (a pool that must
grow beyond decision 4, a structure this plan did not foresee) stops the
task and goes to the owner with the log.
Expected at the end: 20 of 20 `PASS`, with no `ge: lp64:` line in any log.

- [ ] **Step 3: The same under the sanitizers, and on x86_64**

Run: step 6 with `build/lp64/boot-san/IronSights`; then step 6 with a
plain build in the x86_64 container.
Expected: 20 of 20 `PASS` each (decision 3 says what a report inherited
from the 32-bit game means).

- [ ] **Step 4: Census**

Run: task check step 7.
Expected: exit 0. Then `lp64_census.py types build/lp64/boot`, recorded in
FORK-089: the record types and opcodes no mission loaded are the ones this
milestone leaves verified by fixtures only.

- [ ] **Step 5: Task check; commit** (one commit a defect, each with its
  note in FORK-089; then)

```bash
git commit -m "Run the 20 missions on a 64-bit build and compare what they load"
```

---

### Task 17: Land the strict batch and close

- [ ] **Step 1: If `lp64/conv-strict` has commits, hand it to the laptop**

On the Windows laptop, as in LP64-003's Task 12:

```bash
git fetch origin && git switch lp64/conv-strict
git rebase origin/main
python scripts/port/hand_gallery.py generate
python scripts/port/hand_gallery.py verify
git add review/hand-gallery
git commit -m "Regenerate the review gallery for the strict-tier converter fixes"
git switch main && git pull --ff-only
git merge --ff-only lp64/conv-strict && git push origin main
```

The hand-off names every function of the Windows executable that differs
from the last regeneration (9abbe58), by file and kind of difference. If
the branch is empty, the laptop still regenerates once to clear what the
milestone left pending.

- [ ] **Step 2: Confirm on the Mac**

`hand_gallery.py verify` reports fresh; CI passes on the landed commit; the
whole task check runs once more in both containers, steps 4 and 6 as
`--expect-run` and 20 of 20.

- [ ] **Step 3: Settle the cast allowlist**

`scripts/port/lp64_casts_allowed.txt` has 23 truncating casts in 10 files,
most of them in the in-place loaders and each naming LP64-004 as the
milestone that removes it. For each: if its function no longer runs at 64
bits, compile it at 32 bits only (one `#if !IS_64_BIT` around the function,
or the file out of the 64-bit source list when nothing else in it is
used) and delete its line; otherwise give the line its new reason. Record
the count before and after.

- [ ] **Step 4: Close the record**

Set LP64-004 to Done in the status table with the runs. `AGENTS.md`: the
paragraph on the status-5 stop says that all ten classes are converted and
what the guard now stops (a file the ROM lacks; a 64-bit build with a
class's table entry set back to 0), and names `lp64_oracle.py` and the
`romconv_` rule. FORK-089 gets the hand-over to LP64-005: the record types
and opcodes only fixtures cover; the 64-bit code that has still never run
(the renderer's walk, `GE_LP64_DL_CHECK`, the mixer and `Acmd`, the arm
mesh's alias cases); the pool figures of the smoke; the bg tables'
second-load measurement; the late-load audit owed; the cut stages.

## What this plan does not do

- It does not run the video executable, the mixer or the renderer at 64
  bits. `Acmd` and the expanded lists are checked by ROM-free tests and by
  the game code that builds and walks lists in a headless build.
- It does not replay a demo, load a multiplayer stage or a cut stage, or
  play beyond 600 frames of a mission's start. LP64-005 does.
- It does not claim the 64-bit game behaves like the 32-bit one. The
  census compares what was loaded, not what happened next; simulation,
  picture and sound equivalence are LP64-006's.
- It does not size pools beyond the graphics buffer, and does not change a
  32-bit budget.
- It does not remove the 32-bit in-place loaders, or make the 32-bit build
  use a converter outside the oracle build.
- It does not remove the dead copy of each model's display lists (the
  converted file keeps its lists, which `portConvertModelGdl` then copies
  to high memory, as at 32 bits). That is 8 bytes a command, 464 KB over
  every model of the ROM, and a later saving if it matters.
- It does not convert the compiled-in asset copies. They are a 32-bit-only
  fallback by the design; their guards stay.

## Decisions for the owner

**The owner accepted all ten recommendations on 2026-11-16.** They are kept
here as put, each with the alternative that was not taken; the plan is
written for the recommendations. Decision 3 defines the milestone's done
condition, decisions 1 and 2 depart from the design's class order and from
its "new buffer" rule for three classes, and decision 4 scales one pool
ahead of LP64-005; the design records all four.

1. **Task order follows the boot, not the design's list.** Measured, the
   boot needs animation tables, text banks, audio banks and the music
   table before the title's stage load, then textures, fonts and models
   before any mission, then backgrounds, collision tiles and setup files.
   *Recommended:* that order, so that every task ends with the real
   executable stopping one class later. The alternative, the design's
   order, leaves the first three converters unrun until the ninth.
2. **Three classes keep the file's bytes instead of getting a new
   buffer.** The design says a converter "writes the runtime layout into a
   new buffer". Measured: animation headers are addressed by constant
   offset at 83 sites and cannot move, but fit in place at 64 bits when
   packed; a text bank has no run-time struct, and its one reader can add
   the base as the N64 code does; collision tiles hold no pointer. The
   other seven classes get new memory as designed. *Recommended:* as
   planned: it costs no memory, and no strict-tier file changes for the
   animation class. The alternative for animations is to keep the file
   header and resolve its three offsets in `model.c`'s two readers, which
   moves the first task of the milestone behind a laptop trip; for text, a
   pointer table allocated beside each bank (8 bytes a slot, and three
   loaders to change).
3. **What "done" means.** *Recommended:* on `main`, all ten classes marked
   converted; `lp64_boot.py --expect-run --frames 600` passes for the
   title, plain and under the sanitizers, in the aarch64 and the x86_64
   container; the 20-mission smoke passes 20 of 20 on the plain 64-bit
   executable in both containers and on the sanitizer executable in the
   aarch64 one, with no `ge: lp64:` line; the oracle reports no difference
   for any file of any class; the load census is equal at both widths; the
   32-bit smoke is 20 of 20 and the Windows executable is identical to
   f73382a's or every changed function is explained; CI is green and the
   gallery fresh. A sanitizer report that the 32-bit sanitizer build gives
   too, at the same place, is an inherited defect: fixed here when the fix
   is a few lines, otherwise recorded for LP64-005 with your agreement, and
   the smoke run with that one report suppressed by name. The alternative,
   a plain smoke only, leaves grown structures unchecked until LP64-006.
4. **The graphics buffer doubles at 64 bits now.** Your LP64-003 decision
   put pool sizes in LP64-005 and LP64-006. This pool is different in
   kind: its unit is the command, which doubles, so the same bytes hold
   half the frame, and nothing checks its end. Measured, a headless smoke
   peaks at 36 KB of at least 123 KB, so the smoke would fit without it;
   real play is not measured. *Recommended:* multiply it by
   `sizeof(Gfx) / 8` at 64 bits (at most 520 KB more of a stage bank with
   63 MB free), and make an overrun fatal with the pool's name. Room
   memory, whose lists are at most a sixth of its bytes, is not scaled; it
   reports when a room does not fit.
5. **Fixed model buffers.** Three weapon and hand loads have fixed sizes
   that the largest converted files fit by about 5 percent, padding not
   counted. *Recommended:* at 64 bits scale the three sizes and the buffer
   they sit in by 13/8 (the worst file grows 1.55 times), and refuse by
   name what still does not fit. The alternative, leaving them and relying
   on the refusal, risks a missing trigger hand on the first run.
6. **The bg tables are built fresh at each stage load at 64 bits.** At 32
   bits they live in the ROM image, and whatever play wrote into them
   (portal control bytes, a swapped room pair, a room position) is still
   there when the stage loads again; on the N64 they were read fresh.
   *Recommended:* fresh, as on the N64, and Task 12 measures whether the
   32-bit game's second load actually differs from its first. If it does,
   that is a 32-bit question for LP64-006, where the two builds are
   compared. The alternative, keeping a converted copy for each bg file
   for the life of the process to imitate the 32-bit build, preserves
   something that may be a defect.
7. **The eleven cut stages.** Eight bg files are zero-length on the cart
   and three more alias one of them; a 32-bit build loads a header of
   zeros and misbehaves "as it would on hardware". At 64 bits the guard
   stops such a stage by name. *Recommended:* leave that for this
   milestone and settle it in LP64-005, which is the milestone that "loads
   every stage" and has to say what every means.
8. **The oracle is a build option, not a mode of the shipped game.**
   Verification item 4 runs the converters on a 32-bit build. Linking them
   into the shipped 32-bit executable would change it. *Recommended:*
   `-DGE_LP64_ORACLE=ON`, headless and 32-bit only, never shipped; CI does
   not build it (it needs the ROM), and the ROM-free fixtures run in CI at
   both widths. The alternative, a separate test program for each class,
   cannot reach the game's own in-place code for models and setups.
9. **One laptop trip, at the end, even if the strict branch is empty.**
   The milestone's commits to `src/`, `port/` and `include/` leave the
   gallery pending; the trip clears it and lands whatever strict-tier
   fixes the first runs needed. *Recommended:* as planned. If the model
   task needs a strict-tier fix before the title can run, `main` keeps
   stopping at `models` until the trip, and the branch carries the
   progress; say if you would rather make a first trip at that point.
10. **`Acmd` is widened here but cannot run here.** The design puts it
    with the audio class. A headless build has no mixer, so its only check
    in this milestone is the interpreter's ROM-free test at both widths.
    *Recommended:* do it here as designed, and record the mixer as
    unexecuted in the hand-over. The alternative is to move it to
    LP64-005, beside the first video run.
