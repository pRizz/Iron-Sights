#!/usr/bin/env bash
# ROM-free regression (LP64-006 Task 8): a 64-bit build's non-zlib texture
# inflater reads zero, not stack leftovers, past an odd-width 4-bit image.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-image-inflate-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
compiler=${CC:-gcc}
widths=$(harness_widths)
for width in $widths; do
    flags=($(harness_flags "$width"))
    "$compiler" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -funsigned-char -w -O0 \
        -fno-stack-protector -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DNONMATCHING \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_image_inflate.c src/game/image.c src/game/image_bank.c assets/oddtextures.c -o "$test_bin"
    "$test_bin"
    echo "test_image_inflate: PASS (${width}-bit)"
done
