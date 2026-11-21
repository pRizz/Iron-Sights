#!/usr/bin/env bash
# The ROM-free tests of LP64-006's comparison harness, at each pointer width
# the host can build: lockstep (test_lockstep.c, Task 2) includes
# libultra_shim.c and port_lockstep.c and links the real pump, pcsched.c,
# built without GE_VIDEO as a headless build is.
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
done
echo "test_lp64_compare: PASS"
