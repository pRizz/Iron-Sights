#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-wardrobe-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_wardrobe "$width"; then
        echo "test_wardrobe: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -fms-extensions -fcommon \
        -fno-builtin -Werror=implicit-function-declaration $(harness_quiet_flags) $(harness_gc_flags) \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_wardrobe.c src/game/model.c src/game/math_atan2f.c -lm -o "$test_bin"
    "$test_bin"
    echo "test_wardrobe: PASS (${width}-bit)"
done
