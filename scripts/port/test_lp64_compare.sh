#!/usr/bin/env bash
# The ROM-free tests of LP64-006's comparison harness, at each pointer width
# the host can build: lockstep (test_lockstep.c, Task 2) includes
# libultra_shim.c and port_lockstep.c and links the real pump, pcsched.c,
# built without GE_VIDEO as a headless build is; the simulation trace
# (test_simtrace.c, Task 3) includes port_simtrace.c with fabricated game state;
# the sound lock (test_audio_lock.c, Task 9) links port_audio_lock.c, and is
# built once more without GE_COMPARE to check the shipped header's stand-in;
# the comparison build's tanf, atanf and cosf (test_compare_math.c, Task 4) link
# port_compare_math.c, as does the maths sweep (lp64_libm_sweep.c), which must
# print one hash for each of its six functions, with tanf, atanf and cosf's
# pinned (the same on every host). -fno-builtin keeps the compiler
# from folding a call; -ffp-contract=off is the comparison build's own flag
# for port_compare_math.c (CMakeLists.txt).
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-lp64-compare.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

# On its own line, so a failure of harness_widths stops the harness (set -e).
widths=$(harness_widths)
for width in $widths; do
    flags=($(harness_flags "$width" -msse2 -mfpmath=sse)
        -std=gnu11 -fms-extensions -fcommon -funsigned-char -fno-strict-aliasing -w
        -Werror=implicit-function-declaration
        -ffunction-sections -fdata-sections -Wl,--gc-sections
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include)
    "$compiler" "${flags[@]}" scripts/port/test_lockstep.c port/src/pcsched.c -lm -o "$work/lockstep-$width"
    "$work/lockstep-$width"
    "$compiler" "${flags[@]}" scripts/port/test_simtrace.c -lm -o "$work/simtrace-$width"
    "$work/simtrace-$width"
    "$compiler" "${flags[@]}" -DGE_COMPARE=1 scripts/port/test_audio_lock.c port/src/port_audio_lock.c \
        -o "$work/audio-lock-$width"
    "$work/audio-lock-$width"
    "$compiler" "${flags[@]}" scripts/port/test_audio_lock.c -o "$work/audio-lock-shipped-$width"
    "$work/audio-lock-shipped-$width"
    math_flags=("${flags[@]}" -fno-builtin -ffp-contract=off)
    "$compiler" "${math_flags[@]}" scripts/port/test_compare_math.c port/src/port_compare_math.c -lm \
        -o "$work/compare-math-$width"
    "$work/compare-math-$width"
    "$compiler" "${math_flags[@]}" scripts/port/lp64_libm_sweep.c port/src/port_compare_math.c -lm \
        -o "$work/sweep-$width"
    "$work/sweep-$width" >"$work/sweep-$width.txt"
    sweep_lines=$(awk '/^(sinf|cosf|tanf|atanf|expf|powf) [0-9a-f]+$/ && length($2) == 8 { n++ } END { print n + 0 }' \
        "$work/sweep-$width.txt")
    if [[ $sweep_lines != 6 ]]; then
        cat "$work/sweep-$width.txt" >&2
        echo "test_lp64_compare: FAIL ($width-bit): the sweep printed $sweep_lines of its six hashes" >&2
        exit 1
    fi
    # The in-tree functions are host-independent by design: pin their hashes
    # (FORK-089, Task 4's sweep). sinf, expf and powf are libm's and may differ.
    for pinned in tanf:e3ccc704 atanf:f160154d cosf:4c162bf1; do
        name=${pinned%%:*} want=${pinned#*:}
        seen=$(awk -v name="$name" '$1 == name { print $2 }' "$work/sweep-$width.txt")
        if [[ $seen != "$want" ]]; then
            echo "test_lp64_compare: FAIL ($width-bit): the sweep's $name hash is $seen, expected $want" >&2
            exit 1
        fi
    done
    echo "lp64_libm_sweep ($width-bit):" $(tr '\n' ' ' <"$work/sweep-$width.txt")
done
echo "test_lp64_compare: PASS"
