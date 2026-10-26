#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-start-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
sdl_config=${SDL2_CONFIG:-sdl2-config}
"${CC:-gcc}" -m32 -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -w \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
    -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
    $("$sdl_config" --cflags) scripts/port/test_start_input.c \
    $("$sdl_config" --libs) -lm -o "$test_bin"
"$test_bin"
