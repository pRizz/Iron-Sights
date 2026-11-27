#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-buffer-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_buffers "$width"; then
        echo "test_buffers: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    flags=($(harness_flags "$width") -g -O1 $(harness_gc_flags))
    if [[ ${MIDAS_BUFFER_SANITIZE:-0} == 1 ]]; then
        flags+=(-fsanitize=address,undefined -fno-sanitize=alignment,shift,signed-integer-overflow -fno-sanitize-recover=all)
    fi
    sources=(test_model_bounds test_audio_buffers test_memory_banks test_token_bounds test_high_alloc test_indy_file_name
        test_gdl_store)
    if [[ ${MIDAS_BUFFER_SANITIZE:-0} == 1 ]]; then
        sources+=(test_lp64_poison) # the arenas' poisoning exists only under the sanitizer
    fi
    for source in "${sources[@]}"; do
        "${CC:-gcc}" "${flags[@]}" -std=gnu11 -fms-extensions -fcommon -w -fno-builtin -fwrapv -funsigned-char \
            -DVERSION_US -DLANG_US -DREFRESH_NTSC -D_LANGUAGE_C -DLEFTOVERDEBUG \
            -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
            "scripts/port/$source.c" -lm -o "$work/$source"
        "$work/$source"
    done
    "${CXX:-g++}" "${flags[@]}" -std=c++20 -Wall -Wextra -Werror -I. \
        scripts/port/test_gfx_buffers.cpp -o "$work/gfx"
    "$work/gfx"
    echo "test_buffers: PASS (${width}-bit)"
done
python3 scripts/port/test_audit_buffers.py
