#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-start-test.XXXXXX)
probe_log=$(mktemp /tmp/midas-start-probe.XXXXXX)
trap 'rm -f "$test_bin" "$probe_log"' EXIT
sdl_config=${SDL2_CONFIG:-sdl2-config}

# The driver includes video.c, which needs the real SDL2 headers and library
# at the width it builds; an x86_64 host with only the amd64 SDL2 package (CI's
# runner, the x86_64 container) has no i386 SDL2/_real_SDL_config.h, so that
# half is skipped by name there. The i686 video image runs it with
# SDL2_CONFIG=/home/dev/sdl2-linux-i686/bin/sdl2-config. True when SDL.h
# compiles at WIDTH; otherwise prints the compiler's first error.
sdl_available() {
    local width=$1
    if printf '#include <SDL.h>\n' | "${CC:-gcc}" $(harness_flags "$width") $("$sdl_config" --cflags) \
        -x c -fsyntax-only - >"$probe_log" 2>&1; then
        return 0
    fi
    grep -m1 'error' "$probe_log" || head -n 1 "$probe_log"
    return 1
}

widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_start_input "$width"; then
        echo "test_start_input: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    # Only the 32-bit half may be skipped; without SDL2 at 64 bits the compile
    # below fails as before.
    if [[ $width == 32 ]] && ! reason=$(sdl_available "$width"); then
        echo "test_start_input: 32-bit half skipped: no i386 SDL2 headers ($reason)"
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
