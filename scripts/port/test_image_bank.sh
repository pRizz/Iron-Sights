#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-image-bank-test.XXXXXX.cjs)
trap 'rm -f "$test_bin"' EXIT
compiler=${CC:-gcc}
flags=(-m32)
runner=()
if [[ ${1:-} == --web ]]; then
    compiler=emcc
    flags=(-sGLOBAL_BASE=268435456 -sINITIAL_MEMORY=285212672)
    runner=(node)
fi
"$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char \
    -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
    -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
    scripts/port/test_image_bank.c assets/oddtextures.c -o "$test_bin"
"${runner[@]}" "$test_bin"
printf 'Image bank reset regression passed (%s)\n' "$compiler"
