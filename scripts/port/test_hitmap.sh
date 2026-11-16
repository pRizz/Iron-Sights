#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-hitmap.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_hitmap "$width"; then
        echo "test_hitmap: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width" -msse2 -mfpmath=sse) -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char \
        -Werror=implicit-function-declaration \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_hitmap.c -lm -o "$test_bin"
    "$test_bin"
    echo "test_hitmap: PASS (${width}-bit)"
done
