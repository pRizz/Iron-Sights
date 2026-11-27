#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-stan-callbacks.XXXXXX.cjs)
trap 'rm -f "$test_bin"' EXIT
compiler=${CC:-gcc}
runner=()
if [[ ${1:-} == --web ]]; then
    compiler=emcc
    widths=web
    web_flags=(-sGLOBAL_BASE=268435456 -sINITIAL_MEMORY=285212672
        -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types)
    runner=(node)
else
    widths=$(harness_widths)
fi
for width in $widths; do
    if harness_pending test_stan_callbacks "$width"; then
        echo "test_stan_callbacks: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    if [[ $width == web ]]; then flags=("${web_flags[@]}"); else flags=($(harness_flags "$width")); fi
    "$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char \
        -fno-strict-aliasing $(harness_gc_flags) \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        -include port/web/compat.h scripts/port/test_stan_callbacks.c -lm -o "$test_bin"
    "${runner[@]}" "$test_bin"
    if [[ $width == web ]]; then
        printf 'Collision callback regression passed (%s)\n' "$compiler"
    else
        echo "test_stan_callbacks: PASS (${width}-bit)"
    fi
done
