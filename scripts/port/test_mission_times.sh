#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-mission-times.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
for variant in runtime compiletime; do
    extra=()
    if [[ "$variant" == compiletime ]]; then extra=(-DGE_UNLOCK_ALL); fi
    "${CC:-gcc}" -m32 -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -msse2 -mfpmath=sse -Werror=implicit-function-declaration \
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C "${extra[@]}" \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_mission_times.c -lm -o "$test_bin"
    "$test_bin"
done
