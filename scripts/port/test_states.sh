#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-state-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
for test_source in scripts/port/test_states.c scripts/port/test_state_input.c; do
"${CC:-gcc}" -m32 -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -msse2 -mfpmath=sse -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
    -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
    "$test_source" -lm -o "$test_bin"
"$test_bin"
done
python3 scripts/port/test_state_layout.py
