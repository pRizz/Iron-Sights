#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-stan-callbacks.XXXXXX.cjs)
trap 'rm -f "$test_bin"' EXIT
compiler=${CC:-gcc}
flags=(-m32)
runner=()
if [[ ${1:-} == --web ]]; then
    compiler=emcc
    flags=(-sGLOBAL_BASE=268435456 -sINITIAL_MEMORY=285212672
        -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types)
    runner=(node)
fi
"$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char \
    -fno-strict-aliasing -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
    -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
    -include port/web/compat.h scripts/port/test_stan_callbacks.c -lm -o "$test_bin"
"${runner[@]}" "$test_bin"
printf 'Collision callback regression passed (%s)\n' "$compiler"
