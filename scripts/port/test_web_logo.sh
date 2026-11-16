#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-web-logo.XXXXXX.cjs)
trap 'rm -f "$test_bin"' EXIT
compiler=${CC:-gcc}
runner=()
if [[ ${1:-} == --web ]]; then
    compiler=emcc
    widths=web
    web_flags=()
    runner=(node)
else
    widths=$(harness_widths)
fi
for width in $widths; do
    if harness_pending test_web_logo "$width"; then
        echo "test_web_logo: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    if [[ $width == web ]]; then flags=("${web_flags[@]}"); else flags=($(harness_flags "$width")); fi
    "$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Iport/include \
        scripts/port/test_web_logo.c -o "$test_bin"
    "${runner[@]}" "$test_bin"
    if [[ $width != web ]]; then echo "test_web_logo: PASS (${width}-bit)"; fi
done
