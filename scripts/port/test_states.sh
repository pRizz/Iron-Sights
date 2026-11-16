#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-state-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_states "$width"; then
        echo "test_states: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    for test_source in scripts/port/test_states.c scripts/port/test_state_input.c; do
        "${CC:-gcc}" $(harness_flags "$width" -msse2 -mfpmath=sse) -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
            -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
            -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
            "$test_source" -lm -o "$test_bin"
        "$test_bin"
    done
    echo "test_states: PASS (${width}-bit)"
done
python3 scripts/port/test_state_layout.py
