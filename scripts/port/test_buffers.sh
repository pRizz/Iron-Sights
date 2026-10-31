#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-buffer-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
flags=(-m32 -g -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections)
if [[ ${MIDAS_BUFFER_SANITIZE:-0} == 1 ]]; then
    flags+=(-fsanitize=address,undefined -fno-sanitize=alignment,shift,signed-integer-overflow -fno-sanitize-recover=all)
fi
for source in test_model_bounds test_audio_buffers test_memory_banks test_token_bounds test_high_alloc; do
    "${CC:-gcc}" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -w -fno-builtin -fwrapv -funsigned-char \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DLEFTOVERDEBUG \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        "scripts/port/$source.c" -lm -o "$work/$source"
    "$work/$source"
done
"${CXX:-g++}" "${flags[@]}" -std=c++20 -Wall -Wextra -Werror -I. \
    scripts/port/test_gfx_buffers.cpp -o "$work/gfx"
"$work/gfx"
python3 scripts/port/test_audit_buffers.py
