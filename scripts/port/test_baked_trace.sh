#!/usr/bin/env bash
# ROM-free test of Baked Lighting's ray tracer (FORK-082) on a known scene.
# The SDL shim stands in for SDL (no threads: the time-sliced path runs).
# Runs at both pointer widths: struct BakeLevel holds pointers.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
test_bin=$(mktemp /tmp/midas-bake-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_baked_trace "$width"; then
        echo "test_baked_trace: ${width}-bit half pending"
        continue
    fi
    "${CC:-gcc}" $(harness_flags "$width" -msse2 -mfpmath=sse) -std=gnu11 -Wall -Wextra -Werror \
        -Iscripts/port/sdl_shim -Iport/src scripts/port/test_baked_trace.c -lm -o "$test_bin"
    "$test_bin"
    echo "test_baked_trace: PASS (${width}-bit)"
done
