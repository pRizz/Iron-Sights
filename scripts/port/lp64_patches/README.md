# LP64-006 patches

Source patches that LP64-006's records cite: corrections written for the
owner and not applied, and the scratch changes whose runs justify an
`explained:` or `host:` line of `scripts/port/lp64_equiv_expected.txt`. They
were kept under the git-ignored `build/lp64/equiv/` until the final review
(2026-11-23, finding I3) moved them here, so the evidence outlives the
milestone's run folders. None holds ROM data. LP64-007 Task 12 applied the
owner's yes answers (decision 12, 2026-11-24) to the tree in one commit:
questions 2, 4 and 7 and question 6 except `t15-fabsf.patch`. The "Applied"
column says how; every other patch here is not applied. An applied patch is
kept as the record of what was asked; it no longer applies to the tree.

"Applies to" names the commit a patch applies to cleanly. `git apply` or
`patch -p1` from the repository root unless another level is given.

## The owner's questions

These are the numbered questions of FORK-089's "LP64-006 hand-over to
LP64-007" block.

| Patch | What it changes | Applies to | Question | Applied |
| --- | --- | --- | --- | --- |
| `portal-end-32bit.patch` | `src/game/bg.c`: the special-portal walk stops at the array's own end at 32 bits too (at 64 bits it already does) | 29e0ab5 and later, including the final review's commit | 1, Jungle's special-portal walk | no, unanswered |
| `t8-zero-stack-buffers.patch` | `src/game/image.c`: zeroes the scratch buffer at 32 bits too, as 8228116 does at 64 bits (`patch -p0`) | 29e0ab5 to bf53ca1 | 2, `image.c`'s scratch buffer | Task 12, by hand: the 64-bit guard became `#ifndef TARGET_N64`, so every PC build zeroes the buffer and the N64 target keeps the original (the patch's `#if 1` scratch line was not committed) |
| `save-padding-32bit.patch` | `src/game/file2.c`: `fileWriteSave` zeroes the record's padding at both widths, not only at 64 bits | the final review's commit | 4, save-record padding | Task 12, as written |
| `t14-env-ratel.patch` | `src/libultrare/audio/env.c`: a fix confined to aarch64 for the envelope's `ratel` conversion; never compiled | bf53ca1 and the final review's commit | 5, Arm only | no (the owner's answer for now) |
| `t15-atan2f.patch` | `port/src/port_projectile_arc_audit.c`: the `atan2f` prototype, unguarded | 8dcdb00 (f60ed4f applied it at 64 bits only) | 6, Task 15's prototypes | Task 12, by hand: f60ed4f's `#if IS_64_BIT` guard and its `platform_info.h` include removed, `game/math_atan2f.h` included unguarded |
| `t15-fabsf.patch` | `src/game/bondview2.c` (strict tier): the `fabsf` prototype | bf53ca1 and later | 6 | no (strict tier) |
| `t15-strlen.patch` | `src/game/options.c`, `src/token.c`: `<string.h>`, unguarded | 8dcdb00 (f60ed4f applied it at 64 bits only) | 6 | Task 12, by hand: f60ed4f's `#if IS_64_BIT` guard and its `platform_info.h` include removed from both files |
| `t15-tanf.patch` | `src/game/sky.c`: the `tanf` prototype, unguarded | bf53ca1 and later | 6 | Task 12, by hand: f60ed4f's guarded file-scope prototype unguarded (the patch's block-scope one would have been a second declaration) |
| `t16-op17-32bit.patch` | `romfile_model.c`, `romconv_model_records.c` and their tests: OP17's own map at both widths | the final review's commit | 7, Task 16's fixes | Task 12, as written |
| `t16-chraction-32bit.patch` | `src/game/chraction.c` and its test: the empty-hand guard at both widths | the final review's commit | 7 | Task 12, as written, and the now unused `platform_info.h` include of `chraction.c` removed |
| `t16-index0-32bit.patch` | `src/game/ob.c` and `test_lp64_typed_ob.c`: the index-0 payload bound at both widths | the final review's commit (regenerated for its `ob.c`) | 7 | Task 12, as written (`ob.c` still has `IS_64_BIT` through `ultra64.h`) |
| `synth-raise-64bit.patch` | `src/music.c`: `MUSIC_SYN_CONFIG_MAX_UPDATES` back to 0x124 at 64 bits (Task 11's raise, withheld at the final review) | the final review's commit | 8, the synthesizer pool | no, unanswered |

## Evidence for a recorded line or finding

| Patch | What it changes | Applies to | Evidence for |
| --- | --- | --- | --- |
| `libm-cosf-scratch.patch` | `port/src/port_compare_math.c`: renames the in-tree `cosf` so the link takes glibc's | 29e0ab5 and later | Task 6's search for the libm function to blame (FORK-089) |
| `libm-cosf-atanf-scratch.patch` | the same for `cosf` and `atanf`, leaving the in-tree `tanf` | 29e0ab5 and later | the `host: libm tanf alone` lines; the `*-tanf` `removed-by:` runs |
| `t8-rounds-table-index.patch` | `port/include/port_explosive_rounds.h`: the three numbers on their rows (`patch -p0`) | a3dd635 (d1170ef landed the fix) | Task 8's cause C |
| `t8-dbg-compfill.patch`, `t8-dbg-randcaller.patch`, `t8-dbg-tex.patch`, `t8-dbg-uploads.patch` | debugging logs in `image.c`, `n64_random.c`, `gfx_pc.cpp` (strict tier) and `port_gfx_digest.cpp` (`patch -p0`) | bf53ca1 | Task 8's search for cause A; never meant to land |
| `t15-exp1/*.diff` | one prototype correction a file: `mpmenu.c`, `options.c`, `port_hitmap_draw.c`, `port_projectile_arc_audit.c`, `sky.c`, `token.c` | `mpmenu`, `port_hitmap_draw`, `sky`: bf53ca1; the other three: 8dcdb00 | Task 15: corrections that changed a 32-bit object, so not applied |
| `t15-exp2-all.diff`, `t15-exp2-bondview2.diff` | the strict-tier and `bondview.c` prototypes (`bondview.c`, `bondview2.c`, `gunfire.c`) | bf53ca1 | the same, for the strict-tier set |

## Reproducing the `removed-by:` runs

Each `explained:` line's `removed-by:` names a scratch 32-bit comparison
video build: an export of a commit with one patch on it. With the ROM, from
the repository root on the Mac (the containers mount it at
`/home/dev/project`, the ROM at `/rom/baserom.u.z64`):

```sh
SRC=build/lp64/equiv/t8b-portal-src TREE=build/lp64/t8b-portal-i686-video
mkdir -p "$SRC" && git archive 29e0ab5 | tar -x -C "$SRC"
patch -d "$SRC" -p1 < scripts/port/lp64_patches/portal-end-32bit.patch
python3 scripts/port/lp64_env.py run --arch i686 --video -- sh -c "cmake -S $SRC -B $TREE \
  -DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DGE_COMPARE=ON -DSDL2_LINUX_ROOT=/home/dev/sdl2-linux-i686 \
  -DCMAKE_CXX_FLAGS=-D_scheduler_h_ && cmake --build $TREE -j\"\$(nproc)\""
python3 scripts/port/lp64_env.py run --arch i686 --video -- python3 scripts/port/lp64_equiv.py run \
  --rom /rom/baserom.u.z64 --set solo --only stage-jungle,stage-surface --repeat 2 --video \
  --timeout-scale 4 --output build/lp64/equiv/t8b-portal-default $TREE/IronSights
python3 scripts/port/lp64_equiv.py compare --streams sim,gfx build/lp64/equiv/t8b-portal-default \
  build/lp64/equiv/RUN/x64 build/lp64/equiv/RUN/a64
```

`RUN/x64` and `RUN/a64` are 64-bit runs of the same cases from the same
commit (a step-9 run of `cmp-x64-video` and `cmp-a64-video`); the line's
difference must be gone, its stream identical. The runs, as each source
tree's files show (checked at the final review against the commits' trees):

| `removed-by:` run | Commit | Patch | Profile and cases |
| --- | --- | --- | --- |
| `t8b-portal-default`, `t8b-portal-all` | 29e0ab5 | `portal-end-32bit.patch` | default: stage-jungle, stage-surface; `--profile all`: also stage-aztec |
| `t8b-zero32-default`, `t8b-zero32-all` | 29e0ab5 | `t8-zero-stack-buffers.patch` (`patch -p0`) | as the line above |
| `t8b-portal-tanf` | 29e0ab5 | `portal-end-32bit.patch` and `libm-cosf-atanf-scratch.patch` | default: stage-jungle, against the x86_64 build `t8b-tanfonly-x64-video` (29e0ab5 with `libm-cosf-atanf-scratch.patch`) |
| `t8b-zero32-tanf` | 29e0ab5 | `t8-zero-stack-buffers.patch` and `libm-cosf-atanf-scratch.patch` | default: stage-aztec, stage-surface, against the same x86_64 build |
| `t9-portal-default`, `t9-portal-all` | 0878cf4 | `portal-end-32bit.patch` | stage-jungle under each profile, compared with `--streams sim,gfx,snd` (the sound lock is in 0878cf4) |

The headless libm line's run (`sim-libm/portal-end-tanf-only-i686`, Task 6)
is a headless 32-bit build (`-DGE_COMPARE=ON -DGE_COMPARE_LIBM=ON`, no
`--video`) with `portal-end-32bit.patch` and the in-tree `tanf`. The libm
lines were last exercised in Tasks 6 and 8 and are not rerun.
