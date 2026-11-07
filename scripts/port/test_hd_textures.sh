#!/usr/bin/env bash
# ROM-free tests of HD texture packs (FORK-053): the pack index and the
# download engine. Run from any
# directory; the goldeneye Docker image and CI's Ubuntu runner have the tools.
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-hd-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
python3 scripts/port/make_hd_texture_fixture.py "$work/pack"
"${CC:-gcc}" -c port/fast3d/third_party/puff/puff.c -o "$work/puff.o"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I. -Iport/fast3d \
    scripts/port/test_hd_textures.cpp port/fast3d/hd_texture_pack.cpp port/fast3d/hd_texture_format.cpp \
    port/fast3d/png_decode.cpp "$work/puff.o" -o "$work/test"
"$work/test" "$work/pack"
# The download engine with a fake transport (no network).
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I. -Iport/fast3d \
    scripts/port/test_hd_download.cpp port/fast3d/hd_download_engine.cpp -o "$work/test_download"
"$work/test_download" "$work/download"
