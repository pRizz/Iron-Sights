#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
# The browser's compiler: a Mac without it says so by name and runs nothing;
# any other host without it fails below.
harness_skip_unless_tool test_web_touch emcc || exit 0
test_dir=$(mktemp -d /tmp/midas-touch-test.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
emcc -std=gnu11 -fms-extensions -fcommon -ffreestanding -funsigned-char -Wno-microsoft-anon-tag \
    -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C \
    -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
    -include port/web/compat.h scripts/port/test_web_touch.c \
    -sENVIRONMENT=node -sEXIT_RUNTIME=1 -o "$test_dir/test.js"
node "$test_dir/test.js"
