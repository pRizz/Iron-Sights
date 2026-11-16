#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-image-bank-test.XXXXXX.cjs)
trap 'rm -f "$test_bin"' EXIT
compiler=${CC:-gcc}
runner=()
if [[ ${1:-} == --web ]]; then
    compiler=emcc
    widths=web
    web_flags=(-sGLOBAL_BASE=268435456 -sINITIAL_MEMORY=285212672)
    runner=(node)
else
    widths=$(harness_widths)
fi
for width in $widths; do
    if harness_pending test_image_bank "$width"; then
        echo "test_image_bank: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    if [[ $width == web ]]; then flags=("${web_flags[@]}"); else flags=($(harness_flags "$width")); fi
    "$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_image_bank.c assets/oddtextures.c -o "$test_bin"
    "${runner[@]}" "$test_bin"
    if [[ $width == web ]]; then
        printf 'Image bank reset regression passed (%s)\n' "$compiler"
    else
        echo "test_image_bank: PASS (${width}-bit)"
    fi
done
