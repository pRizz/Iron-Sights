#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-hud-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_hud "$width"; then
        echo "test_hud: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width" -msse2 -mfpmath=sse) -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_hud.c -lm -o "$test_bin"
    "$test_bin"
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_hud_weapons.c -o "$test_bin"
    "$test_bin"
    echo "test_hud: PASS (${width}-bit)"
done
"${CC:-gcc}" -std=gnu11 -Werror=implicit-function-declaration -I. scripts/port/test_hud_clearance.c -o "$test_bin"
"$test_bin"
