#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-start-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
sdl_config=${SDL2_CONFIG:-sdl2-config}
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_start_input "$width"; then
        echo "test_start_input: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -w \
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        $("$sdl_config" --cflags) scripts/port/test_start_input.c \
        $("$sdl_config" --libs) -lm -o "$test_bin"
    "$test_bin"
    echo "test_start_input: PASS (${width}-bit)"
done
