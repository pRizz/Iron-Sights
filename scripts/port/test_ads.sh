#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-ads-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_ads "$width"; then
        echo "test_ads: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width") -std=gnu11 -fms-extensions -fcommon \
        -fno-builtin -Werror=implicit-function-declaration $(harness_quiet_flags) $(harness_gc_flags) \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_ads.c -lm -o "$test_bin" \
        $(harness_weak_undefined portVideoClassicSavedGet portVideoClassicSavedSet)
    "$test_bin"
    echo "test_ads: PASS (${width}-bit)"
done
bash scripts/port/test_dyn.sh
