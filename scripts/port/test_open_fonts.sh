#!/usr/bin/env sh
# ROM-free test of the open-font glyph path (FORK-050). Run from the repo root;
# the goldeneye Docker image has the compilers.
set -eu
out="${TMPDIR:-/tmp}/open_fonts_test"
mkdir -p "$out"
cmake -DOUT="$out/open_fonts_data.c" -P scripts/port/test_open_fonts_data.cmake
cc -std=c99 -O1 -c port/fast3d/third_party/puff/puff.c -o "$out/puff.o"
cc -std=c99 -O1 -c "$out/open_fonts_data.c" -o "$out/data.o"
c++ -std=c++20 -O1 -Wall -Wextra -Wno-maybe-uninitialized -Iport/fast3d port/fast3d/open_fonts.cpp scripts/port/test_open_fonts.cpp \
    "$out/puff.o" "$out/data.o" -o "$out/test_open_fonts"
"$out/test_open_fonts"
