#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-hd-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
"${CC:-gcc}" -c port/fast3d/third_party/puff/puff.c -o "$work/puff.o"
"${CXX:-g++}" -std=c++20 -DGE_VIDEO=1 -Wall -Wextra -Werror -I. -Iport/include \
    scripts/port/test_hd_textures.cpp port/fast3d/hd_texture_format.cpp \
    port/fast3d/hd_textures.cpp "$work/puff.o" -o "$work/test"
"$work/test" "$work/data"
if [ "$#" -gt 0 ]; then "$work/test" --pack "$1"; fi
if [ "${MIDAS_HD_TEST_I686:-0}" = 1 ]; then
    # Exercise the shipping i686 ABI, including modern filesystem metadata.
    "${CC:-gcc}" -m32 -c port/fast3d/third_party/puff/puff.c -o "$work/puff32.o"
    "${CXX:-g++}" -m32 -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c++20 -DGE_VIDEO=1 \
        -Wall -Wextra -Werror -I. -Iport/include scripts/port/test_hd_textures.cpp \
        port/fast3d/hd_texture_format.cpp port/fast3d/hd_textures.cpp "$work/puff32.o" -o "$work/test32"
    "$work/test32" "$work/data32"
    if [ "$#" -gt 0 ]; then "$work/test32" --pack "$1"; fi
fi
