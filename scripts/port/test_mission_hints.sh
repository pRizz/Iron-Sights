#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_bin=$(mktemp /tmp/midas-mission-hints.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
"${CC:-gcc}" -m32 -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -msse2 -mfpmath=sse -Werror=implicit-function-declaration \
    -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DGE_VIDEO \
    -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
    scripts/port/test_mission_hints.c -o "$test_bin"
"$test_bin"
