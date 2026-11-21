#!/usr/bin/env bash
# A headless build replays recorded pads (LP64-005 Task 8): a ROM-free program
# that includes libultra_shim.c's pad read and inputrec.c, built without
# GE_VIDEO at each pointer width, writes a route of one START press at tick 3
# and reads pad 0 five times with GE_INPUTPLAY set, then with it unset. A
# second program links port_playlog.c with a fabricated player 0 and checks
# the GE_PLAYLOG=2 gun line, GE_PLAYSTALL and GE_PLAYHURT (LP64-006 Task 1).
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-inputrec-headless.XXXXXX)
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
    "$compiler" "${flags[@]}" scripts/port/test_inputrec_headless.c -lm -o "$work/inputrec-$width"
    "$work/inputrec-$width"
    "$compiler" "${flags[@]}" scripts/port/test_playlog_gun.c port/src/port_playlog.c -lm \
        -o "$work/playlog-gun-$width"
    "$work/playlog-gun-$width"
done
echo "test_inputrec_headless: PASS"
