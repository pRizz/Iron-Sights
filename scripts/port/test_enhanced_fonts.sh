#!/usr/bin/env sh
# ROM-free test of the Enhanced fonts (FORK-052). Run from the repo root; the
# goldeneye Docker image has the compilers. Uses no pack files.
set -eu
out="${TMPDIR:-/tmp}/enhanced_fonts_test"
mkdir -p "$out"
cc -std=c99 -O1 -c port/fast3d/third_party/puff/puff.c -o "$out/puff.o"
c++ -std=c++20 -O1 -Wall -Wextra -Iport/fast3d port/fast3d/enhanced_fonts.cpp port/fast3d/enhanced_font_masks.cpp \
    port/fast3d/png_decode.cpp scripts/port/test_enhanced_fonts.cpp "$out/puff.o" -o "$out/test_enhanced_fonts"
"$out/test_enhanced_fonts"
# The download engine (FONT-3) with a fake transport; no network.
c++ -std=c++20 -O1 -Wall -Wextra -Iport/fast3d port/fast3d/enhanced_fonts_download.cpp port/fast3d/enhanced_fonts_http.cpp \
    port/fast3d/enhanced_fonts.cpp port/fast3d/enhanced_font_masks.cpp port/fast3d/png_decode.cpp \
    scripts/port/test_enhanced_fonts_download.cpp "$out/puff.o" -ldl -pthread -o "$out/test_enhanced_fonts_download"
"$out/test_enhanced_fonts_download"
