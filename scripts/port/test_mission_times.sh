#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-mission-times.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_mission_times "$width"; then
        echo "test_mission_times: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    for variant in runtime compiletime; do
        extra=()
        if [[ "$variant" == compiletime ]]; then extra=(-DGE_UNLOCK_ALL); fi
        "${CC:-gcc}" $(harness_flags "$width" -msse2 -mfpmath=sse) -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Werror=implicit-function-declaration \
            -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C "${extra[@]}" \
            -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
            scripts/port/test_mission_times.c -lm -o "$test_bin"
        "$test_bin"
    done
    echo "test_mission_times: PASS (${width}-bit)"
done
